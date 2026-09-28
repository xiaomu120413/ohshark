// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qohosuiextensionplatformwindow.h>

#include <QtGui/private/qguiapplication_p.h>
#include <algorithm>
#include <memory>
#include <private/qhighdpiscaling_p.h>
#include <qarkui/qembeddedwindownode.h>
#include <qarkui/qqtembeddedwindownode.h>
#include <qarkui/qxcomponentregistry.h>
#include <qohosinputmethodeventhandler.h>
#include <qohosjsmain.h>
#include <qohosplatformintegration.h>
#include <qohosplatformwindow.h>
#include <qohosplugincore.h>
#include <qohosutils.h>
#include <qpa/qwindowsysteminterface.h>
#include <render/qohoswindowproxy.h>
#include <render/qwindowproxyregistry.h>
#include <render/qxcomponent.h>
#include <utility>

QT_BEGIN_NAMESPACE

namespace {

class UiExtensionStateTracker
{
public:
    void setUiExtensionRoot(QWindow *uiExtensionRootWindow);
    QWindow *uiExtensionRootOrNull() const;
    QWindow *uiExtensionRootOrFail() const;

private:
    QOhosOptional<QPointer<QWindow>> m_uiExtensionRoot;
};

static UiExtensionStateTracker uiExtensionStateTracker;

void UiExtensionStateTracker::setUiExtensionRoot(QWindow *uiExtensionRootWindow) 
{
    if (m_uiExtensionRoot.hasValue()) {
        qOhosReportFatalErrorAndAbort(
            "Attempted to set ui extension root window multiple times. This is not supported.");
    }
    m_uiExtensionRoot = uiExtensionRootWindow;
}

QWindow *UiExtensionStateTracker::uiExtensionRootOrNull() const
{
    return m_uiExtensionRoot.valueOr(nullptr);
}

QWindow *UiExtensionStateTracker::uiExtensionRootOrFail() const
{
    auto *extensionRoot = uiExtensionRootOrNull();
    if (extensionRoot == nullptr) {
        qOhosReportFatalErrorAndAbort("UiExtension root window was empty.");
    }
    return extensionRoot;
}

QOhosUiExtensionPlatformWindow *getOrCreatePlatformParent(QWindow *window)
{
    auto *platformWindow = window->handle();
    if (platformWindow == nullptr) {
        window->winId();
    }
    return static_cast<QOhosUiExtensionPlatformWindow *>(window->handle());
}

template<typename ...Args>
std::function<void(Args...)> makeConditionalCallback(
    std::function<bool()> predicate,
    QOhosUiExtensionPlatformWindow &platformWindow,
    void (QOhosUiExtensionPlatformWindow::*platformWindowMemberFunc)(Args ...))
{
    return [predicate = std::move(predicate), platformWindow = &platformWindow, platformWindowMemberFunc](Args... args) {
        if (predicate()) {
            ((*platformWindow).*platformWindowMemberFunc)(args...);
        }
    };
}

QNapi::Object getUiExtensionContentSessionWindowProxy(std::shared_ptr<QtOhos::QAbilityPeer> qAbilityPeer)
{
    auto uiExtensionAbilityPeer = QtOhos::QUiExtensionAbilityPeer::tryCastFromQAbilityPeerOrNull(qAbilityPeer);
    if (!uiExtensionAbilityPeer) {
        qOhosReportFatalErrorAndAbort(
            "%s: QAbility with instance id: \"%s\" is not ui extension ability",
            Q_FUNC_INFO, qAbilityPeer->instanceId().c_str());
    }
    return uiExtensionAbilityPeer->uiExtensionContentSession().call<QNapi::Object>("getUIExtensionWindowProxy");
}

}

QOhosUiExtensionPlatformWindow::ViewTypeInfo QOhosUiExtensionPlatformWindow::ViewTypeInfo::createForUiExtensionRoot(const std::string &qAbilityInstanceId)
{
    return ViewTypeInfo(ViewType::UiExtensionRoot, nullptr, makeQOhosOptional(qAbilityInstanceId));
}

QOhosUiExtensionPlatformWindow::ViewTypeInfo
QOhosUiExtensionPlatformWindow::ViewTypeInfo::createForSubWindowOrFail(QWindow *logicalParent)
{
    if (logicalParent == nullptr) {
        qOhosReportFatalErrorAndAbort("%s: logicalParent must not be null. This is a programming error.", Q_FUNC_INFO);
    }
    return ViewTypeInfo(ViewType::SubWindow, logicalParent, makeEmptyQOhosOptional());
}

QOhosUiExtensionPlatformWindow::ViewTypeInfo
QOhosUiExtensionPlatformWindow::ViewTypeInfo::createForEmbeddedWindowOrFail(QWindow *logicalParent)
{
    if (logicalParent == nullptr) {
        qOhosReportFatalErrorAndAbort("%s: logicalParent must not be null. This is a programming error.", Q_FUNC_INFO);
    }
    return ViewTypeInfo(ViewType::EmbeddedWindow, logicalParent, makeEmptyQOhosOptional());
}

QOhosUiExtensionPlatformWindow::ViewType
QOhosUiExtensionPlatformWindow::ViewTypeInfo::viewType() const
{
    return m_viewType;
}

QWindow *QOhosUiExtensionPlatformWindow::ViewTypeInfo::logicalParentOrFail() const
{
    if (m_optLogicalParent == nullptr) {
        qOhosReportFatalErrorAndAbort("LogicalParent was not set");
    }
    return m_optLogicalParent;
}

QWindow *QOhosUiExtensionPlatformWindow::ViewTypeInfo::logicalParentOrNull() const
{
    return m_optLogicalParent;
}

std::string QOhosUiExtensionPlatformWindow::ViewTypeInfo::qAbilityInstanceIdOrFail() const
{
    if (!m_optQAbilityInstanceId.hasValue()) {
        qOhosReportFatalErrorAndAbort("QAbilityInstanceId has no value");
    }
    return m_optQAbilityInstanceId.value();
}

QOhosUiExtensionPlatformWindow::ViewTypeInfo::ViewTypeInfo(
    ViewType viewType, QWindow *optLogicalParent, QOhosOptional<std::string> optQAbilityInstanceId)
    : m_viewType(viewType)
    , m_optLogicalParent(optLogicalParent)
    , m_optQAbilityInstanceId(optQAbilityInstanceId)
{
}

QOhosUiExtensionPlatformWindow::QOhosUiExtensionPlatformWindow(QWindow *window)
    : QOhosPlatformWindow(window)
{
}

void QOhosUiExtensionPlatformWindow::initialize()
{
    QOhosPlatformWindow::initialize();

    auto *qWindow = window();

    QNativeNode::CreateInfo nativeNodeCreateInfo {
        .geometry = qWindow->geometry(),
        .window = qWindow,
    };

    QtOhos::runInJsThreadAndWait(
        [&](QtOhos::JsState &) {
            m_jsScopeData = QtOhos::makeProxyWithJsThreadDeleter(std::make_shared<JsScopeData>());
        },
        Q_FUNC_INFO);

    m_nativeNode = std::make_shared<QNativeNode>(nativeNodeCreateInfo);
    m_nativeNode->setNodeAreaChangeHandler(
        [this](QArkUi::QQtEmbeddedWindowNode::NodeAreaInfo event) {
            if (viewType() != ViewType::EmbeddedWindow) {
                setWindowGeometryFromOhos(event.screenGeometryPixels);
            }
        });

    m_nativeNode->setNodeVisibilityChangeHandler(
        [this](bool visible) {
            if (viewType() == ViewType::SubWindow) {
                return;
            }

            setExposedFromOhos(visible);
        });

    m_nativeNode->setNodeFocusChangeHandler(
        [this](bool focused) {
            if (viewType() == ViewType::SubWindow) {
                return;
            }

            handleWindowFocusStateChangedFromOhos(focused);
        });

    QObject::connect(
        m_nativeNode.get(), &QNativeNode::surfaceStatusChanged, &m_qObjecContext,
        [this](const QOhosOptional<QSize> &) {
            auto *surface = m_nativeNode.get()->surfaceOrNull();
            bool hasSurface = surface != nullptr;

            if (hasSurface && isExposed()) {
                window()->requestUpdate();
            }
        });
}

void QOhosUiExtensionPlatformWindow::setGeometry(const QRect &geometry)
{
    switch (viewType()) {
    case ViewType::UiExtensionRoot:
        break;
    case ViewType::SubWindow:
        {
            QOhosPlatformWindow::setGeometry(geometry);
            auto targetGeometry = windowFrameGeometry();
            m_ohosWindowProxy->moveWindowToGlobal(targetGeometry.topLeft(), {});
            m_ohosWindowProxy->setSize(targetGeometry.size());
        }
        break;
    case ViewType::EmbeddedWindow:
        {
            QOhosPlatformWindow::setGeometry(geometry);
            auto targetGeometry = windowGeometry();
            auto scaledSize = QHighDpi::fromNative(targetGeometry.size(), screen()->pixelDensity());
            auto scaledPosition = QHighDpi::fromNative(targetGeometry.topLeft(), screen()->pixelDensity());
            m_nativeNode->setPosition(scaledPosition);
            m_nativeNode->setSize(scaledSize);
        }
        break;
    }
}

void QOhosUiExtensionPlatformWindow::setVisible(bool visible)
{
    if (!visible) {
        QtOhos::runInJsThreadAndWait(
            [&](QtOhos::JsState &) {
                m_jsScopeData->optQAbilityInstanceId.reset();
            },
            Q_FUNC_INFO);
        m_ohosWindowProxy.reset();
        m_nativeNode->setVisibility(false);
        return;
    }

    auto viewTypeInfo = determineViewTypeInfo();

    switch (viewTypeInfo.viewType()) {
    case ViewType::UiExtensionRoot:
        showAsUiExtensionRoot(viewTypeInfo.qAbilityInstanceIdOrFail());
        break;
    case ViewType::SubWindow:
        showAsSubWindow(viewTypeInfo.logicalParentOrFail());
        break;
    case ViewType::EmbeddedWindow:
        showAsEmbeddedWindow(viewTypeInfo.logicalParentOrFail());
        break;
    }

    onWindowShow(viewTypeInfo.viewType());

    m_optLogicalParent = viewTypeInfo.logicalParentOrNull();
    m_nativeNode->setVisibility(true);
}

QOhosSurface *QOhosUiExtensionPlatformWindow::ownedSurfaceOrNull() const
{
    return m_nativeNode->surfaceOrNull();
}

QOhosView *QOhosUiExtensionPlatformWindow::ownedViewOrNull() const
{
    return nullptr;
}

void QOhosUiExtensionPlatformWindow::showAsUiExtensionRoot(const std::string &qAbilityInstanceId)
{
    std::shared_ptr<QXComponentNode> rootNode;

    auto callbackReceiverRef = QtOhos::makeQThreadSafeRef(&m_qObjecContext);
    QtOhos::runInJsThreadAndWait(
        [&](QtOhos::JsState &jsState) {
            auto nodeXComponentId = QXComponentId::createForNativeNodeUiExtensionWindow(qAbilityInstanceId); 
            auto rootNodeXComponent = QArkUi::QXComponentRegistry::instance().tryTakeNodeByXComponentId(nodeXComponentId);

            if (!rootNodeXComponent.hasValue()) {
                qOhosReportFatalErrorAndAbort(
                    "Failed to find root xComponent to attach to. Expected xComponent with id: %s",
                    nodeXComponentId.stringId().c_str());
            }
            m_jsScopeData->optQAbilityInstanceId = qAbilityInstanceId;
            m_jsScopeData->uiExtensionRootNode = std::make_shared<QXComponentNode>(rootNodeXComponent.value());
            auto qAbilityPeer = jsState.tryGetQAbilityPeerByInstanceId(qAbilityInstanceId);
            if (!qAbilityPeer) {
                qOhosReportFatalErrorAndAbort("%s: no QAbilityPeer with id '%s'", Q_FUNC_INFO, qAbilityInstanceId.c_str());
            }
            auto uiExtensionWindowProxy = getUiExtensionContentSessionWindowProxy(qAbilityPeer);
            rootNode = m_jsScopeData->uiExtensionRootNode;

            uiExtensionWindowProxy.call(
                "on",
                {
                    "rectChange",
                    jsState.eval<QNapi::Number>("@ohos.arkui.uiExtension.RectChangeReason.HOST_WINDOW_RECT_CHANGE"),
                    [this, callbackReceiverRef](const QNapi::CallbackInfo &) {
                        callbackReceiverRef.visitInQtThreadIfAlive(
                            [this](QObject &) {
                                handleUiExtensionRootWindowRectChangeEventFromOhos();
                            });
                    }
                });
        },
        Q_FUNC_INFO);

    m_nativeNode->setParent(rootNode);
    m_nativeNode->fillToParent();
}

void QOhosUiExtensionPlatformWindow::showAsEmbeddedWindow(QWindow *logicalParent)
{
    auto *parent = getOrCreatePlatformParent(logicalParent);
    m_nativeNode->setParent(*parent->m_nativeNode);
    auto targetGeometry = windowGeometry();
    m_nativeNode->setPosition(targetGeometry.topLeft());
    m_nativeNode->setSize(targetGeometry.size());
}

void QOhosUiExtensionPlatformWindow::showAsSubWindow(QWindow *logicalParent)
{
    auto *qWindow = window();
    auto *platformParent = getOrCreatePlatformParent(logicalParent);
    bool showAsUiExtensionSubWindow = platformParent->isUiExtensionRootWindow();
    auto optQAbilityInstanceId = platformParent->tryGetQAbilityInstanceId();
    if (!optQAbilityInstanceId.hasValue()) {
        qOhosReportFatalErrorAndAbort("%s: Failed to determine qAbilityInstanceId", Q_FUNC_INFO);
    }

    QOhosWindowProxy::SubWindowCreateInfo subWindowCreateInfo {
        .window = QtOhos::QObjectThreadSafeRef(qWindow),
        .windowId = internalWindowId(),
        .qAbilityInstanceId = optQAbilityInstanceId.value(),
        .windowTitle = qWindow->title().toStdString(),
        .windowRect = geometry().marginsAdded(frameMargins()),
        .decorEnabled = decorationPreset() != DecorationPreset::Frameless,
        .disableWindowFocusableBeforeLoadContentHack = false,
        .modal = qWindow->isModal(),
    };

    auto windowProxy = showAsUiExtensionSubWindow
        ? QOhosWindowProxy::createUiExtensionSubWindow(subWindowCreateInfo)
        : platformParent->m_ohosWindowProxy->createSubWindow(subWindowCreateInfo);

    m_ohosWindowProxy =
        QtOhos::makeSharedPtrWithAttachedExtraData<QOhosWindowProxy>(
            windowProxy,
            QWindowProxyRegistry::instance().registerQWindowWithWindowProxy(qWindow, *windowProxy));

    auto weakWindowProxy = QtOhos::makeWeakPtr(m_ohosWindowProxy);
    auto shouldCallHandlePredicate = [weakWindowProxy, targetWindowProxyPtr = m_ohosWindowProxy.get()]() {
        auto windowProxy = weakWindowProxy.lock();
        return windowProxy != nullptr && windowProxy.get() == targetWindowProxyPtr;
    };

    m_ohosWindowProxy->setWindowCallbackReceiver(
        std::make_unique<QOhosWindowProxy::WindowCallbacks>(
            QOhosWindowProxy::WindowCallbacks {
                .onWindowEvent = makeConditionalCallback(
                    shouldCallHandlePredicate,
                    *this, &QOhosUiExtensionPlatformWindow::handleSubWindowEventFromOhos),
                .onWindowStatusChange = makeQOhosNoOpConsumer(),
                .onWindowVisibilityChange = makeQOhosNoOpConsumer(),
                .onTouchOutside = makeQOhosNoOpConsumer(),
                .onAvoidAreaChange = makeQOhosNoOpConsumer(),
                .onWindowRectChange = makeConditionalCallback(
                    shouldCallHandlePredicate,
                    *this, &QOhosUiExtensionPlatformWindow::handleSubWindowRectChangeEventFromOhos),
                .onWindowDisplayIdChange = makeQOhosNoOpConsumer(),
            }));

    auto targetGeometry = windowFrameGeometry();

    m_nativeNode->setParent(windowProxy->nodeXComponent());
    m_nativeNode->fillToParent();

    m_ohosWindowProxy->setSize(targetGeometry.size());
    m_ohosWindowProxy->moveWindowToGlobal(targetGeometry.topLeft(), {});
    m_ohosWindowProxy->showWindow(
        QOhosWindowProxy::ShowWindowOptions{
            .focusOnShow = shouldShowWindowWithoutActivating()
                ? makeQOhosOptional(false)
                : makeEmptyQOhosOptional(),
        });
}

QOhosOptional<std::string> QOhosUiExtensionPlatformWindow::tryGetQAbilityInstanceId() const
{
    return m_ohosWindowProxy
        ? makeQOhosOptional(m_ohosWindowProxy->qAbilityInstanceId())
        : m_optLogicalParent != nullptr
            ? static_cast<QOhosUiExtensionPlatformWindow *>(
                QOhosPlatformWindow::fromQWindow(m_optLogicalParent))->tryGetQAbilityInstanceId()
            : QtOhos::evalInJsThread(
                    [&](QtOhos::JsState &) {
                        return m_jsScopeData->optQAbilityInstanceId;
                    },
                    Q_FUNC_INFO);
}

WId QOhosUiExtensionPlatformWindow::winId() const
{
    return m_nativeNode->windowId();
}

void QOhosUiExtensionPlatformWindow::handleUiExtensionRootWindowRectChangeEventFromOhos()
{
    auto nodeGeometry = m_nativeNode->nodeScreenGeometryPixels();
    setWindowGeometryFromOhos(nodeGeometry);
}

void QOhosUiExtensionPlatformWindow::handleWindowFocusStateChangedFromOhos(bool focused)
{
    bool windowAcceptsFocus = checkWindowAcceptsFocus();
    if (!windowAcceptsFocus) {
        return;
    }

    auto windowStatesToSet = windowStates();

    if (focused) {
        QWindowSystemInterface::handleWindowActivated(window());
    } else if (QGuiApplicationPrivate::focus_window == window()) {
        QWindowSystemInterface::handleWindowActivated(nullptr, Qt::ActiveWindowFocusReason);
    }

    windowStatesToSet.setFlag(Qt::WindowState::WindowActive, focused);
    setWindowStateFromOhos(windowStatesToSet);

    notifyInputSystemsWindowActiveStatusChanged(focused);
}

void QOhosUiExtensionPlatformWindow::handleSubWindowEventFromOhos(QOhosWindowProxy::WindowEvent event)
{
    QOhosOptional<bool> exposed;
    QOhosOptional<bool> focused;
    switch (event.type) {
    case QOhosWindowProxy::WindowEventType::WINDOW_SHOWN:
        exposed = true;
        break;
    case QOhosWindowProxy::WindowEventType::WINDOW_ACTIVE:
        focused = true;
        break;
    case QOhosWindowProxy::WindowEventType::WINDOW_INACTIVE:
        focused = false;
        break;
    case QOhosWindowProxy::WindowEventType::WINDOW_HIDDEN:
    case QOhosWindowProxy::WindowEventType::WINDOW_DESTROYED:
        m_ohosWindowProxy.reset();
        notifyWindowDestroyedFromOhos();
        return;
    }

    if (exposed.hasValue()) {
        setExposedFromOhos(exposed.value());
    }

    if (focused.hasValue()) {
        handleWindowFocusStateChangedFromOhos(focused.value());
    }
}

void QOhosUiExtensionPlatformWindow::handleSubWindowRectChangeEventFromOhos(QOhosWindowProxy::RectChangeOptions event)
{
    bool shouldUpdateGeometry = event.reason == QOhosWindowProxy::RectChangeReason::MOVE;
    if (!shouldUpdateGeometry) {
        return;
    }

    setWindowGeometryFromOhos(m_nativeNode->nodeScreenGeometryPixels());
}

QOhosUiExtensionPlatformWindow::ViewType QOhosUiExtensionPlatformWindow::viewType() const
{
    if (m_ohosWindowProxy) {
        switch (m_ohosWindowProxy->windowProxyType()) {
        case WindowProxyType::FloatWindow:
        case WindowProxyType::MainWindow:
            qOhosReportFatalErrorAndAbort("%s: Window proxy reported unsupported window proxy type", Q_FUNC_INFO);
        case WindowProxyType::SubWindow:
            return ViewType::SubWindow;
        }
    }

    return isUiExtensionRootWindow()
        ? ViewType::UiExtensionRoot
        : ViewType::EmbeddedWindow;
}

QOhosInstanceUiExtensionPlatformWindow::QOhosInstanceUiExtensionPlatformWindow(QWindow *window)
    : QOhosUiExtensionPlatformWindow(window)
{
}

bool QOhosInstanceUiExtensionPlatformWindow::isUiExtensionRootWindow() const
{
    return uiExtensionStateTracker.uiExtensionRootOrNull() == window();
}

QOhosUiExtensionPlatformWindow::ViewTypeInfo
QOhosInstanceUiExtensionPlatformWindow::determineViewTypeInfo() const
{
    bool canAttachToUiExtensionRoot = uiExtensionStateTracker.uiExtensionRootOrNull() == nullptr;
    auto *qWindow = window();

    if (qWindow->parent() != nullptr) {
        return ViewTypeInfo::createForEmbeddedWindowOrFail(qWindow->parent());
    } else if (qWindow->transientParent() != nullptr) {
        return ViewTypeInfo::createForSubWindowOrFail(qWindow->transientParent());
    } else if (canAttachToUiExtensionRoot) {
        return ViewTypeInfo::createForUiExtensionRoot(
            QtOhos::evalInJsThread(
                [](QtOhos::JsState &jsState) {
                    return jsState.defaultQAbilityPeer()->instanceId();
                },
                Q_FUNC_INFO));
    } else {
        return ViewTypeInfo::createForSubWindowOrFail(uiExtensionStateTracker.uiExtensionRootOrFail());
    }
}

void QOhosInstanceUiExtensionPlatformWindow::onWindowShow(ViewType shownViewType)
{
    if (shownViewType == ViewType::UiExtensionRoot) {
        uiExtensionStateTracker.setUiExtensionRoot(window());
    }
}

QOhosBundleUiExtensionPlatformWindow::QOhosBundleUiExtensionPlatformWindow(QWindow *qWindow)
    : QOhosUiExtensionPlatformWindow(qWindow)
{
}

void QOhosBundleUiExtensionPlatformWindow::setQAbilityQWindowBindingKeyForQWindow(
    QWindow *qWindow, const std::string &qAbilityQWindowBindingKeyStr)
{
    qCDebug(QtForOhos)
        << Q_FUNC_INFO << "qWindow:" << qWindow
        << "qAbilityQWindowBindingKey:" << QString::fromStdString(qAbilityQWindowBindingKeyStr);

    QOhosBundleUiExtensionPlatformWindow::checkQWindowMayBeShownAsUiExtensionRootOrFail(qWindow);
    auto &context = contextInstance();
    auto qAbilityQWindowBindingKey = QAbilityQWindowBindingKey(qAbilityQWindowBindingKeyStr);
    context.setQAbilityQWindowBindingKeyForQWindow(qWindow, qAbilityQWindowBindingKey);
    if (context.hasInstanceStarted(qAbilityQWindowBindingKey)) {
        qCDebug(QtForOhos)
            << Q_FUNC_INFO << "window" << qWindow
            << "was already bound for qAbilityQWindowBindingKey"
            << QString::fromStdString(qAbilityQWindowBindingKey.value())
            << "showing it.";
        qWindow->show();
    }
}

void QOhosBundleUiExtensionPlatformWindow::handleInstanceStarted(
    const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey, const std::string &qAbilityInstanceId)
{
    qCDebug(QtForOhos) << Q_FUNC_INFO << "qAbilityInstanceId:" << QString::fromStdString(qAbilityInstanceId);

    auto &context = contextInstance();
    context.addStartedInstanceOrFail(qAbilityQWindowBindingKey, qAbilityInstanceId);
    auto optBoundQWindow = context.tryFindQWindowForQAbilityQWindowBindingKey(qAbilityQWindowBindingKey);
    if (!optBoundQWindow.hasValue()) {
        return;
    }

    auto *qWindow = optBoundQWindow.value();
    if (qWindow == nullptr) {
        qOhosReportFatalErrorAndAbort(
            "%s: Error starting ability with instance id: \"%s\", QWindow was bound, but it was destroyed",
            Q_FUNC_INFO,
            qAbilityInstanceId.c_str());
    }
    QOhosBundleUiExtensionPlatformWindow::checkQWindowMayBeShownAsUiExtensionRootOrFail(qWindow);

    qCDebug(QtForOhos)
        << Q_FUNC_INFO << "showing window" << qWindow
        << "for qAbilityInstanceId" << QString::fromStdString(qAbilityInstanceId);
    qWindow->show();
}

bool QOhosBundleUiExtensionPlatformWindow::isUiExtensionRootWindow() const
{
    return contextInstance().tryFindQAbilityInstanceIdForQWindow(window()).hasValue();
}

QOhosUiExtensionPlatformWindow::ViewTypeInfo QOhosBundleUiExtensionPlatformWindow::determineViewTypeInfo() const
{
    auto *qWindow = window();
    auto rootWindowQAbilityInstanceId = contextInstance().tryFindQAbilityInstanceIdForQWindow(qWindow);

    if (rootWindowQAbilityInstanceId.hasValue()) {
        if (qWindow->parent() != nullptr) {
            qOhosReportFatalErrorAndAbort(
                "%s: QWindow marked as UiExtension root for instance: \"%s\" contains a parent."
                "This is not supported",
                Q_FUNC_INFO, rootWindowQAbilityInstanceId.value().c_str());
        }

        if (qWindow->transientParent() != nullptr) {
            qOhosReportFatalErrorAndAbort(
                "%s: QWindow marked as UiExtension root for instance: \"%s\" contains a transient parent."
                "This is not supported",
                Q_FUNC_INFO, rootWindowQAbilityInstanceId.value().c_str());
        }

        return ViewTypeInfo::createForUiExtensionRoot(rootWindowQAbilityInstanceId.value());
    }

    if (qWindow->parent() != nullptr) {
        return ViewTypeInfo::createForEmbeddedWindowOrFail(qWindow->parent());
    } else if (qWindow->transientParent() != nullptr) {
        return ViewTypeInfo::createForSubWindowOrFail(qWindow->transientParent());
    } else {
        qOhosReportFatalErrorAndAbort(
            "%s: QWindow is not bound, and has no parent. Don't know how to show it.", Q_FUNC_INFO);
    }
}

void QOhosBundleUiExtensionPlatformWindow::onWindowShow(ViewType)
{
}

void QOhosBundleUiExtensionPlatformWindow::checkQWindowMayBeShownAsUiExtensionRootOrFail(QWindow *qWindow)
{
    if (qWindow->isVisible()) {
        qOhosReportFatalErrorAndAbort(
            "%s: QWindow is already visible. It cannot be used as UiExtensionRoot", Q_FUNC_INFO);
    }

    if (qWindow->parent() != nullptr) {
        qOhosReportFatalErrorAndAbort(
            "%s: QWindow contains a parent. It cannot be used as UiExtension root.", Q_FUNC_INFO);
    }

    if (qWindow->transientParent() != nullptr) {
        qOhosReportFatalErrorAndAbort(
            "%s: QWindow contains a transient parent. It cannot be used as UiExtension root.", Q_FUNC_INFO);
    }
}

QOhosBundleUiExtensionPlatformWindow::Context &QOhosBundleUiExtensionPlatformWindow::contextInstance()
{
    static QOhosBundleUiExtensionPlatformWindow::Context context;
    return context;
}

QOhosBundleUiExtensionPlatformWindow::Context::Context() = default;

void QOhosBundleUiExtensionPlatformWindow::Context::setQAbilityQWindowBindingKeyForQWindow(
    QWindow *qWindow, const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey)
{
    auto &binding = m_qAbilityQWindowBindings[qAbilityQWindowBindingKey];

    if (binding.qWindow != nullptr && binding.qWindow != qWindow) {
        qWarning(
            "%s: Attempted to change QWindow binding for QAbilityQWindowBindingKey '%s': %p => %p, ignoring",
            Q_FUNC_INFO, qAbilityQWindowBindingKey.value().c_str(),
            binding.qWindow.data(), qWindow);
        return;
    }

    binding.qWindow = qWindow;
}

bool QOhosBundleUiExtensionPlatformWindow::Context::hasInstanceStarted(
    const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey) const
{
    auto qAbilityQWindowBindingsIt = m_qAbilityQWindowBindings.find(qAbilityQWindowBindingKey);
    return
        qAbilityQWindowBindingsIt != m_qAbilityQWindowBindings.end()
        && qAbilityQWindowBindingsIt->second.qAbilityInstanceId.hasValue();
}

QOhosOptional<std::string> QOhosBundleUiExtensionPlatformWindow::Context::tryFindQAbilityInstanceIdForQWindow(
    QWindow *qWindow) const
{
    auto qAbilityQWindowBindingsIt = std::find_if(
        m_qAbilityQWindowBindings.begin(), m_qAbilityQWindowBindings.end(),
        [&](const auto &qAbilityQWindowBindingPair) {
            return qAbilityQWindowBindingPair.second.qWindow == qWindow;
        });

    return qAbilityQWindowBindingsIt != m_qAbilityQWindowBindings.end()
        ? qAbilityQWindowBindingsIt->second.qAbilityInstanceId
        : makeEmptyQOhosOptional();
}

QOhosOptional<QWindow *> QOhosBundleUiExtensionPlatformWindow::Context::tryFindQWindowForQAbilityQWindowBindingKey(
    const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey) const
{
    auto qAbilityQWindowBindingsIt = m_qAbilityQWindowBindings.find(qAbilityQWindowBindingKey);
    if (qAbilityQWindowBindingsIt != m_qAbilityQWindowBindings.end()) {
        QWindow *qWindow = qAbilityQWindowBindingsIt->second.qWindow;
        return qWindow != nullptr
            ? makeQOhosOptional(qWindow)
            : makeEmptyQOhosOptional();
    }

    return {};
}

void QOhosBundleUiExtensionPlatformWindow::Context::addStartedInstanceOrFail(
    const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey, const std::string &qAbilityInstanceId)
{
    auto &binding = m_qAbilityQWindowBindings[qAbilityQWindowBindingKey];

    if (binding.qAbilityInstanceId.hasValue() && binding.qAbilityInstanceId != qAbilityInstanceId) {
        qOhosReportFatalErrorAndAbort(
            "%s: Attempted to add multiple instances with the same qAbilityQWindowBindingKey: %s",
            Q_FUNC_INFO, qAbilityQWindowBindingKey.value().c_str());
    }

    binding.qAbilityInstanceId = qAbilityInstanceId;
}

QT_END_NAMESPACE
