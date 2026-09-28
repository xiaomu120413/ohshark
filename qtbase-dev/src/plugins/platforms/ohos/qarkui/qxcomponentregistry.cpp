// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <hilog/log.h>

#include <qarkui/qxcomponentregistry.h>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <QtCore/private/qohoslogger_p.h>
#include <map>
#include <mutex>
#include <qohosutils.h>
#include <string>

QT_BEGIN_NAMESPACE

namespace QArkUi {

// Global storage for the parent XComponent's native window surface.
// The parent XComponent (ArkTS-created) supports surface allocation.
// We register the surface callback early (during Init, before the
// ArkTS page is rendered) so OnSurfaceCreated fires when the surface
// is allocated. The stored window is retrieved later by QNativeNode.
static OHNativeWindow *g_parentSurface = nullptr;
static std::mutex g_parentSurfaceMutex;

// Per-window surfaces captured from the ArkTS SURFACE-type XComponent
// ("<id>_surf") embedded in subwindow / floating-window pages. On this
// OHOS build the C-API child XComponent never allocates a surface and
// OH_ArkUI_SurfaceHolder_Create fails, so a subwindow's own render
// target is a dedicated ArkTS XComponent, mirroring the main window.
static std::map<std::string, OHNativeWindow *> g_capturedSurfaces;
static std::mutex g_capturedSurfacesMutex;

static constexpr char kSurfIdSuffix[] = "_surf";

OHNativeWindow *parentSurfaceOrNull()
{
    std::lock_guard<std::mutex> lk(g_parentSurfaceMutex);
    return g_parentSurface;
}

OHNativeWindow *capturedSurfaceOrNull(const std::string &surfXComponentId)
{
    std::lock_guard<std::mutex> lk(g_capturedSurfacesMutex);
    const auto it = g_capturedSurfaces.find(surfXComponentId);
    return it != g_capturedSurfaces.end() ? it->second : nullptr;
}

static void storeCapturedSurface(const std::string &id, OHNativeWindow *window)
{
    std::lock_guard<std::mutex> lk(g_capturedSurfacesMutex);
    if (window != nullptr)
        g_capturedSurfaces[id] = window;
    else
        g_capturedSurfaces.erase(id);
}

static std::string xComponentIdOf(::OH_NativeXComponent *xc)
{
    char idBuf[128];
    uint64_t idLen = sizeof(idBuf) - 1;
    if (::OH_NativeXComponent_GetXComponentId(xc, idBuf, &idLen) == 0) {
        idBuf[idLen] = '\0';
        return std::string(idBuf);
    }
    return std::string();
}

// Registers surface callbacks on the ArkTS SURFACE-type XComponent that
// acts as a subwindow's render target; its surface is delivered
// asynchronously once the window is shown.
static void registerCapturedSurfaceCallback(OH_NativeXComponent *xComponent, const std::string &id)
{
    static OH_NativeXComponent_Callback surfaceCb = {
        .OnSurfaceCreated = [](::OH_NativeXComponent *xc, void *win) {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "SURFCAP onSurfaceCreated: xc=%{public}p win=%{public}p", xc, win);
            storeCapturedSurface(xComponentIdOf(xc), reinterpret_cast<OHNativeWindow *>(win));
        },
        .OnSurfaceChanged = [](::OH_NativeXComponent *xc, void *win) {
            if (win)
                storeCapturedSurface(xComponentIdOf(xc), reinterpret_cast<OHNativeWindow *>(win));
        },
        .OnSurfaceDestroyed = [](::OH_NativeXComponent *xc, void *) {
            storeCapturedSurface(xComponentIdOf(xc), nullptr);
        },
        .DispatchTouchEvent = [](::OH_NativeXComponent *, void *) {
            // Input is handled by the NODE-type sibling XComponent.
        },
    };

    auto rc = ::OH_NativeXComponent_RegisterCallback(xComponent, &surfaceCb);
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "SURFCAP RegisterCallback: rc=%{public}d xc=%{public}p id=%{public}s",
        rc, xComponent, id.c_str());
}

void registerParentSurfaceCallback(OH_NativeXComponent *xComponent, const std::string &id)
{
    static OH_NativeXComponent_Callback surfaceCb = {
        .OnSurfaceCreated = [](::OH_NativeXComponent *xc, void *win) {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "REGISTRY onSurfaceCreated: xc=%{public}p win=%{public}p", xc, win);
            std::lock_guard<std::mutex> lk(g_parentSurfaceMutex);
            g_parentSurface = reinterpret_cast<OHNativeWindow *>(win);
        },
        .OnSurfaceChanged = [](::OH_NativeXComponent *xc, void *win) {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "REGISTRY onSurfaceChanged: xc=%{public}p win=%{public}p", xc, win);
            if (win) {
                std::lock_guard<std::mutex> lk(g_parentSurfaceMutex);
                g_parentSurface = reinterpret_cast<OHNativeWindow *>(win);
            }
        },
        .OnSurfaceDestroyed = [](::OH_NativeXComponent *, void *) {
            std::lock_guard<std::mutex> lk(g_parentSurfaceMutex);
            g_parentSurface = nullptr;
        },
        .DispatchTouchEvent = [](::OH_NativeXComponent *xc, void *) {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "REGISTRY TouchEvent: xc=%{public}p", xc);
        },
    };

    auto rc = ::OH_NativeXComponent_RegisterCallback(xComponent, &surfaceCb);
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "REGISTRY RegisterCallback: rc=%{public}d xc=%{public}p id=%{public}s",
        rc, xComponent, id.c_str());
}

std::optional<QXComponentNode>
QXComponentRegistry::tryTakeNodeByXComponentId(const QXComponentId &id)
{
    std::optional<QXComponentNode> result;
    auto it = m_xComponents.find(id);
    if (it != m_xComponents.end()) {
        result = it->second;
        m_xComponents.erase(it);
    }
    return result;
}

QXComponentRegistry &QXComponentRegistry::instance()
{
    static QXComponentRegistry registry;
    return registry;
}

bool QXComponentRegistry::Init(napi_env env, napi_value exports)
{
    auto exportsObj = QNapi::checkedCast<QNapi::Object>(QNapi::Value{env, exports});
    auto xComponentWrappedValue = QNapi::getOptionalPropOrEmpty<QNapi::Object>(
        exportsObj,
        OH_NATIVE_XCOMPONENT_OBJ);
    if (xComponentWrappedValue.IsEmpty())
        return false;

    ::OH_NativeXComponent *xComponent = nullptr;
    auto status = ::napi_unwrap(
        env,
        xComponentWrappedValue,
        reinterpret_cast<void **>(&xComponent));
    if (status != ::napi_ok) {
        qOhosPrintfError("Failed to unwrap xcomponent with napi_status: %d", status);
        return false;
    }

    auto optXComponentId = QXComponentId::tryCreateFromXComponent(xComponent);
    if (!optXComponentId.has_value()) {
        qOhosPrintfError("Failed to retrieve id from xcomponent. Ignoring the component");
        return false;
    }

    auto xComponentId = optXComponentId.value();

    // Subwindow / floating-window render-target XComponents ("<id>_surf"):
    // capture their surface instead of registering them as node anchors.
    {
        const auto sid = xComponentId.stringId();
        if (sid.size() > sizeof(kSurfIdSuffix) - 1
            && sid.compare(
                   sid.size() - (sizeof(kSurfIdSuffix) - 1),
                   sizeof(kSurfIdSuffix) - 1,
                   kSurfIdSuffix) == 0) {
            registerCapturedSurfaceCallback(xComponent, sid);
            return true;
        }
    }

    auto optXComponentIdType = xComponentId.recognizedType();
    if (!optXComponentIdType.has_value()) {
        qOhosPrintfError(
            "Ignoring xComponent due to unrecognized id value: %s",
            xComponentId.stringId().c_str());
        return false;
    }

    bool canRegister = false;
    switch (optXComponentIdType.value()) {
    case QXComponentId::RecognizedType::RenderXComponent:
        canRegister = false;
        break;
    case QXComponentId::RecognizedType::NativeNodeFloatWindow:
    case QXComponentId::RecognizedType::NativeNodeSubWindow:
    case QXComponentId::RecognizedType::NativeNodeMainWindow:
        canRegister = true;
        break;
    }

    if (!canRegister) {
        qOhosPrintfDebug(
            "Ignoring xComponent because its id type is not supported in registry. id: %s type: %d",
            xComponentId.stringId().c_str(),
            optXComponentIdType.value());
        return false;
    }

    auto &registry = instance();

    bool xComponentRegistered;
    std::tie(std::ignore, xComponentRegistered) =
        registry.m_xComponents.emplace(xComponentId, QXComponentNode(xComponent));

    if (!xComponentRegistered) {
        qOhosReportFatalErrorAndAbort(
            "Error: Duplicate xComponent detected. Duplicate ID: %s",
            xComponentId.stringId().c_str());
    }

    // Register surface callback on the PARENT XComponent (ArkTS-created).
    // This must happen BEFORE the surface is allocated (which happens when
    // the ArkTS page is rendered). The C API child XComponent doesn't
    // support surface allocation, so we use the parent's surface instead.
    registerParentSurfaceCallback(xComponent, xComponentId.stringId());

    return true;
}

}

QT_END_NAMESPACE
