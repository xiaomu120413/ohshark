// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>
#include <QTime>

#include <qpa/qwindowsysteminterface.h>

#include <qohosdisplayinfo.h>
#include "qohosplatformscreen.h"
#include "qohosplatformbackingstore.h"
#include "qohosplatformintegration.h"
#include "qohosplatformwindow.h"
#include "qohosjsmain.h"
#include "qohosdeadlockprotector.h"
#include "render/qohosview.h"
#include <algorithm>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <qarkui/qarkuiutils.h>
#include <qguiapplication.h>
#include <qohosapppermissions_p.h>
#include <qohosjsutils.h>
#include <qohospixelmapconversions.h>
#include <qohosutils.h>
#include <render/qwindowproxyregistry.h>
#include <window_manager/oh_display_capture.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtCore/private/qnapi_p.h>
#include <QtCore/private/qohoslogger_p.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/private/qwindow_p.h>

#include <vector>

QT_BEGIN_NAMESPACE

namespace {

constexpr const char *pixelDensityRoundingCoefficientEnvVariableName = "OHOS_SCREEN_PIXEL_DENSITY_ROUNDING_COEFFICIENT";

std::shared_ptr<::OH_PixelmapNative> captureScreenPixelmap(
    QtOhos::JsState &, QOhosDisplayInfo::JsDisplayId displayId)
{
    ::OH_PixelmapNative *pixelMapNativePtr;

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_NativeDisplayManager_CaptureScreenPixelmap),
        static_cast<std::uint32_t>(displayId.value()), &pixelMapNativePtr);

    return wrapNativePixelMapPtr(pixelMapNativePtr);
}

void tryCaptureScreenPixelmapWithPermissionCheck(
    QtOhos::JsState &jsState, QOhosDisplayInfo::JsDisplayId displayId,
    QOhosConsumer<std::shared_ptr<::OH_PixelmapNative>> pixelMapOrNullConsumer)
{
    static constexpr const char *ohosCustomScreenCapturePermission =
        "ohos.permission.CUSTOM_SCREEN_CAPTURE";

    QOhosAppPermissions::checkAppPermissionGrantedWithConsumer(
        jsState, ohosCustomScreenCapturePermission,
        [pixelMapOrNullConsumer = std::move(pixelMapOrNullConsumer), displayId](
            auto &jsState, bool permissionGranted) {
            if (permissionGranted) {
                pixelMapOrNullConsumer(captureScreenPixelmap(jsState, displayId));
            } else {
                qOhosPrintfError(
                    "%s: %s hasn't been granted by user. Cannot grab window.", Q_FUNC_INFO,
                    ohosCustomScreenCapturePermission);
                pixelMapOrNullConsumer(nullptr);
            }
        });
}

QWindow *tryFindWindowByWIdOrNull(WId wId)
{
    auto allWindows = qApp->allWindows();
    auto windowItByWId = std::find_if(
        std::begin(allWindows), std::end(allWindows),
        [&](QWindow *w) {
            return w->winId() == wId;
        });

    if (windowItByWId == allWindows.end()) {
        return nullptr;
    }

    return *windowItByWId;
}

QSize calculateResultWindowCapturePixmapSize(
    const QSize &windowSnapshotSize, const QRect &windowSpaceCaptureRect)
{
    return {
        windowSpaceCaptureRect.width() < 0
            ? qMax(0, windowSnapshotSize.width() - windowSpaceCaptureRect.x())
            : windowSpaceCaptureRect.width(),
        windowSpaceCaptureRect.height() < 0
            ? qMax(0, windowSnapshotSize.height() - windowSpaceCaptureRect.y())
            : windowSpaceCaptureRect.height()
    };
}

QRect calculateResultPixmapWindowFragmentRect(
    const QRect &windowSpaceCaptureRect, const QRect &windowFragmentRect)
{
    return {
        qAbs(windowSpaceCaptureRect.x() - windowFragmentRect.x()),
        qAbs(windowSpaceCaptureRect.y() - windowFragmentRect.y()),
        windowFragmentRect.width(),
        windowFragmentRect.height()
    };
}

QPixmap grabWindowFromCapturedScreenPixmap(
    const QOhosPlatformScreen *platformScreen, const QPixmap &capturedWindowPixmap,
    const QRect &windowSpaceCaptureRect)
{
    auto resultSize = calculateResultWindowCapturePixmapSize(
        QHighDpi::fromNativePixels(capturedWindowPixmap.size(), platformScreen),
        windowSpaceCaptureRect);

    if (resultSize.isEmpty()) {
        return {};
    }

    auto windowFragmentRect = capturedWindowPixmap.rect().intersected(
        QRect(windowSpaceCaptureRect.topLeft(), resultSize));

    if (windowFragmentRect.isEmpty()) {
        return QPixmap(resultSize);
    }

    auto resultSpaceWindowFragmentRect =
        calculateResultPixmapWindowFragmentRect(windowSpaceCaptureRect, windowFragmentRect);

    QPixmap result(resultSize);
    QPainter p(&result);
    p.drawPixmap(
        QHighDpi::toNativePixels(resultSpaceWindowFragmentRect, platformScreen),
        capturedWindowPixmap,
        QHighDpi::toNativePixels(windowFragmentRect, platformScreen));

    return result;
}

QOhosOptional<Qt::ScreenOrientation> tryMapJsDisplayOrientationToQt(QOhosDisplayInfo::JsDisplayOrientation jsDisplayOrientation)
{
    using JsDisplayOrientation = QOhosDisplayInfo::JsDisplayOrientation;

    switch (jsDisplayOrientation) {
    case JsDisplayOrientation::PORTRAIT:
        return makeQOhosOptional(Qt::ScreenOrientation::PortraitOrientation);
    case JsDisplayOrientation::LANDSCAPE:
        return makeQOhosOptional(Qt::ScreenOrientation::LandscapeOrientation);
    case JsDisplayOrientation::PORTRAIT_INVERTED:
        return makeQOhosOptional(Qt::ScreenOrientation::InvertedPortraitOrientation);
    case JsDisplayOrientation::LANDSCAPE_INVERTED:
        return makeQOhosOptional(Qt::ScreenOrientation::InvertedLandscapeOrientation);
    }

    return {};
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)

QOhosOptional<double> tryReadPixelDensityRoundingCoefficientFromEnvironment()
{
    const auto pixelDensityRoundingCoefficientEnvVar = qgetenv(pixelDensityRoundingCoefficientEnvVariableName);
    const auto pixelDensityRoundingCoefficient = QtOhos::tryParseStringAsFiniteDouble(
        pixelDensityRoundingCoefficientEnvVar.toStdString());
    if (!pixelDensityRoundingCoefficient.hasValue() && !pixelDensityRoundingCoefficientEnvVar.isEmpty()) {
        qCWarning(
            QtForOhos, "%s: Cannot convert %s env variable to double value",
            Q_FUNC_INFO, pixelDensityRoundingCoefficientEnvVariableName);
    }

    return pixelDensityRoundingCoefficient;
}

#else

QOhosOptional<double> tryReadPixelDensityRoundingCoefficientFromEnvironment()
{
    if (!qEnvironmentVariableIsEmpty(pixelDensityRoundingCoefficientEnvVariableName)) {
        qCWarning(
            QtForOhos, "%s: %s env variable has no impact on qt version 5.15 or higher",
            Q_FUNC_INFO, pixelDensityRoundingCoefficientEnvVariableName);
    }

    return {};
}

#endif

double roundPixelDensityScaleIfRoundingEnabled(double pixelDensityScale)
{
    static const auto pixelDensityRoundingCoefficient = tryReadPixelDensityRoundingCoefficientFromEnvironment();

    return pixelDensityRoundingCoefficient.hasValue()
        ? qRound(pixelDensityScale / pixelDensityRoundingCoefficient.value()) * pixelDensityRoundingCoefficient.value()
        : pixelDensityScale;
}

}

QOhosPlatformScreen::QOhosPlatformScreen(const QOhosDisplayInfo &displayInfo, QOhosSupplier<std::vector<QOhosPlatformScreen *>> platformScreenListSupplier)
    : QObject()
    , QPlatformScreen()
    , m_displayInfo(displayInfo)
    , m_availableGeometry(getAvailableArea())
    , m_platformScreenListSupplier(std::move(platformScreenListSupplier))
{
    auto __dbg = make_QCScopedDebug("QOhosPlatformScreen::QOhosPlatformScreen");
    // Raster only apps should set QT_OHOS_RASTER_IMAGE_DEPTH to 16
    // is way much faster than 32
    if (qEnvironmentVariableIntValue("QT_OHOS_RASTER_IMAGE_DEPTH") == 16) {
        m_format = QImage::Format_RGB16;
        m_depth = 16;
    } else {
        m_format = QImage::Format_ARGB32_Premultiplied;
        m_depth = 32;
    }
    m_platformCursor.reset(new QOhosPlatformCursor());
}

QOhosPlatformScreen::~QOhosPlatformScreen() = default;

QOhosPlatformScreen *QOhosPlatformScreen::fromQScreen(QScreen *screen)
{
    auto *platformScreen = screen->handle();
    if (Q_UNLIKELY(platformScreen == nullptr)) {
        qOhosReportFatalErrorAndAbort("QScreen::handle() returned null");
    }
    return static_cast<QOhosPlatformScreen *>(platformScreen);
}

QWindow *QOhosPlatformScreen::topLevelAt(const QPoint &p) const
{
    for (auto *platformScreen : virtualSiblings()) {
        auto *ohosPlatformScreen = static_cast<QOhosPlatformScreen *>(platformScreen);

        auto windowIds = QOhosWindowProxy::queryWindowIdsByCoordinate(ohosPlatformScreen->m_displayInfo.id, p);
        if (!windowIds.empty()) {
            return QWindowProxyRegistry::instance().findQWindowByJsWindowIdOrNull(windowIds.front());
        }
    }

    return nullptr;
}

QPlatformCursor *QOhosPlatformScreen::cursor() const
{
    return m_platformCursor.data();
}

void QOhosPlatformScreen::setDisplayInfo(const QOhosDisplayInfo &displayInfo)
{
    auto geometryChanged = displayInfo.displayGeometryPixels() != m_displayInfo.displayGeometryPixels();
    auto logicalDpiChanged = displayInfo.densityDPI != m_displayInfo.densityDPI;

    QVector<QPair<QWindow *, QOhosOptional<QRect>>> windowRectPairs;

    if (logicalDpiChanged) {
        for (auto *window : qGuiApp->allWindows()) {
            auto *platformWindow = QOhosPlatformWindow::fromQWindowOrNull(window);
            if (platformWindow == nullptr || platformWindow->screen() != this || !window->isVisible()) {
                continue;
            }

            auto *ownedView = platformWindow->ownedViewOrNull();
            if (ownedView == nullptr) {
                continue;
            }

            windowRectPairs.push_back({
                window,
                ownedView->viewType() == QOhosView::ViewType::EmbeddedWindow
                    ? makeQOhosOptional(window->geometry())
                    : makeEmptyQOhosOptional()});
        }
    }

    m_displayInfo = displayInfo;

    if (geometryChanged) {
        QWindowSystemInterface::handleScreenGeometryChange(QPlatformScreen::screen(), geometry(), availableGeometry());
    }

    if (logicalDpiChanged) {
        auto ldpi = logicalDpi();
        QWindowSystemInterface::handleScreenLogicalDotsPerInchChange(
            screen(), ldpi.first, ldpi.second);

        QWindowSystemInterface::flushWindowSystemEvents();

        for (const auto &windowRectPair : windowRectPairs) {
            auto *qWindow = windowRectPair.first;
            auto *platformWindow = QOhosPlatformWindow::fromQWindow(qWindow);
            if (windowRectPair.second.hasValue()) {
                qWindow->setGeometry(windowRectPair.second.value());
            }
            platformWindow->handleDpiChange();
        }
    }
}

QDpi QOhosPlatformScreen::logicalDpi() const
{
    qreal lDpi = 96 * roundPixelDensityScaleIfRoundingEnabled(m_displayInfo.densityScaled);
    return QDpi(lDpi, lDpi);
}

qreal QOhosPlatformScreen::pixelDensity() const
{
    return roundPixelDensityScaleIfRoundingEnabled(m_displayInfo.densityPixels);
}

Qt::ScreenOrientation QOhosPlatformScreen::orientation() const
{
    return
        m_displayInfo.orientation
        .andThen(tryMapJsDisplayOrientationToQt)
        .valueOr(Qt::ScreenOrientation::PrimaryOrientation);
}

Qt::ScreenOrientation QOhosPlatformScreen::nativeOrientation() const
{
    return Qt::ScreenOrientation::PrimaryOrientation;
}

QRect QOhosPlatformScreen::getAvailableArea() const
{
    return QtOhos::evalInJsThreadWithPromise<QRect>(
        [&](QtOhos::JsState &jsState, QOhosTaskPromise<QRect> evalPromise) {

            QNapi::Object display;
            try {
                display = jsState.eval<QNapi::Object>(
                    "@ohos.display.getDisplayByIdSync(*)", {m_displayInfo.id.value()});
            } catch (const Napi::Error &error) {
                qOhosPrintfError(
                    "%s: getDisplayByIdSync(%f) failed with error: %s",
                    Q_FUNC_INFO, m_displayInfo.id.value(), error.Message().c_str());
                evalPromise(QRect());
                return;
            }

            auto thenCatchPromises = std::move(evalPromise).makeThenCatchBranches(Q_FUNC_INFO);
            display.evalToPromiseOrRejectOnThrow("getAvailableArea()")
            .onThen(
                [thenPromise = std::move(thenCatchPromises.first)](const QtOhos::CallbackInfo &cbInfo) {
                    auto availableArea = cbInfo.getFirstArg<QNapi::Object>(Q_FUNC_INFO);
                    thenPromise(
                        QRect(
                            availableArea.get<QNapi::Number>("left"),
                            availableArea.get<QNapi::Number>("top"),
                            availableArea.get<QNapi::Number>("width"),
                            availableArea.get<QNapi::Number>("height")));
                })
            .onCatch(
                [catchPromise = std::move(thenCatchPromises.second)](const QtOhos::CallbackInfo &cbInfo) {
                    QtOhos::logJsCallbackError(cbInfo, "Error occurred in JS getAvailableArea()");
                    catchPromise(QRect());
                });
        },
        Q_FUNC_INFO);
}

void QOhosPlatformScreen::releaseSurface()
{
}

const QOhosDisplayInfo &QOhosPlatformScreen::displayInfo() const
{
    return m_displayInfo;
}

QRect QOhosPlatformScreen::geometry() const
{
    return m_displayInfo.displayGeometryPixels();
}

QRect QOhosPlatformScreen::availableGeometry() const
{
    bool handheldDeviceFullScreen = queryQOhosRuntimeDeviceAndMode() == QOhosRuntimeDeviceTypeAndMode::HandheldDeviceFullScreen;
    constexpr auto syntheticAvailableGeometryVerticalMarginOnTablet = QMargins(0, 88, 0, 151);

    if (handheldDeviceFullScreen && (m_availableGeometry.isEmpty() || m_availableGeometry.height() == geometry().height())) {
        return m_availableGeometry.marginsRemoved(syntheticAvailableGeometryVerticalMarginOnTablet);
    }

    auto screenGeometry = geometry();
    return !m_availableGeometry.isEmpty()
        ? m_availableGeometry.translated(screenGeometry.topLeft())
        : screenGeometry;
}

int QOhosPlatformScreen::depth() const
{
    return m_depth;
}

QImage::Format QOhosPlatformScreen::format() const
{
    return m_format;
}

QSizeF QOhosPlatformScreen::physicalSize() const
{
    return m_displayInfo.physicalSize();
}

void QOhosPlatformScreen::setAvailableGeometry(const QRect &rect)
{
    if (rect != m_availableGeometry) {
        m_availableGeometry = rect;
        QWindowSystemInterface::handleScreenGeometryChange(QPlatformScreen::screen(), geometry(), availableGeometry());
    }
}

QList<QPlatformScreen *> QOhosPlatformScreen::virtualSiblings() const
{
    if (!m_displayInfo.isDisplayMainOrExtended()) {
        return QPlatformScreen::virtualSiblings();
    }

    QList<QPlatformScreen *> result;
    auto allPlatformScreens = m_platformScreenListSupplier();
    std::copy_if(
        allPlatformScreens.begin(), allPlatformScreens.end(),
        std::back_inserter(result),
        [](QOhosPlatformScreen *ohosPlatformScreen) {
            return ohosPlatformScreen->displayInfo().isDisplayMainOrExtended();
        });

    return result;
}

QPixmap QOhosPlatformScreen::grabWindow(WId wId, int x, int y, int width, int height) const
{
    auto captureRect = QHighDpi::toNativePixels(QRect(x, y, width, height), this);

    if (wId != 0) {
        auto *window = tryFindWindowByWIdOrNull(wId);
        if (window == nullptr) {
            qOhosPrintfError(
                "%s: Cannot find window with the given WId: %lld.", Q_FUNC_INFO, wId);
            return {};
        }
        return grabWindowFromCapturedScreenPixmap(
            this, QOhosPlatformWindow::fromQWindow(window)->makeSnapshot(), captureRect);
    }

    auto capturedScreenPixmap = QtOhos::evalInJsThreadWithPromise<QPixmap>(
        [displayId = m_displayInfo.id](
            QtOhos::JsState &jsState, QOhosTaskPromise<QPixmap> evalPromise) {
            auto sharedEvalPromise = QtOhos::moveToSharedPtr(std::move(evalPromise).makeChained(Q_FUNC_INFO));
            tryCaptureScreenPixelmapWithPermissionCheck(
                jsState, displayId,
                [sharedEvalPromise](
                    std::shared_ptr<::OH_PixelmapNative> optPixelMap) {
                    (*sharedEvalPromise)(
                        optPixelMap
                            ? QPixmap::fromImage(createQImageFromNativePixelMap(optPixelMap.get()))
                            : QPixmap());
                });
        },
        Q_FUNC_INFO);

    return capturedScreenPixmap.copy(captureRect);
}

QString QOhosPlatformScreen::name() const
{
    return m_displayInfo.name;
}

QT_END_NAMESPACE
