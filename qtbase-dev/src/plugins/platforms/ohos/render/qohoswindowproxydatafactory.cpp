// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <render/qohoswindowproxydatafactory.h>

#include <hilog/log.h>

#include <QtCore/qscopeguard.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>
#include <cstdint>
#include <qarkui/qxcomponentregistry.h>
#include <qohosinputmethodeventhandler.h>
#include <qohosplatformintegration.h>
#include <qohosutils.h>
#include <render/qohoswindowproxy.h>
#include <render/qxcomponent.h>

QT_BEGIN_NAMESPACE

namespace {

std::string makeOhosUniqueSystemWindowName(QtOhos::InternalWindowId internalWindowId)
{
    static std::uint64_t uniqueIdSuffixCounter = 0;
    auto result = QStringLiteral("QtWindow_%1_%2")
        .arg(internalWindowId.toString())
        .arg(QString::number(uniqueIdSuffixCounter));
    ++uniqueIdSuffixCounter;
    return result.toStdString();
}

struct OnWindowCreatedLoadWindowContentsContext
{
    bool disableWindowFocusableBeforeLoadContentHack;
    std::string contentPagePath;
    QNapi::Object localStorage;
};

struct SubWindowOptions
{
    std::string windowTitle;
    bool decorEnabled;
    bool isModal;
    QRect windowRect;
};

struct SubWindowOnAppearContext
{
    QXComponentId xComponentId;
    QNapi::Reference<QNapi::Object> localStorageObj;
    QNapi::Reference<QNapi::Object> windowObject;
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer;
    std::shared_ptr<QtOhos::QAbilityPeer> qAbilityPeer;
    WindowProxyType windowProxyType;
    QtOhos::QObjectThreadSafeRef owningQWindowRef;
};

struct LocalStorageForWindowCreateInfo
{
    QXComponentId xComponentId;
    QNapi::Object windowObject;
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer;
    std::shared_ptr<QtOhos::QAbilityPeer> qAbilityPeer;
    WindowProxyType windowProxyType;
    QtOhos::QObjectThreadSafeRef owningQWindowRef;
};

std::shared_ptr<QtOhos::QAbilityPeer> getQAbilityPeerByInstanceIdOrFail(
    QtOhos::JsState &jsState, const std::string &qAbilityInstanceId)
{
    auto qAbilityPeer = jsState.tryGetQAbilityPeerByInstanceId(qAbilityInstanceId);
    if (!qAbilityPeer) {
        qOhosReportFatalErrorAndAbort(
            "Failed to find QAbilityPeer for qAbilityInstanceId: %s", qAbilityInstanceId.c_str());
    }
    return qAbilityPeer;
}

QXComponentNode takeNodeXComponentFromRegistryOrFail(const QXComponentId &xComponentId)
{
    auto &registry = QArkUi::QXComponentRegistry::instance();
    auto nativeNodeXComponentOpt = registry.tryTakeNodeByXComponentId(xComponentId);
    if (!nativeNodeXComponentOpt.has_value()) {
        qOhosReportFatalErrorAndAbort(
            "Failed to fetch native node xcomponent with id: %s",
            xComponentId.stringId().c_str());
    }
    return nativeNodeXComponentOpt.value();
}

QNapi::Object makeLocalStorage(QtOhos::JsState &jsState)
{
    return jsState.eval<QNapi::Object>("LocalStorage.makeNewLocalStorage()");
}

QNapi::Object makeRectObject(Napi::Env env, const QRect &rect)
{
    return QNapi::makeObject(
        env,
        {
            {"left", rect.x()},
            {"top", rect.top()},
            {"width", rect.width()},
            {"height", rect.height()},
        });
}

QNapi::Promise createSubWindowWithOptions(
    QNapi::Object windowStageOrWindowObject,
    const std::string &windowName, const SubWindowOptions &subWindowOptions)
{
    auto subWindowOptionsObject =
        QNapi::makeObject(
            windowStageOrWindowObject.Env(),
            {
                {"title", subWindowOptions.windowTitle},
                {"decorEnabled", subWindowOptions.decorEnabled},
                {"isModal", subWindowOptions.isModal},
                {
                    "windowRect", makeRectObject(windowStageOrWindowObject.Env(), subWindowOptions.windowRect)
                }
            });

    return windowStageOrWindowObject.evalToPromiseOrRejectOnThrow(
        "createSubWindowWithOptions(*)",
        {windowName, subWindowOptionsObject});
}

// Optionless sub window creation, used on phones where the SessionManager-only
// createSubWindowWithOptions() is unavailable. WindowStage.createSubWindow() is @crossplatform.
QNapi::Promise createSubWindow(
    QNapi::Object windowStageOrWindowObject, const std::string &windowName)
{
    return windowStageOrWindowObject.evalToPromiseOrRejectOnThrow(
        "createSubWindow(*)", {windowName});
}

// createSubWindow() exists only on the WindowStage, not on a Window.
QNapi::Promise createSubWindowViaWindowStage(
    const std::shared_ptr<QtOhos::QAbilityPeer> &qAbilityPeer, const std::string &windowName)
{
    auto optQUiAbilityPeer = QtOhos::QUiAbilityPeer::tryCastFromQAbilityPeerOrNull(qAbilityPeer);
    if (!optQUiAbilityPeer) {
        qOhosReportFatalErrorAndAbort(
            "%s sub window creation requires an ability with a windowStage. Aborting...",
            Q_FUNC_INFO);
    }
    return createSubWindow(optQUiAbilityPeer->windowStage(), windowName);
}

// Query SystemCapability.Window.SessionManager (needed by createSubWindowWithOptions())
// directly rather than inferring it from the device type.
bool isWindowSessionManagerAvailable(QtOhos::JsState &jsState)
{
    try {
        return jsState.eval<QNapi::Boolean>(
            "Global.canIUse(*)",
            {std::string("SystemCapability.Window.SessionManager")}).Value();
    } catch (const Napi::Error &error) {
        qOhosPrintfWarning(
            "canIUse('SystemCapability.Window.SessionManager') query failed (%s); assuming the "
            "capability is unavailable and using createSubWindow()",
            error.what());
        return false;
    }
}

std::function<void(const QtOhos::CallbackInfo &)>
makeSubWindowOnAppearCallbackHandler(SubWindowOnAppearContext subWindowOnAppearCbCtx)
{
    auto sharedContext = QtOhos::moveToSharedPtr(std::move(subWindowOnAppearCbCtx));
    return [sharedContext](const QtOhos::CallbackInfo &cbInfo) mutable {
        if (!sharedContext)
            return;

        OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
            "[subwin] onAppear fired for xComponentId=%{public}s",
            sharedContext->xComponentId.stringId().c_str());

        auto xComponent = takeNodeXComponentFromRegistryOrFail(sharedContext->xComponentId);

        sharedContext->resultConsumer(
            cbInfo.jsState(),
            QOhosWindowProxyData {
                .qAbilityPeer = sharedContext->qAbilityPeer,
                .jsWindow = std::move(sharedContext->windowObject),
                .windowProxyType = sharedContext->windowProxyType,
                .nodeXComponent = std::make_shared<QXComponentNode>(xComponent),
                .jsKeepAliveData = QtOhos::moveToSharedPtr(std::move(sharedContext->localStorageObj)),
                .owningQWindowRef = sharedContext->owningQWindowRef,
            });

        sharedContext.reset();
    };
}

QNapi::Promise loadWindowContents(QNapi::Object window, QNapi::Object localStorage, const std::string &contentPagePath)
{
    return window.evalToPromiseOrRejectOnThrow("loadContent(*)", {contentPagePath, localStorage});
}

QNapi::Object makeLocalStorageForWindow(
    QtOhos::JsState &jsState, LocalStorageForWindowCreateInfo &&createInfo)
{
    auto *env = jsState.env();

    auto localStorage = makeLocalStorage(jsState);
    auto subWindowNativeNodeCreateInfo = QNapi::makeObject(
        env,
        {
            {"xComponentId", createInfo.xComponentId.toNapiValue(env)},
            // Render-target XComponent (SURFACE type) sibling in the page;
            // its surface is captured by QXComponentRegistry.
            {"surfaceXComponentId", createInfo.xComponentId.stringId() + "_surf"},
            // Wheel / touchpad-scroll axis events from the ArkTS XComponent's
            // universal onAxisEvent callback; forwarded to this subwindow's
            // owning Qt window as wheel events.
            {
                "onAxisEvent",
                [owningWindowRef = createInfo.owningQWindowRef](const QtOhos::CallbackInfo &cbInfo) {
                    QNapi::Number horizontalAxis;
                    QNapi::Number verticalAxis;
                    QNapi::Number windowX;
                    QNapi::Number windowY;
                    cbInfo.getLeadingArgs(
                        "onAxisEvent", horizontalAxis, verticalAxis, windowX, windowY);
                    const auto h = horizontalAxis.DoubleValue();
                    const auto v = verticalAxis.DoubleValue();
                    const auto x = windowX.DoubleValue();
                    const auto y = windowY.DoubleValue();
                    owningWindowRef.visitInQtThreadIfAlive(
                        [owningWindowRef, h, v, x, y](auto &) {
                            auto *qWindow = qobject_cast<QWindow *>(owningWindowRef.data());
                            if (qWindow == nullptr)
                                return;
                            auto *imeHandler =
                                QOhosPlatformIntegration::instance()->inputMethodEventHandler();
                            if (imeHandler != nullptr)
                                imeHandler->onAxisEventFromArkUi(qWindow, h, v, x, y);
                        });
                }
            },
            {
                "onDisAppear",
                [xComponentId = createInfo.xComponentId.stringId()]() {
                    qOhosPrintfDebug(
                        "WindowNativeNodeCreateInfo.onDisAppear() called from JS for xComponentId='%s'",
                        xComponentId.c_str());
                }
            },
            {
                "onAppear",
                makeSubWindowOnAppearCallbackHandler(SubWindowOnAppearContext {
                    .xComponentId = createInfo.xComponentId,
                    .localStorageObj = Napi::Persistent(localStorage),
                    .windowObject = Napi::Persistent(createInfo.windowObject),
                    .resultConsumer = std::move(createInfo.resultConsumer),
                    .qAbilityPeer = std::move(createInfo.qAbilityPeer),
                    .windowProxyType = createInfo.windowProxyType,
                    .owningQWindowRef = createInfo.owningQWindowRef,
                }),
            }
        });

    localStorage.eval("setOrCreate(*)", {"createInfo", subWindowNativeNodeCreateInfo});
    return localStorage;
}

QNapi::Promise onWindowCreatedLoadWindowContents(
    QtOhos::JsState &, const QNapi::Object &windowObject,
    OnWindowCreatedLoadWindowContentsContext context)
{
    struct LoadWindowContentsArgs
    {
        QNapi::Reference<QNapi::Object> window;
        QNapi::Reference<QNapi::Object> localStorage;
        std::string contentPagePath;
    };

    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "[subwin] calling loadContent page='%{public}s' focusableHack=%{public}d",
        context.contentPagePath.c_str(), context.disableWindowFocusableBeforeLoadContentHack ? 1 : 0);

    // Keep the window object alive across the promise chain.
    auto windowRef = std::make_shared<QNapi::Reference<QNapi::Object>>(
        Napi::Persistent(windowObject));
    auto localStorageRef = std::make_shared<QNapi::Reference<QNapi::Object>>(
        Napi::Persistent(context.localStorage));
    const std::string contentPagePath = context.contentPagePath;

    QNapi::Promise loadPromise =
        !context.disableWindowFocusableBeforeLoadContentHack
            ? loadWindowContents(windowObject, context.localStorage, contentPagePath)
            : windowObject.evalToPromiseOrRejectOnThrow("setWindowFocusable(*)", {false})
                  .onThen([windowRef, localStorageRef, contentPagePath]() {
                      return loadWindowContents(
                          windowRef->Value(), localStorageRef->Value(), contentPagePath);
                  });

    return loadPromise
        .onThen([windowRef]() {
            // The window was made non-focusable before loadContent to keep it
            // from stealing focus while hidden. Restore focusability once the
            // content is loaded: a non-focusable subwindow is also
            // input-transparent on this platform, so its ArkTS window event
            // filter never fires and the window never receives any touch or
            // mouse event. The window is shown later with focusOnShow=false,
            // which is what actually keeps it from stealing focus.
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "[subwin] content loaded, restoring window focusable");
            return windowRef->Value().evalToPromiseOrRejectOnThrow("setWindowFocusable(*)", {true});
        })
        .onFinally([]() {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "[subwin] loadContent promise settled");
        });
}

}

void makeWindowProxyDataForMainWindowInJsThread(
    QtOhos::JsState &jsState,
    const QOhosWindowProxyMainWindowCreateInfo &createInfo,
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer)
{
    auto optBaseQAbility = jsState.defaultQAbility();
    if (!optBaseQAbility)
        qOhosReportFatalErrorAndAbort("%s: can't create window without the default Ability instance", Q_FUNC_INFO);

    auto abilityStartupOptions =
        QNapi::makeObject(
            jsState.env(),
            {
                {"windowLeft", createInfo.frameGeometry.x()},
                {"windowTop", createInfo.frameGeometry.y()},
                {"windowWidth", createInfo.frameGeometry.width()},
                {"windowHeight", createInfo.frameGeometry.height()},
            });

    if (createInfo.fullscreen) {
        auto fullscreenWindowMode = jsState.eval<QNapi::Number>(
            "@ohos.app.ability.AbilityConstant.WindowMode.WINDOW_MODE_FULLSCREEN");
        abilityStartupOptions.set("windowMode", fullscreenWindowMode);
    }

    jsState.startNewQAbilityInstance(
        optBaseQAbility.value(), createInfo.qWindowRef,
        abilityStartupOptions,
        [qWindowRef = createInfo.qWindowRef,
            windowId = createInfo.windowId,
            resultConsumer = std::move(resultConsumer)](QtOhos::JsState &jsState, std::shared_ptr<QtOhos::QAbilityPeer> qAbilityPeer) {
                auto createInfo = QOhosWindowProxyExistingMainWindowCreateInfo {
                    .qWindowRef = qWindowRef,
                    .qAbilityInstanceId = qAbilityPeer->instanceId(),
                    .windowId = windowId,
                };

                makeWindowProxyDataForExistingMainWindowInJsThread(
                    jsState, createInfo, std::move(resultConsumer));
        });
}

void makeWindowProxyDataForExistingMainWindowInJsThread(
    QtOhos::JsState &jsState,
    const QOhosWindowProxyExistingMainWindowCreateInfo &createInfo,
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer)
{
    auto optQUiAbilityPeer = QtOhos::QUiAbilityPeer::tryCastFromQAbilityPeerOrNull(
        getQAbilityPeerByInstanceIdOrFail(jsState, createInfo.qAbilityInstanceId));
    if (!optQUiAbilityPeer) {
        qOhosReportFatalErrorAndAbort(
            "%s Attempting to make window proxy for main window for ability without windowStage. This is most likely a programming error. Aborting...",
            Q_FUNC_INFO);
    }
    optQUiAbilityPeer->setQWindow(jsState.env(), createInfo.qWindowRef);

    auto window = optQUiAbilityPeer->windowStage().eval<QNapi::Object>("getMainWindowSync()");
    auto nativeNodeXComponentId = QXComponentId::createForNativeNodeMainWindow(optQUiAbilityPeer->instanceId());
    auto nodeXComponent = takeNodeXComponentFromRegistryOrFail(nativeNodeXComponentId);

    resultConsumer(
        jsState,
        QOhosWindowProxyData {
            .qAbilityPeer = optQUiAbilityPeer,
            .jsWindow = Napi::Persistent(window),
            .windowProxyType = WindowProxyType::MainWindow,
            .nodeXComponent = std::make_shared<QXComponentNode>(nodeXComponent),
            .jsKeepAliveData = nullptr,
            .owningQWindowRef = createInfo.qWindowRef,
        });
}

void makeWindowProxyDataForSubWindowInJsThread(
    QtOhos::JsState &jsState,
    const QOhosWindowProxySubWindowCreateInfo &createInfo,
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer)
{
    auto qAbilityPeer = getQAbilityPeerByInstanceIdOrFail(jsState, createInfo.qAbilityInstanceId);
    auto optQUiAbilityPeer = QtOhos::QUiAbilityPeer::tryCastFromQAbilityPeerOrNull(qAbilityPeer);
    if (!optQUiAbilityPeer) {
        qOhosReportFatalErrorAndAbort(
            "%s Attempting to make window proxy for sub window for ability without windowStage. This is most likely a programming error. Aborting...",
            Q_FUNC_INFO);
    }
    makeWindowProxyDataForSubWindowInJsThread(
        jsState, optQUiAbilityPeer->windowStage(), createInfo, std::move(resultConsumer));
}

void makeWindowProxyDataForSubWindowInJsThread(
    QtOhos::JsState &jsState,
    QNapi::Object windowStageOrWindowObject,
    const QOhosWindowProxySubWindowCreateInfo &createInfo,
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer)
{
    auto xComponentId = QXComponentId::createForNativeNodeSubWindow(createInfo.windowId);
    struct Context
    {
        bool disableWindowFocusableBeforeLoadContentHack;
        QXComponentId xComponentId;
        std::shared_ptr<QtOhos::QAbilityPeer> qAbilityPeer;
        QtOhos::QObjectThreadSafeRef owningQWindowRef;
    };

    auto qAbilityPeer = getQAbilityPeerByInstanceIdOrFail(jsState, createInfo.qAbilityInstanceId);

    const auto windowName = makeOhosUniqueSystemWindowName(createInfo.windowId);

    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "[subwin] creation begin: windowId=%{public}s title='%{public}s' decor=%{public}d "
        "modal=%{public}d rect=[%{public}d,%{public}d %{public}dx%{public}d] name=%{public}s",
        createInfo.windowId.toString().toStdString().c_str(), createInfo.windowTitle.c_str(),
        createInfo.decorEnabled ? 1 : 0, createInfo.modal ? 1 : 0,
        createInfo.windowRect.x(), createInfo.windowRect.y(),
        createInfo.windowRect.width(), createInfo.windowRect.height(), windowName.c_str());

    // Prefer createSubWindowWithOptions() when the capability is present, but fall back to the
    // @crossplatform createSubWindow() when it is absent or fails (its dropped options are
    // applied separately after creation anyway) rather than aborting the application.
    auto subWindowCreationPromise = [&]() -> QNapi::Promise {
        if (!isWindowSessionManagerAvailable(jsState)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "[subwin] SessionManager unavailable, using createSubWindow()");
            return createSubWindowViaWindowStage(qAbilityPeer, windowName);
        }

        OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
            "[subwin] using createSubWindowWithOptions()");
        return createSubWindowWithOptions(
                   windowStageOrWindowObject,
                   windowName,
                   SubWindowOptions {
                       .windowTitle = createInfo.windowTitle,
                       .decorEnabled = createInfo.decorEnabled,
                       .isModal = createInfo.modal,
                       .windowRect = createInfo.windowRect,
                   })
            .onCatch([qAbilityPeer, windowName](const QtOhos::CallbackInfo &) -> QNapi::Promise {
                // Unsupported in some window contexts even when the capability is advertised.
                qOhosPrintfWarning(
                    "createSubWindowWithOptions() is unavailable in this context; "
                    "falling back to createSubWindow()");
                return createSubWindowViaWindowStage(qAbilityPeer, windowName);
            });
    }();

    subWindowCreationPromise
        .withContext(Context {
            .disableWindowFocusableBeforeLoadContentHack = createInfo.disableWindowFocusableBeforeLoadContentHack,
            .xComponentId = xComponentId,
            .qAbilityPeer = qAbilityPeer,
            .owningQWindowRef = createInfo.window,
        })
        .onThenWithContext([resultConsumer = std::move(resultConsumer)](const QtOhos::CallbackInfo &cbInfo, Context &context) mutable {
            auto windowObject = cbInfo.getFirstArg<QNapi::Object>(Q_FUNC_INFO);

            OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                "[subwin] subwindow object obtained, loading content page");

            auto localStorage = makeLocalStorageForWindow(
                cbInfo.jsState(),
                LocalStorageForWindowCreateInfo {
                    .xComponentId = context.xComponentId,
                    .windowObject = windowObject,
                    .resultConsumer = std::move(resultConsumer),
                    .qAbilityPeer = context.qAbilityPeer,
                    .windowProxyType = WindowProxyType::SubWindow,
                    .owningQWindowRef = context.owningQWindowRef,
                });

            return onWindowCreatedLoadWindowContents(
                cbInfo.jsState(), windowObject,
                OnWindowCreatedLoadWindowContentsContext {
                    .disableWindowFocusableBeforeLoadContentHack = context.disableWindowFocusableBeforeLoadContentHack,
                    .contentPagePath = "pages/SubWindowNativeNode",
                    .localStorage = localStorage,
                });
        })
        .onCatch([windowId = createInfo.windowId](const QtOhos::CallbackInfo &cbInfo) {
            QtOhos::logJsCallbackError(cbInfo, "sub window creation failed");
            OH_LOG_Print(LOG_APP, LOG_ERROR, 0x0500, "OhShark",
                "[subwin] CREATION FAILED for windowId=%{public}s",
                windowId.toStdString().c_str());
            qOhosReportFatalErrorAndAbort(
                "Failed to create subwindow for windowId='%s'",
                windowId.toStdString().c_str());
        });
}

void makeWindowProxyDataForFloatWindowInJsThread(
    QtOhos::JsState &jsState, const QOhosWindowProxyFloatWindowCreateInfo &createInfo,
    QOhosConsumer<QtOhos::JsState &, QOhosWindowProxyData> resultConsumer)
{
    auto xComponentId = QXComponentId::createForNativeNodeFloatWindow(createInfo.internalWindowId);
    auto qAbilityPeer = jsState.defaultQAbilityPeer();

    if (qAbilityPeer->qAbility().IsEmpty())
        qOhosReportFatalErrorAndAbort("%s: can't create window without the default Ability instance", Q_FUNC_INFO);

    auto configurationObject = QNapi::makeObject(
        jsState.env(),
        {
            // NOTE - The parameter name is misleading, what it refers to actually is (system) window id
            {"name", makeOhosUniqueSystemWindowName(createInfo.internalWindowId)},
            {"windowType", jsState.eval<QNapi::Number>("@ohos.window.WindowType.TYPE_FLOAT")},
            {"ctx", qAbilityPeer->qAbility().get<QNapi::Object>("context")},
        });

    struct Context
    {
        QXComponentId xComponentId;
        std::shared_ptr<QtOhos::QAbilityPeer> qAbilityPeer;
        QtOhos::QObjectThreadSafeRef owningQWindowRef;
    };

    jsState
        .evalToPromiseOrRejectOnThrow("@ohos.window.createWindow(*)", {configurationObject})
        .withContext(Context {
            .xComponentId = xComponentId,
            .qAbilityPeer = qAbilityPeer,
            .owningQWindowRef = createInfo.qWindowRef,
        })
        .onThenWithContext([resultConsumer = std::move(resultConsumer)](const QtOhos::CallbackInfo &cbInfo, Context &context) mutable {
            auto windowObject = cbInfo.getFirstArg<QNapi::Object>(Q_FUNC_INFO);

            auto localStorage = makeLocalStorageForWindow(
                cbInfo.jsState(),
                LocalStorageForWindowCreateInfo {
                    .xComponentId = context.xComponentId,
                    .windowObject = windowObject,
                    .resultConsumer = std::move(resultConsumer),
                    .qAbilityPeer = context.qAbilityPeer,
                    .windowProxyType = WindowProxyType::FloatWindow,
                    .owningQWindowRef = context.owningQWindowRef,
                });

            return onWindowCreatedLoadWindowContents(
                cbInfo.jsState(), windowObject,
                OnWindowCreatedLoadWindowContentsContext {
                    .disableWindowFocusableBeforeLoadContentHack = false,
                    .contentPagePath = "pages/FloatWindowNativeNode",
                    .localStorage = localStorage,
                });
        })
        .onCatch([internalWindowId = createInfo.internalWindowId](const QtOhos::CallbackInfo &cbInfo) {
            QtOhos::logJsCallbackError(cbInfo, "Failed to create TYPE_FLOAT window");
            qOhosReportFatalErrorAndAbort(
                "Failed to create TYPE_FLOAT window for windowId='%s'",
                internalWindowId.toStdString().c_str());
        });
}


QT_END_NAMESPACE
