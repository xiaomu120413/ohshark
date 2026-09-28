#include <hilog/log.h>
// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qohosfloatingwindow.h>

#include <QtCore/private/qohoslogger_p.h>
#include "QtGui/private/qguiapplication_p.h"
#include <QtGui/private/qwindow_p.h>
#include <qohosdeviceinfo_p.h>
#include <qohosinputcontext.h>
#include <qohosinputmethodeventhandler.h>
#include <qohosjsmain.h>
#include <qohosplatformbackingstore.h>
#include <qohosplatformwindow.h>
#include <qohosruntimedevicetypeandmode.h>
#include <qohossettings.h>
#include <render/qohossurface.h>
#include <render/qohoswindowproxy.h>
#include <render/qwindowproxyregistry.h>

QT_BEGIN_NAMESPACE

namespace {

bool isWindowRotatedByTabletScreenRotation(
    QWindow *window, QOhosWindowProxy::RectChangeOptions rectChangeOptions)
{
    return !QOhosSettings::instance().isWindowPcModeEnabled()
        && rectChangeOptions.rect.isValid()
        && window->geometry() != rectChangeOptions.rect
        && rectChangeOptions.reason == QOhosWindowProxy::RectChangeReason::UNDEFINED;
}

bool shouldClearQtFocusOnSystemFocusLoss(QWindow *windowLosingSystemFocus)
{
    return QGuiApplicationPrivate::focus_window == windowLosingSystemFocus
        || (QGuiApplicationPrivate::focus_window != nullptr
            && QOhosPlatformWindow::platformWindowFlagsForQWindow(
                QGuiApplicationPrivate::focus_window).testFlag(Qt::WindowDoesNotAcceptFocus));
}

bool anyQtWindowHoldsSystemFocus()
{
    return !QWindowProxyRegistry::instance().queryWindowsWithSystemWindowAndFocus().empty();
}

}

QOhosFloatingWindow::QOhosFloatingWindow(QWindow *window)
: QOhosPlatformWindow(window)
{
}

QOhosFloatingWindow::~QOhosFloatingWindow() = default;

void QOhosFloatingWindow::setGeometry(const QRect &rect)
{
    // Qt's high-DPI layer is active (factor = screen DPR): the platform
    // window works in NATIVE pixels and Qt converts native<->logical at the
    // QWindow boundary. Store every incoming rect raw.
    const QRect &adjustedRect = rect;
    // ohshark diag
    if (FILE *df = fopen("/data/storage/el2/base/files/qt_debug.log", "a")) {
        fprintf(df, "[fw.setGeometry] win=%p '%s' rect=%d,%d %dx%d type=%d\n",
                (void *)window(),
                window() ? window()->metaObject()->className() : "?",
                adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height(),
                int(window()->type()));
        fclose(df);
    }
    auto *view = ownedViewOrNull();
    bool geometryControlledBySystem =
        !QOhosSettings::instance().isWindowPcModeEnabled()
        && view != nullptr
        && view->viewType() == QOhosView::ViewType::MainWindow;

    if (geometryControlledBySystem) {
        // Still apply the requested geometry to the system window below —
        // the app explicitly asked for this rect (e.g. fullscreen). Only
        // sync back when the system reports a change (handleNodeResizeEvent).
        QOhosPlatformWindow::setGeometry(adjustedRect);
    }

    auto *currentScreen = QOhosPlatformWindow::screen();
    QOhosPlatformWindow::setGeometry(adjustedRect);

    auto *targetScreen = QOhosPlatformWindow::screenForGeometry(adjustedRect.marginsAdded(frameMargins()));
    if (targetScreen != nullptr && targetScreen != currentScreen)
        QWindowSystemInterface::handleWindowScreenChanged(window(), targetScreen->screen());

    if (view != nullptr) {
        auto frameGeometry = adjustedRect.marginsAdded(frameMargins());
        if (window()->type() == Qt::Popup
            && view->viewType() == QOhosView::ViewType::SubWindow) {
            // Popup menus are composited into the main window's surface and
            // receive input through Qt's popup-event redirection from the
            // main window. A system subwindow with real bounds only stacks
            // its opaque white background over the composited content, and
            // no transparency API works for subwindows on this device — so
            // shrink it to an invisible 1x1 placeholder.
            view->setPosition(QPoint(0, 0));
            view->setSize(QSize(1, 1));
        } else {
            if (!qt_window_private(window())->positionAutomatic)
                view->setPosition(frameGeometry.topLeft());
            view->setSize(frameGeometry.size());
        }

        if (view->viewType() == QOhosView::ViewType::EmbeddedWindow)
            setWindowGeometryFromOhos(adjustedRect);
        else if (window()->isVisible()) {
            // Resizing can leave the last backing-store image stale (the new
            // size never repaints — expose events are deduplicated), so the
            // user sees content that does not match the widget positions.
            // Schedule an update at the new size.
            window()->requestUpdate();
        }
    }
}

void QOhosFloatingWindow::setVisible(bool visible)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark", "QOhosFloatingWindow::setVisible: ENTER vis=%{public}d", (int)visible);
    if (!visible)
        m_view->hide();
    else
        m_view->showImmediate();
    startAsyncWaitForNodeResizeIfNeeded();
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark", "QOhosFloatingWindow::setVisible: EXIT");
}

WId QOhosFloatingWindow::winId() const
{
    auto windowObjectName = window()->objectName();

    qOhosPrintfDebug(
        "%s: winId called for window named: \"%s\"",
        Q_FUNC_INFO,
        windowObjectName.toStdString().c_str());

    return reinterpret_cast<WId>(m_view->viewWindowId());
}

void QOhosFloatingWindow::raise()
{
    auto *view = ownedViewOrNull();
    if (view != nullptr) {
        view->raise();
        window()->requestUpdate();
    }
}

void QOhosFloatingWindow::lower()
{
    auto *view = ownedViewOrNull();
    if (view != nullptr) {
        view->lower();
        window()->requestUpdate();
    }
}

QOhosSurface *QOhosFloatingWindow::ownedSurfaceOrNull() const
{
    auto *view = ownedViewOrNull();
    return view != nullptr ? view->surfaceOrNull() : nullptr;
}

QOhosView *QOhosFloatingWindow::ownedViewOrNull() const
{
    return m_view.get();
}

void QOhosFloatingWindow::initialize()
{
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark", "QOhosFloatingWindow::initialize: ENTER");
    QOhosPlatformWindow::initialize();
    m_view = QOhosView::createForWindow(this, propertiesProvider());

    auto *qWindow = window();

    // HACK
    // There is no functionality to fetch the initial window status from ohos
    // We can only listen for the changes - we need to assume sane default for tablets
    // so assign the default most-likely status of the tablet here.
    if (m_view->viewType() == QOhosView::ViewType::MainWindow
        && !QOhosSettings::instance().isWindowPcModeEnabled()
        && !m_lastWindowStatusType.has_value()) {
        m_lastWindowStatusType = QOhosWindowProxy::WindowStatusType::FULL_SCREEN;
    }

    QObject::connect(
        m_view.get(), &QOhosView::externalContentInteractionDetected,
        qWindow, &QOhosPlatformWindow::closeAllActivePopups);

    QObject::connect(
        m_view.get(), &QOhosView::nodeAreaChanged, m_view.get(),
        [this](QArkUi::QQtEmbeddedWindowNode::NodeAreaInfo event) {
            handleNodeResizeEvent(event);
        });

    QObject::connect(
        m_view.get(), &QOhosView::windowEvent,
        qWindow,
        [this](QOhosWindowProxy::WindowEvent evt) { handleWindowEvent(evt); });

    QObject::connect(
        m_view.get(), &QOhosView::windowStatusChange,
        qWindow,
        [this](QOhosWindowProxy::WindowStatus evt) { handleWindowStatusChange(evt); });

    QObject::connect(
        m_view.get(), &QOhosView::windowVisibilityChange,
        qWindow,
        [this](bool visible) { handleWindowVisibilityChange(visible); });

    bool monitorAvoidAreaChange =
        !(isHandheldDeviceType() && m_view->viewType() == QOhosView::ViewType::SubWindow);

    if (monitorAvoidAreaChange) {
        QObject::connect(
            m_view.get(), &QOhosView::avoidAreaChanged,
            qWindow,
            [this](QOhosWindowProxy::AvoidAreaType avoidAreaType,
                   const QOhosWindowProxy::AvoidArea &systemAvoidArea) {
                handleAvoidAreaChanged(avoidAreaType, systemAvoidArea);
            });
    }

    QObject::connect(
        m_view.get(), &QOhosView::windowRectChangedInGlobalDisplay,
        qWindow,
        [this](const QOhosWindowProxy::RectChangeOptions &rectChangeOptions) {
            handleWindowRectChanged(rectChangeOptions);
        });

    QObject::connect(qWindow, &QWindow::modalityChanged, m_view.get(), &QOhosView::setModality);

    QObject::connect(
        m_view.get(), &QOhosView::surfaceStatusChanged,
        qWindow,
        [this](const std::optional<QSize> &optSurfaceSize) { handleSurfaceStatusChanged(optSurfaceSize); });

    QObject::connect(qWindow, &QWindow::windowTitleChanged, m_view.get(), &QOhosView::setTitle);

    QObject::connect(
        m_view.get(), &QOhosView::windowDisplayIdChanged,
        qWindow,
        [this](QOhosDisplayInfo::JsDisplayId displayId) { handleWindowDisplayIdChanged(displayId); });
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark", "QOhosFloatingWindow::initialize: EXIT");
}

void QOhosFloatingWindow::restoreWindowCurrentCursorIfNeeded()
{
    auto *view = ownedViewOrNull();
    if (view != nullptr && m_cursor.has_value())
        view->setCursor(m_cursor.value());
}

void QOhosFloatingWindow::onWindowFlagsChanged(
    Qt::WindowFlags previousWindowFlags, Qt::WindowFlags currentWindowFlags)
{
    auto *view = ownedViewOrNull();
    if (view == nullptr)
        return;

    view->handleWindowFlagsChange(previousWindowFlags, currentWindowFlags);
}

void QOhosFloatingWindow::onWindowStateChanged(
    Qt::WindowStates oldWindowState, Qt::WindowStates currentWindowState)
{
    auto *view = ownedViewOrNull();
    if (view == nullptr)
        return;

    view->handleWindowStateChange(oldWindowState, currentWindowState);
}

void QOhosFloatingWindow::internalHijackSystemFocusAsPopup()
{
    QWindowSystemInterface::handleFocusWindowChanged(window(), Qt::PopupFocusReason);
}

void QOhosFloatingWindow::focusHijackingPopupHidden()
{
    auto systemFocusedWindows = QWindowProxyRegistry::instance().queryWindowsWithSystemWindowAndFocus();
    QWindowSystemInterface::handleFocusWindowChanged(
        !systemFocusedWindows.empty() ? systemFocusedWindows.front() : nullptr,
        Qt::ActiveWindowFocusReason);
}

void QOhosFloatingWindow::setMask(const QRegion &region)
{
    m_windowMask = region;

    auto *view = ownedViewOrNull();
    if (view == nullptr)
        return;

    view->setWindowMask(QOhosWindowProxy::WindowMask{region});
}

bool QOhosFloatingWindow::startSystemMove()
{
    auto *view = ownedViewOrNull();
    if (view != nullptr)
        return view->startMoving();
    return false;
}

void QOhosFloatingWindow::requestActivateWindow()
{
    if (windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus) && window()->type() == Qt::Popup) {
        internalHijackSystemFocusAsPopup();
        return;
    }

    QOhosPlatformWindow::requestActivateWindow();
}

void QOhosFloatingWindow::handleWindowEvent(QOhosWindowProxy::WindowEvent evt)
{
    auto *qWindow = window();
    bool windowAcceptsFocusAndInput = checkWindowAcceptsFocus() && checkWindowAcceptsInput();
    auto previousWindowEventType = std::exchange(m_lastWindowEventType, evt.type);

    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "[fw.windowEvent] win=%{public}p type=%{public}d", (void *)qWindow, int(evt.type));

    if (evt.type != QOhosWindowProxy::WindowEventType::WINDOW_SHOWN)
        m_activationEventDeferredUntilWindowUnblocked.reset();

    switch (evt.type) {
    case QOhosWindowProxy::WindowEventType::WINDOW_ACTIVE:
    {
        QWindow *modalWindow = nullptr;
        if (QGuiApplicationPrivate::instance()->isWindowBlocked(qWindow, &modalWindow)
            && qWindow != modalWindow) {
            if (qWindow->isTopLevel())
                m_activationEventDeferredUntilWindowUnblocked = evt;
            modalWindow->requestActivate();
            return;
        }

        // FIXME - restore cursor set by the User every time window is activated.
        // Perform it because Ohos overrides it with default "standard arrow" cursor (when
        // window is deactivated) and does not bring selected cursor back (when window is
        // activated again). This bahaviour is Ohos platform issue.
        restoreWindowCurrentCursorIfNeeded();
        if (windowAcceptsFocusAndInput) {
            QWindowSystemInterface::handleFocusWindowChanged(qWindow, Qt::ActiveWindowFocusReason);
            notifyInputSystemsWindowActiveStatusChanged(true);
        }
        break;
    }
    case QOhosWindowProxy::WindowEventType::WINDOW_INACTIVE:
        if (!windowAcceptsFocusAndInput)
            break;
        if (shouldClearQtFocusOnSystemFocusLoss(qWindow) && !anyQtWindowHoldsSystemFocus())
            QWindowSystemInterface::handleFocusWindowChanged(nullptr, Qt::ActiveWindowFocusReason);
        notifyInputSystemsWindowActiveStatusChanged(false);
        break;
    case QOhosWindowProxy::WindowEventType::WINDOW_HIDDEN:
        if (!checkWindowAcceptsFocus() && QGuiApplicationPrivate::focus_window == qWindow)
            focusHijackingPopupHidden();
        setExposedFromOhos(false);
        break;
    case QOhosWindowProxy::WindowEventType::WINDOW_SHOWN:
        // HACK
        // This is a fix for the windows hidden using `QWidget::hide()`.
        // When a window is hidden and then bring back from the system Dock
        // Qt side keeps wrong (hidden) state. This causes different issues due to
        // improper state. To fix it - change visibility state based on the system
        // window state.
        if (previousWindowEventType == QOhosWindowProxy::WindowEventType::WINDOW_HIDDEN && !qWindow->isVisible())
            qWindow->setVisible(true);
        setExposedFromOhos(true);
        startAsyncWaitForNodeResizeIfNeeded();
        break;
    case QOhosWindowProxy::WindowEventType::WINDOW_DESTROYED:
        notifyWindowDestroyedFromOhos();
        return;
    }
}

void QOhosFloatingWindow::handleWindowStatusChange(QOhosWindowProxy::WindowStatus evt)
{
    auto *qWindow = window();
    qCDebug(
        QtForOhos,
        "Window status changed, window: %p(%s) status: %d",
        qWindow,
        qPrintable(qWindow->objectName()),
        evt.type);

    // Drop duplicate FULL_SCREEN notifications; MainWindow views in non-PC mode
    // are exempt because initialize() pre-seeds their status to FULL_SCREEN.
    if (m_lastWindowStatusType == evt.type
        && m_lastWindowStatusType == QOhosWindowProxy::WindowStatusType::FULL_SCREEN
        && !(m_view->viewType() == QOhosView::ViewType::MainWindow
             && !QOhosSettings::instance().isWindowPcModeEnabled())) {
        qCDebug(QtForOhos, "Window status is the same as the previous one");
        return;
    }

    Qt::WindowStates windowStatesToSet = windowStates();
    m_lastWindowStatusType = evt.type;

    switch (evt.type) {
    // NOTE - Qt::WindowFullScreen and Qt::WindowMaximized represent the exact same
    // OHOS Window state.
    case QOhosWindowProxy::WindowStatusType::FULL_SCREEN:
    case QOhosWindowProxy::WindowStatusType::MAXIMIZE:
        windowStatesToSet = m_view->isFullscreenImmersiveModeEnabled()
            ? Qt::WindowState::WindowFullScreen
            : Qt::WindowState::WindowMaximized;
        break;
    case QOhosWindowProxy::WindowStatusType::MINIMIZE:
        if (m_view->consumePendingSelfMinimizeEcho()) {
            // The window is hidden in Qt's view, so ignore this echo entirely.
            // Deliberately skip setWindowStateFromOhos() and the node-resize
            // reconciliation below: reporting WindowMinimized would cause a
            // spurious restore -> QShowEvent, and the reconciliation would apply
            // the minimized node's geometry to a window Qt still considers to be
            // at its normal (pre-hide) geometry. showImmediate() reconciles it
            // when the window is shown again.
            return;
        }
        windowStatesToSet.setFlag(Qt::WindowState::WindowMinimized);
        break;
    case QOhosWindowProxy::WindowStatusType::UNDEFINED:
    case QOhosWindowProxy::WindowStatusType::FLOATING:
    case QOhosWindowProxy::WindowStatusType::SPLIT_SCREEN:
        windowStatesToSet = Qt::WindowState::WindowNoState;
        break;
    }

    setWindowStateFromOhos(windowStatesToSet);

    // HACK
    // There is no functionality in OHOS to fetch the window status that would allow checking
    // the value while processing the WINDOW_SHOWN window event type. Therefore, the window
    // geometry view needs to be recalculated with any change.
    startAsyncWaitForNodeResizeIfNeeded();
}

void QOhosFloatingWindow::handleWindowVisibilityChange(bool visible)
{
    auto *qWindow = window();
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "[fw.visChange] win=%{public}p visible=%{public}d", (void *)qWindow, int(visible));
    qCDebug(
        QtForOhos,
        "Window %p(%s) visibility changed: %s",
        qWindow,
        qPrintable(qWindow->objectName()),
        visible ? "true" : "false");
    if (visible && m_windowMask.has_value())
        m_view->setWindowMask(QOhosWindowProxy::WindowMask{m_windowMask.value()});

    setExposedFromOhos(visible);
}

void QOhosFloatingWindow::handleAvoidAreaChanged(
    QOhosWindowProxy::AvoidAreaType avoidAreaType,
    const QOhosWindowProxy::AvoidArea &systemAvoidArea)
{
    const auto &cached = m_avoidAreaCache[avoidAreaType];

    bool actuallyChanged =
        cached.visible != systemAvoidArea.visible
        || cached.leftRect != systemAvoidArea.leftRect
        || cached.rightRect != systemAvoidArea.rightRect
        || cached.bottomRect != systemAvoidArea.bottomRect
        || cached.topRect != systemAvoidArea.topRect;

    if (actuallyChanged) {
        m_avoidAreaCache[avoidAreaType] = systemAvoidArea;
        qCDebug(QtForOhos) << "Avoid area changed:"
            << static_cast<int>(avoidAreaType)
            << "visible:" << systemAvoidArea.visible
            << "top:" <<  systemAvoidArea.topRect
            << "left:" <<  systemAvoidArea.leftRect
            << "right:" <<  systemAvoidArea.rightRect
            << "bottom:" <<  systemAvoidArea.bottomRect;
        startAsyncWaitForNodeResizeIfNeeded();
    }
}

void QOhosFloatingWindow::handleSurfaceStatusChanged(const std::optional<QSize> &optSurfaceSize)
{
    m_optLastSurfaceSize = optSurfaceSize;
    bool hasSurface = m_view->surfaceOrNull() != nullptr;
    if (m_view->viewType() == QOhosView::ViewType::EmbeddedWindow) {
        setExposedFromOhos(hasSurface);
    }

    if (hasSurface) {
        startAsyncWaitForNodeResizeIfNeeded();
        if (m_view->viewType() == QOhosView::ViewType::SubWindow
            || m_view->viewType() == QOhosView::ViewType::FloatWindow) {
            // The window's first paint after expose may have been flushed
            // before its per-window surface was adopted (the surface arrives
            // asynchronously); schedule a repaint so the content reaches the
            // new surface.
            window()->requestUpdate();
        }
    }
}

void QOhosFloatingWindow::handleWindowDisplayIdChanged(QOhosDisplayInfo::JsDisplayId displayId)
{
    setDisplayIdFromOhos(displayId);
    startAsyncWaitForNodeResizeIfNeeded();
}

void QOhosFloatingWindow::handleWindowRectChanged(
    const QOhosWindowProxy::RectChangeOptions &rectChangeOptions)
{
    bool needsCloseAllActivePopups = rectChangeOptions.reason == QOhosWindowProxy::RectChangeReason::DRAG_START;

    qCDebug(QtForOhos)
        << "windowRectChanged window:" << window()
        << "rect:" << rectChangeOptions.rect
        << "reason:" << static_cast<int>(rectChangeOptions.reason);

    startAsyncWaitForNodeResizeIfNeeded();

    if (isWindowRotatedByTabletScreenRotation(window(), rectChangeOptions))
        needsCloseAllActivePopups = true;

    if (needsCloseAllActivePopups)
        QOhosPlatformWindow::closeAllActivePopups();
}

bool QOhosFloatingWindow::windowEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowUnblocked) {
        auto deferredActivationEvent =
            std::exchange(m_activationEventDeferredUntilWindowUnblocked, std::nullopt);
        const QWindow *windowHoldingFocus = QGuiApplicationPrivate::focus_window;
        bool noVisibleWindowHoldsFocus =
            windowHoldingFocus == nullptr || !windowHoldingFocus->isVisible();
        if (deferredActivationEvent && noVisibleWindowHoldsFocus)
            handleWindowEvent(*deferredActivationEvent);
    }

    if (event->type() == QEvent::Timer) {
        auto *timerEvent = static_cast<QTimerEvent *>(event);
        if (m_view && timerEvent->timerId() == m_geometryChangeTimer.timerId()) {
            auto syntheticEvent = m_view->nodeAreaInfo();
            handleNodeResizeEvent(syntheticEvent);
        }
    }

    return QOhosPlatformWindow::windowEvent(event);
}

void QOhosFloatingWindow::startAsyncWaitForNodeResizeIfNeeded()
{
    constexpr auto geometryChangeEventTimeoutMs = std::chrono::milliseconds(16);

    if (!m_geometryChangeTimer.isActive()) {
        m_geometryChangeTimer.start(
            geometryChangeEventTimeoutMs.count(),
            Qt::PreciseTimer,
            window());
    }
}

void QOhosFloatingWindow::handleNodeResizeEvent(const QArkUi::QQtEmbeddedWindowNode::NodeAreaInfo &areaChangeEvent)
{
    m_geometryChangeTimer.stop();

    if (Q_UNLIKELY(!m_view))
        return;

    // The window is a normal decorated system window; when the user drags,
    // resizes, maximizes or restores it, the system rect must flow back into
    // Qt so the content renders correctly at the new geometry. Converting
    // the physical client rect to logical. A report equal to Qt's current
    // geometry is a no-op, so the sync converges instead of oscillating.
    if (m_view->viewType() == QOhosView::ViewType::MainWindow) {
        const auto viewGeom = m_view->viewGeometry();
        const QRect physicalClientRect = viewGeom.geometry;
        if (!physicalClientRect.isEmpty()) {
            const qreal dpr = window()->devicePixelRatio();
            const QRect logicalRect(
                qRound(physicalClientRect.x() / dpr),
                qRound(physicalClientRect.y() / dpr),
                qRound(physicalClientRect.width() / dpr),
                qRound(physicalClientRect.height() / dpr));
            const QRect qtGeom = window()->geometry();
            // Small rounding differences between the system-reported client
            // rect and Qt's frame-derived geometry would otherwise ping-pong
            // forever; only sync on real changes.
            if (qAbs(logicalRect.x() - qtGeom.x()) > 2
                || qAbs(logicalRect.y() - qtGeom.y()) > 2
                || qAbs(logicalRect.width() - qtGeom.width()) > 4
                || qAbs(logicalRect.height() - qtGeom.height()) > 4) {
                setWindowGeometryFromOhos(physicalClientRect);
            }
        }
    }

    updateSafeAreaMargins();
}

// Map display-space cutout rects to window-relative margins, each attributed to
// the nearest window edge.
static QMargins cutoutMarginsForWindow(const QList<QRect> &cutoutRects, const QRect &windowRect)
{
    QMargins margins;
    for (const QRect &cutout : cutoutRects) {
        if (cutout.isEmpty() || !cutout.intersects(windowRect))
            continue;
        const int distTop = cutout.top() - windowRect.top();
        const int distBottom = windowRect.bottom() - cutout.bottom();
        const int distLeft = cutout.left() - windowRect.left();
        const int distRight = windowRect.right() - cutout.right();
        int nearest = qMin(qMin(distTop, distBottom), qMin(distLeft, distRight));
        if (nearest == distTop)
            margins.setTop(qMax(margins.top(), cutout.bottom() + 1 - windowRect.top()));
        else if (nearest == distBottom)
            margins.setBottom(qMax(margins.bottom(), windowRect.bottom() + 1 - cutout.top()));
        else if (nearest == distLeft)
            margins.setLeft(qMax(margins.left(), cutout.right() + 1 - windowRect.left()));
        else
            margins.setRight(qMax(margins.right(), windowRect.right() + 1 - cutout.left()));
    }
    return margins;
}

void QOhosFloatingWindow::updateSafeAreaMargins()
{
    // The system bar and navigation indicator avoid areas are window-relative
    // and visibility-aware: they report zero when hidden (e.g. in fullscreen).
    static constexpr QOhosWindowProxy::AvoidAreaType kSafeAreaContributors[] = {
        QOhosWindowProxy::AvoidAreaType::TYPE_SYSTEM,
        QOhosWindowProxy::AvoidAreaType::TYPE_NAVIGATION_INDICATOR,
    };

    // Avoid-area sizes are relative to the full window; subtract the inset the
    // compositor already applies (frame vs. true on-screen content) so a normal
    // window reports no safe area, matching Android's max(0, inset - offset).
    // nodeScreenGeometryPixels() is the real content rect; drawableRect lies.
    const QRect frame = m_view->viewGeometry().frameGeometry;
    const QRect content = m_view->nodeScreenGeometryPixels();
    const int alreadyTop = content.top() - frame.top();
    const int alreadyLeft = content.left() - frame.left();
    const int alreadyRight = frame.right() - content.right();
    const int alreadyBottom = frame.bottom() - content.bottom();

    QMargins combined;
    for (auto type : kSafeAreaContributors) {
        const QMargins m = m_view->avoidAreaMargins(type);
        combined.setTop(qMax(combined.top(), qMax(0, m.top() - alreadyTop)));
        combined.setLeft(qMax(combined.left(), qMax(0, m.left() - alreadyLeft)));
        combined.setRight(qMax(combined.right(), qMax(0, m.right() - alreadyRight)));
        combined.setBottom(qMax(combined.bottom(), qMax(0, m.bottom() - alreadyBottom)));
    }

    // The display cutout is not in the avoid areas; fold it in from
    // getCutoutInfo() (fetched fresh as it rotates with the display) and report
    // it even in fullscreen, mapped against the content area.
    const QMargins cutout = cutoutMarginsForWindow(m_view->displayCutoutRects(), content);
    combined.setTop(qMax(combined.top(), cutout.top()));
    combined.setLeft(qMax(combined.left(), cutout.left()));
    combined.setRight(qMax(combined.right(), cutout.right()));
    combined.setBottom(qMax(combined.bottom(), cutout.bottom()));

    setSafeAreaMarginsFromOhos(combined);
}

QT_END_NAMESPACE
