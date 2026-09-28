// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "accessibility/qohosaccessibilityeventhandler.h"
#include "accessibility/qohosaccessibilityprovider.h"
#include "accessibility/qohosplatformaccessibility.h"
#include "private/qohosplatformtheme_p.h"
#include "qohosapplicationstatetracker.h"
#include "qohoseglplatformcontext.h"
#include "qohoseventdispatcher.h"
#include "qohosfloatingwindow.h"
#include "qohosforeignwindow.h"
#include "qohosjsmain.h"
#include "qohosplatformbackingstore.h"
#include "qohosplatformbackingstoregl.h"
#include "qohosplatformdrag.h"
#include "qohosplatformintegration.h"
#include "qohosplatformnativeinterface.h"
#include "qohosplatformscreen.h"
#include "qohosplatformservices.h"
#include "qohosplatformwindow.h"
#include "qohosinputcontext.h"
#include "qohosinputmethodeventhandler.h"
#include "qohosplatformtheme.h"
#include "qohossystemlocale.h"
#include "qohosuiextensionplatformwindow.h"
#include "qohosutils.h"

#include <qohosplugincore.h>

#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtCore/private/qnapi_p.h>
#include <QtCore/private/qohoslogger_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/qcoreapplication.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <QtFontDatabaseSupport/private/qohosplatformfontdatabase_p.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <QtCore/qset.h>
#include <QtPlatformHeaders/QEGLNativeContext>


QT_BEGIN_NAMESPACE

namespace {

bool isInputDeviceOfType(QtOhos::JsState &jsState, std::uint32_t deviceId, const std::string &type)
{
    auto devSources = QNapi::getArrayElements<std::vector<std::string>, QNapi::String>(
        jsState.eval<QNapi::Array>(
            "@ohos.multimodalInput.inputDevice.getDeviceInfoSync(*).sources",
            {deviceId}));
    return std::find(devSources.begin(), devSources.end(), type) != devSources.end();
}

bool isInputDeviceWithTouchscreen(QtOhos::JsState &jsState, std::uint32_t deviceId)
{
    return isInputDeviceOfType(jsState, deviceId, "touchscreen");
}

bool isInputDeviceWithTouchpad(QtOhos::JsState &jsState, std::uint32_t deviceId)
{
    return isInputDeviceOfType(jsState, deviceId, "touchpad");
}

bool isDeviceTypeInDeviceIds(
    QtOhos::JsState &jsState, const std::vector<std::uint32_t> &deviceIds,
    const std::function<bool(QtOhos::JsState &, std::uint32_t)> &isDeviceTypeFunc)
{
    return std::any_of(
        deviceIds.begin(), deviceIds.end(),
        [&](std::uint32_t deviceId) {
            return isDeviceTypeFunc(jsState, deviceId);
        });
}

std::set<QTouchDevice::DeviceType> getAvailableDeviceTypes()
{
    return QtOhos::evalInJsThreadWithPromise<std::set<QTouchDevice::DeviceType>>(
        [](QtOhos::JsState &jsState, QOhosTaskPromise<std::set<QTouchDevice::DeviceType>> evalPromise) {
            auto thenCatchPromises = std::move(evalPromise).makeThenCatchBranches(Q_FUNC_INFO);
            jsState.evalToPromiseOrRejectOnThrow("@ohos.multimodalInput.inputDevice.getDeviceList()")
                .onThen(
                    [thenPromise = std::move(thenCatchPromises.first)](const QtOhos::CallbackInfo &cbInfo) {
                        auto deviceIdsJsArray = cbInfo.getFirstArg<QNapi::Array>(Q_FUNC_INFO);
                        auto deviceIds = QNapi::getArrayElements<std::vector<std::uint32_t>, QNapi::Number>(deviceIdsJsArray);
                        std::set<QTouchDevice::DeviceType> deviceTypes;
                        if (isDeviceTypeInDeviceIds(cbInfo.jsState(), deviceIds, isInputDeviceWithTouchscreen)) {
                            deviceTypes.insert(QTouchDevice::DeviceType::TouchScreen);
                        }
                        if (isDeviceTypeInDeviceIds(cbInfo.jsState(), deviceIds, isInputDeviceWithTouchpad)) {
                            deviceTypes.insert(QTouchDevice::DeviceType::TouchPad);
                        }
                        thenPromise(deviceTypes);
                    })
                .onCatch(
                    [catchPromise = std::move(thenCatchPromises.second)](const QtOhos::CallbackInfo &) {
                        qOhosPrintfError("Error while obtaining device list (@ohos.multimodalInput.inputDevice.getDeviceList())");
                        catchPromise({});
                    });
        },
        Q_FUNC_INFO);
}

}

QScopedPointer<QOhosSystemLocale> QOhosPlatformIntegration::m_systemLocale;
QOhosPlatformIntegration::WindowGeometryPersistencePolicy QOhosPlatformIntegration::m_mainWindowPersistencePolicy =
    QOhosPlatformIntegration::WindowGeometryPersistencePolicy::Disabled;

QOhosPlatformIntegration *QOhosPlatformIntegration::instance()
{
    return static_cast<QOhosPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
}

QOhosPlatformIntegration::QOhosPlatformIntegration(const QStringList &paramList)
{
    Q_UNUSED(paramList);
    auto __dbg = make_QCScopedDebug("QOhosPlatformIntegration::QOhosPlatformIntegration");
    m_ohosPlatformNativeInterface.reset(new QOhosPlatformNativeInterface());

    if (!QtOhos::isOhosNoUiChildMode()) {
        auto eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (Q_UNLIKELY(eglDisplay == EGL_NO_DISPLAY))
            qOhosReportFatalErrorAndAbort("Could not open egl display");

        EGLint major;
        EGLint minor;
        if (Q_UNLIKELY(!eglInitialize(eglDisplay, &major, &minor)))
            qOhosReportFatalErrorAndAbort("Could not initialize egl display");

        m_eglDisplayHandle = QtOhos::makeDestroyNotifier(
            [eglDisplay]() {
                eglTerminate(eglDisplay);
            });

        if (Q_UNLIKELY(!eglBindAPI(EGL_OPENGL_ES_API)))
            qOhosReportFatalErrorAndAbort("Could not bind GL_ES API");
    }

    if (!QtOhos::isOhosNoUiChildMode()) {
        m_screenManager = std::make_unique<QOhosScreenManager>();
    }

    m_mainThread = QThread::currentThread();

    m_ohosFDB = std::make_unique<QOhosPlatformFontDatabase>();
    m_ohosPlatformServices = std::make_unique<QOhosPlatformServices>();
    if (!QtOhos::isOhosNoUiChildMode()) {
        m_ohosInputMethodEventHandler = std::make_unique<QOhosInputMethodEventHandler>(getAvailableDeviceTypes());
    }

#ifndef QT_NO_CLIPBOARD
    if (!QtOhos::isOhosNoUiChildMode()) {
        m_platformClipboard = std::make_unique<QOhosPlatformClipboard>();
    }
#endif // QT_NO_CLIPBOARD

#if QT_CONFIG(draganddrop)
    if (!QtOhos::isOhosNoUiChildMode()) {
        m_drag = makeQOhosPlatformDrag();
    }
#endif // QT_CONFIG(draganddrop)

#ifndef QT_NO_ACCESSIBILITY
    if (QtOhos::isA11ySupportEnabled()) {
        m_accessibilityContext = makeAccessibilityContext();
        QCoreApplication::instance()->installEventFilter(
            QOhos::makeAccessibilityEventHandler().release());
    }
#endif // QT_NO_ACCESSIBILITY

    QWindowSystemInterfacePrivate::TabletEvent::setPlatformSynthesizesMouse(false);

    // QCoreApplication::postEvent takes ownership of the created event.
    QCoreApplication::postEvent(m_ohosPlatformNativeInterface.data(), new QEvent(QEvent::User));
}

QOhosInputMethodEventHandler *QOhosPlatformIntegration::inputMethodEventHandler()
{
    return m_ohosInputMethodEventHandler.get();
}

void QOhosPlatformIntegration::setDarkThemeEnabled(bool darkThemeEnabled)
{
    auto *ohosPlatformTheme = static_cast<QOhosPlatformTheme *>(QGuiApplicationPrivate::platformTheme());
    const auto newTheme = darkThemeEnabled
        ? QOhosPlatformTheme::ColorsTheme::Dark
        : QOhosPlatformTheme::ColorsTheme::Light;

    ohosPlatformTheme->setColorsTheme(newTheme);

    const auto allWindows = qGuiApp->allWindows();
    if (allWindows.empty()) {
        QWindowSystemInterface::handleThemeChange(nullptr);
    } else {
        for (auto *window : allWindows) {
            QWindowSystemInterface::handleThemeChange(window);
        }
    }
}

#ifndef QT_NO_ACCESSIBILITY

std::unique_ptr<QOhosPlatformIntegration::AccessibilityContext>
QOhosPlatformIntegration::makeAccessibilityContext()
{
    struct JsScopeData
    {
        std::shared_ptr<QOhos::AccessibilityTree> accessibilityTree;
        std::shared_ptr<QOhos::AccessibilityXComponentRegistry> accessibilityXComponentRegistry;
        std::shared_ptr<QOhos::AccessibilityActionsConsumer> accessibilityActionsConsumer;
    };

    auto accessibilityTreeContext = QOhos::makeAccessibilityTreeContext();
    auto jsScopeData = QtOhos::makeProxyWithJsThreadDeleter(std::make_shared<JsScopeData>());
    jsScopeData->accessibilityTree = accessibilityTreeContext.accessibilityTree;
    jsScopeData->accessibilityXComponentRegistry =
        accessibilityTreeContext.accessibilityXComponentRegistry;
    auto accessibilityTreeEventsConsumer =
        QOhos::makeGeometryValidatingAccessibilityTreeEventsConsumerDecorator(
            QOhos::makeAccessibilityTreeEventsConsumerJsDecorator(
                accessibilityTreeContext.accessibilityTreeEventsConsumer));
    jsScopeData->accessibilityActionsConsumer = QOhos::makeAccessibilityActionsQtDecorator(
        QOhos::makeAccessibilityActionsConsumer(accessibilityTreeEventsConsumer));

    return std::make_unique<AccessibilityContext>(
        AccessibilityContext {
            .accessibility = std::make_unique<QOhosPlatformAccessibility>(accessibilityTreeEventsConsumer),
            .tryInitializeAccessibilityProviderInJsThreadFunc =
                [jsScopeData](auto qAccessibilityRootId, const auto &xComponentProvider) {
                    return QtOhos::evalInJsThread(
                        [&](QtOhos::JsState &jsState) {
                            return QOhos::tryInitializeAccessibilityProvider(
                                xComponentProvider(jsState), jsScopeData->accessibilityTree,
                                jsScopeData->accessibilityXComponentRegistry,
                                jsScopeData->accessibilityActionsConsumer,
                                qAccessibilityRootId);
                        },
                        Q_FUNC_INFO);
                },
            .accessibilityDataHandle = jsScopeData,
        });
}

QPlatformAccessibility *QOhosPlatformIntegration::accessibility() const
{
    return m_accessibilityContext ? m_accessibilityContext->accessibility.get() : nullptr;
}

std::shared_ptr<void> QOhosPlatformIntegration::tryInitializeAccessibilityForQWindowWithXComponent(
    QWindow *qWindow, const std::function<QXComponentRender(QtOhos::JsState &)> &xComponentProvider)
{
    if (!m_accessibilityContext) {
        return nullptr;
    }

    auto qAccessibilityRootId = qWindow->accessibleRoot() != nullptr
        ? makeQOhosOptional(
            QOhos::AccessibilityNode::Id(
                QAccessible::uniqueId(qWindow->accessibleRoot())))
        : makeEmptyQOhosOptional();

    if (!qAccessibilityRootId.hasValue()) {
        return nullptr;
    }

    return m_accessibilityContext->tryInitializeAccessibilityProviderInJsThreadFunc(
        qAccessibilityRootId.value(), xComponentProvider);
}

#endif // QT_NO_ACCESSIBILITY

QPoint QOhosPlatformIntegration::getSyntheticQPointForContextMenuOnLongPress()
{
    constexpr std::int32_t longPressIdentifierMagicNumber = 1844133518;
    return QPoint(longPressIdentifierMagicNumber, longPressIdentifierMagicNumber);
}

void QOhosPlatformIntegration::setContextMenuOnLongPressEnabled()
{
    m_isContextMenuOnLongPressEnabled = true;
}

bool QOhosPlatformIntegration::isContextMenuOnLongPressEnabled() const
{
    return QtOhos::isSupportContextMenuEventOnLongPressEnabled() && m_isContextMenuOnLongPressEnabled;
}

QAbstractEventDispatcher *QOhosPlatformIntegration::createEventDispatcher() const
{
    return new QOhosEventDispatcher;
}

QVariant QOhosPlatformIntegration::styleHint(StyleHint hint) const
{
    if (hint == ShowIsMaximized)
        return false;
    return QPlatformIntegration::styleHint(hint);
}

Qt::WindowState QOhosPlatformIntegration::defaultWindowState(Qt::WindowFlags flags) const
{
    // Don't maximize dialogs on Android
    if ((flags & Qt::Dialog & ~Qt::Window) != 0)
        return Qt::WindowNoState;

    return QPlatformIntegration::defaultWindowState(flags);
}

QPlatformWindow *QOhosPlatformIntegration::createPlatformWindow(QWindow *window) const
{
    auto __dbg = make_QCScopedDebug("QOhosPlatformIntegration::createPlatformWindow");

    if (QtOhos::isOhosUiExtensionMode()) {
        return std::make_unique<QOhosInstanceUiExtensionPlatformWindow>(window).release();
    } else if (QtOhos::isOhosBundledUiExtensionMode()) {
        return std::make_unique<QOhosBundleUiExtensionPlatformWindow>(window).release();
    }

    static const QSet<QString> nativeDialogClass = {
        QString::fromUtf8("QFileDialogClassWindow"),
    };
    // FIXME: - System that decides the window class should be reworked
    // For now this behaviour avoids potential crashes related to the lack of surface
    if (window != nullptr && !nativeDialogClass.contains(window->objectName())){
        return new QOhosFloatingWindow(window);
    }
    return new QOhosPlatformWindow(window);
}

QPlatformWindow *QOhosPlatformIntegration::createForeignWindow(QWindow *window, WId windowId) const
{
    return new QOhosForeignWindow(window, windowId);
}

QPlatformFontDatabase *QOhosPlatformIntegration::fontDatabase() const
{
    return m_ohosFDB.get();
}

#ifndef QT_NO_CLIPBOARD
QOhosPlatformClipboard *QOhosPlatformIntegration::clipboard() const
{
    return m_platformClipboard.get();
}
#endif

#if QT_CONFIG(draganddrop)
QPlatformDrag *QOhosPlatformIntegration::drag() const
{
    return m_drag.get();
}
#endif // QT_CONFIG(draganddrop)

QPlatformBackingStore *QOhosPlatformIntegration::createPlatformBackingStore(QWindow *window) const
{
    std::unique_ptr<QPlatformBackingStore> result;
    switch (window->surfaceType()) {
    case QSurface::RasterSurface:
        result = std::make_unique<QOhosPlatformBackingStore>(
            window,
            QOhosPlatformBackingStore::CreateInfo{
                .debugDrawFlushedRegion = QtOhos::isDebugDrawQtRasterBackingStoreFlushedRegionEnabled(),
                .enableVsync = QtOhos::isVsyncOnSoftwareBackingStoreEnabled(),
            });
        break;
    case QSurface::RasterGLSurface:
        // NOTE - This is temporary change done so that tests can be performed
        // on the new implementation - if there are no problems
        // a switch to it as the default will be made
        result = QtOhos::isGlBackingStoreDefaultEnabled()
            ? makeGlOhosPlatformBackingStore(window)
            : std::make_unique<QOhosPlatformBackingStore>(
                window,
                QOhosPlatformBackingStore::CreateInfo{
                    .debugDrawFlushedRegion = QtOhos::isDebugDrawQtRasterBackingStoreFlushedRegionEnabled(),
                    .enableVsync = QtOhos::isVsyncOnSoftwareBackingStoreEnabled(),
                });
        break;
    case QSurface::OpenGLSurface:
    case QSurface::OpenVGSurface:
    case QSurface::VulkanSurface:
    case QSurface::MetalSurface:
        qOhosReportFatalErrorAndAbort("Unsupported window surface type for backing store: %d", window->surfaceType());
        break;
    }

    return result.release();
}
#ifndef QT_NO_OPENGL
QPlatformOpenGLContext *QOhosPlatformIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    QSurfaceFormat targetSurfaceFormat = context->format();
    targetSurfaceFormat.setRedBufferSize(8);
    targetSurfaceFormat.setGreenBufferSize(8);
    targetSurfaceFormat.setBlueBufferSize(8);
    targetSurfaceFormat.setAlphaBufferSize(8);

    QOhosEGLPlatformContext::CreateInfo ctxCreateInfo{};
    ctxCreateInfo.format = targetSurfaceFormat;
    ctxCreateInfo.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ctxCreateInfo.optionalNativeHandle = context->nativeHandle();
    ctxCreateInfo.optionalShareContext = context->shareHandle();

    auto *eglCtx = new QOhosEGLPlatformContext(ctxCreateInfo);
    auto eglNativeCtxObject = QEGLNativeContext(eglCtx->eglContext(), ctxCreateInfo.display);
    context->setNativeHandle(QVariant::fromValue<QEGLNativeContext>(eglNativeCtxObject));

    return eglCtx;
}
#endif // QT_NO_OPENGL

QPlatformOffscreenSurface *QOhosPlatformIntegration::createPlatformOffscreenSurface(QOffscreenSurface *) const
{
    auto __dbg = make_QCScopedDebug("QOhosPlatformIntegration::createPlatformOffscreenSurface");
    return nullptr;
}

bool QOhosPlatformIntegration::hasCapability(Capability cap) const
{
    qOhosDebug(QtForOhos) << "QOhosPlatformIntegration::hasCapability:" << cap;

    switch (cap) {
        case ApplicationState: return true;
        case ThreadedPixmaps: return true;
        case NativeWidgets: return true ;// HACK QtAndroid::activity();
        case OpenGL: return true ;// HACK return QtAndroid::activity();
        case ForeignWindows: return true ;// HACK return QtAndroid::activity();
        case ThreadedOpenGL: return true ;// HACK return !needsBasicRenderloopWorkaround() && QtAndroid::activity();
        case RasterGLSurface: return true ;// HACK  return QtAndroid::activity();
        case MultipleWindows: return true;
        case WindowManagement: return true;
        case TopStackedNativeChildWindows: return false;
        default:
            return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformInputContext *QOhosPlatformIntegration::inputContext() const
{
    return m_platformInputContext.data();
}

void QOhosPlatformIntegration::initialize()
{
    auto d = make_QCScopedDebug("QOhosPlatformIntegration::initialize");
    const QString requestedInputContext = QPlatformInputContextFactory::requested();
    if (requestedInputContext.isNull()) {
        QOhosInputContext *context = new QOhosInputContext;
        m_platformInputContext.reset(context);
    } else {
        m_platformInputContext.reset(QPlatformInputContextFactory::create(requestedInputContext));
    }

    if (QWindowSystemInterfacePrivate::eventHandler != nullptr) {
        qOhosReportFatalErrorAndAbort("QWindowSystemInterfacePrivate::eventHandler was already registered.");
    }

    auto tracker = makeApplicationStateTracker();
    QWindowSystemInterfacePrivate::installWindowSystemEventHandler(tracker.get());
    m_applicationStateTracker = QtOhos::makeDestroyNotifier(
        [tracker = std::move(tracker)]() {
            QWindowSystemInterfacePrivate::removeWindowSystemEventhandler(tracker.get());
        });
}

QPlatformTheme *QOhosPlatformIntegration::createPlatformTheme(const QString &name) const
{
    if (QtOhos::isDebugUseBasicStyleAndThemeEnabled()) {
        return std::make_unique<QPlatformTheme>().release();
    }

    if (name == QString::fromUtf8(ohosThemeName)) {
        return new QOhosPlatformTheme();
    }
    return nullptr;
}

QStringList QOhosPlatformIntegration::themeNames() const
{
    return {QString::fromUtf8(ohosThemeName)};
}

QOhosSystemLocale *QOhosPlatformIntegration::systemLocale()
{
    return m_systemLocale.data();
}

void QOhosPlatformIntegration::setSystemLocale(QOhosSystemLocale *systemLocale)
{
    m_systemLocale.reset(systemLocale);
}

void QOhosPlatformIntegration::setMainWindowGeometryPersistencePolicy(
    QOhosPlatformIntegration::WindowGeometryPersistencePolicy policy)
{
    m_mainWindowPersistencePolicy = policy;
}

QOhosPlatformIntegration::WindowGeometryPersistencePolicy QOhosPlatformIntegration::getMainWindowGeometryPersistencePolicy()
{
    return m_mainWindowPersistencePolicy;
}

QPlatformServices *QOhosPlatformIntegration::services() const
{
    return m_ohosPlatformServices.get();
}

QPlatformNativeInterface *QOhosPlatformIntegration::nativeInterface() const
{
    return m_ohosPlatformNativeInterface.get();
}

QOhosScreenManager *QOhosPlatformIntegration::screenManager() const
{
    return m_screenManager.get();
}

QT_END_NAMESPACE
