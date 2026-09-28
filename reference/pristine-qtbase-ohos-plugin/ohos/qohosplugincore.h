// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSPLUGINCORE_H
#define QOHOSPLUGINCORE_H

#include <QtCore/private/qcore_ohos_p.h>
#include <QtCore/private/qnapi_p.h>
#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qobject.h>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <pthread.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

QT_BEGIN_NAMESPACE

namespace QtOhos {

class JsWindowsTracker
{
public:
    static void tagWindowAsClosing(QNapi::Object jsWindow, const char *logContext);
    static bool isWindowClosing(QNapi::Object jsWindow);

    JsWindowsTracker() = delete;
};

class QAbilityPeer
{
public:
    virtual ~QAbilityPeer();

    virtual std::string instanceId() = 0;
    virtual QNapi::Object uiContext() = 0;
    virtual QNapi::Object qAbility() = 0;
    virtual QNapi::Object launchWant() = 0;
    virtual QObjectThreadSafeRef qWindowRef() = 0;
    virtual QOhosOptional<QNapi::Promise> qWindowDestroyPromise() = 0;
    virtual std::shared_ptr<std::atomic_bool> destroyAllowedFlag() = 0;
    virtual bool isTerminating() = 0;

    virtual void setQWindow(Napi::Env env, QObjectThreadSafeRef qwindow) = 0;

    virtual void *tryCastWithTypeIdObject(const void *matchTypeIdObject) = 0;

protected:
    QAbilityPeer();
};

struct QAbilityInfo
{
    std::string name;
    std::string bundleName;
    std::string moduleName;
};

class QAbilityEngine
{
public:
    virtual ~QAbilityEngine();

    virtual QAbilityInfo readAbilityInfo(const QNapi::Object &ability) const = 0;

protected:
    QAbilityEngine();
};

class JsState;

class AppFunctions
{
public:
    virtual ~AppFunctions();

    virtual void startQAbilityInstance(
        QNapi::Object baseQAbility, QObjectThreadSafeRef qwindow,
        QNapi::Object optStartOptions,
        std::function<void(JsState &, std::shared_ptr<QAbilityPeer>)> startupNotifyFunc) = 0;

    virtual void startAppProcess(
        QNapi::Object baseQAbility, const std::string &processId, QNapi::Object want,
        QNapi::Object optStartOptions) = 0;

    virtual void startAppProcess(
        QNapi::Object baseQAbility, const std::string &processId, QNapi::Object want,
        QNapi::Object optStartOptions, std::function<void(JsState &)> continueFunc) = 0;

    virtual void startNoUiChildProcess(JsState &jsState, const std::string &libraryName, const std::vector<std::string> &args) = 0;

    virtual void tagWidgetOrWindowAsFloatWindow(QObject *widgetOrWindow, bool floatWindowEnabled) = 0;
};

enum class QtRunMode
{
    Normal,
    NoUiChildProcess,
    UiExtension,
    BundledUiExtension,
};

class JsState : public QOhosJsState
{
public:
    JsState(const JsState &) = delete;
    JsState &operator=(const JsState &) = delete;

    ~JsState() override;

    QT_DEPRECATED virtual QNapi::Object getModule(const std::string &moduleName) = 0;

    virtual QNapi::Object appLaunchWant() = 0;
    virtual QOhosOptional<QNapi::Object> optAppLaunchParam() = 0;

    virtual std::shared_ptr<QAbilityPeer> defaultQAbilityPeer() = 0;
    virtual std::shared_ptr<QAbilityPeer> tryGetQAbilityPeerByInstanceId(const std::string &instanceId) = 0;
    virtual std::shared_ptr<QAbilityPeer> tryGetQAbilityPeerByInstance(QNapi::Object qAbility) = 0;
    virtual std::shared_ptr<QAbilityPeer> tryGetQAbilityPeerByQWindow(QObjectThreadSafeRef qwindow) = 0;

    virtual void visitEachQAbilityPeer(const std::function<void(std::shared_ptr<QAbilityPeer>)> &visitor) = 0;

    virtual void startNewQAbilityInstance(
        std::shared_ptr<QAbilityPeer> baseQAbilityPeer, QObjectThreadSafeRef qwindow,
        QNapi::Object optStartOptions,
        std::function<void(JsState &, std::shared_ptr<QAbilityPeer>)> startupNotifyFunc) = 0;

    virtual void startAppProcess(
        const std::string &processId, QNapi::Object requestWant,
        QNapi::Object optStartOptions = {}) = 0;

    virtual void startAppProcess(
        const std::string &processId, QNapi::Object requestWant,
        QNapi::Object optStartOptions, std::function<void(JsState &)> continueFunc) = 0;

    virtual void addNewWantConsumer(QOhosConsumer<JsState &, QNapi::Object, QNapi::Object> wantConsumer) = 0;

    virtual void startNoUiChildProcess(const std::string &libraryName, const std::vector<std::string> &args) = 0;

    virtual QtRunMode qtRunMode() = 0;

    template<typename T>
    std::enable_if_t<std::is_default_constructible<T>::value, T> &getAttachedObjectWithLazyCreate();

    template<typename T>
    QNapi::Symbol getJsSymbolForType();

protected:
    JsState();

private:
    virtual void *getAttachedObjectWithLazyCreate(
        const std::type_info &objectTypeInfo, QOhosSupplier<std::shared_ptr<void>> objectFactory) = 0;

    virtual QNapi::Symbol getJsSymbolForType(const std::type_info &typeInfo) = 0;
};

enum class QOhosAbilityOnContinueResult
{
    AGREE,
    REJECT,
    MISMATCH,
};

class QUiAbilityPeer : public virtual QAbilityPeer, public std::enable_shared_from_this<QUiAbilityPeer>
{
public:
    static std::shared_ptr<QUiAbilityPeer> tryCastFromQAbilityPeerOrNull(std::shared_ptr<QAbilityPeer> qAbilityPeer);

    ~QUiAbilityPeer() override;

    virtual QNapi::Object launchParam() = 0;

    virtual QNapi::Object windowStage() = 0;
    virtual QNapi::Object window() = 0;

    virtual void setOnContinueRequestsHandler(
        std::function<void(JsState &, QNapi::Object, QOhosConsumer<JsState &, QOhosAbilityOnContinueResult>)> requestsHandler) = 0;

protected:
    QUiAbilityPeer();

private:
    static const nullptr_t typeIdObject;

    void *tryCastWithTypeIdObject(const void *matchTypeIdObject) final;
};

class QUiExtensionAbilityPeer : public virtual QAbilityPeer, public std::enable_shared_from_this<QUiExtensionAbilityPeer>
{
public:
    static std::shared_ptr<QUiExtensionAbilityPeer> tryCastFromQAbilityPeerOrNull(std::shared_ptr<QAbilityPeer> qAbilityPeer);

    virtual QNapi::Object uiExtensionContentSession() = 0;
    virtual QOhosOptional<std::string> tryGetQAbilityQWindowBindingKey() = 0;

    ~QUiExtensionAbilityPeer() override;

protected:
    QUiExtensionAbilityPeer();

private:
    static const nullptr_t typeIdObject;

    void *tryCastWithTypeIdObject(const void *matchTypeIdObject) final;
};

class CallbackInfo : public ::QOhosCallbackInfo
{
public:
    using ::QOhosCallbackInfo::QOhosCallbackInfo;

    JsState &jsState() const;
};

// this function should be called once from JS thread at some point during startup
void initJsThreadState(
    napi_env env, std::map<std::string, QNapi::Reference<QNapi::Function>> &&jsModulesFactories,
    std::shared_ptr<AppFunctions> appFunctions, QtRunMode qtRunMode);

// this function should be called from JS thread for each UIAbility when it's ready
void addJsQAbilityPeer(std::shared_ptr<QAbilityPeer> qAbilityPeer);

// this function should be called from JS thread when WindowStage of UIAbility is destroyed
void removeMatchingJsQAbilityPeer(QNapi::Object qAbility);

// this function should be called from JS thread when new Want object is received
void dispatchNewWant(QNapi::Object want, QNapi::Object launchParam);

// invokes the task inside the JS thread, can be called from Qt thread at any time
void invokeInJsThread(std::function<void(JsState &)> task);

// Invokes the task inside the JS thread and blocks the caller's thread until
// the promise (QOhosTaskPromise<>, passed as second argument to the task) is
// resolved on the JS side.
// It can be called from the Qt thread at any time, calling it from the JS
// thread is illegal.
void invokeInJsThreadAndWaitForContinue(
    std::function<void(JsState &, QOhosTaskPromise<>)> &&task,
    std::string callerContextName = {});

// Runs the task inside the JS thread and waits until its execution ends.
// When called from the JS thread, it calls the task directly. For other threads
// it behaves like a wrapper around the invokeInJsThreadAndWaitForContinue().
void runInJsThreadAndWait(
    const std::function<void(JsState &)> &task,
    std::string callerContextName = {});

template<typename Func>
auto evalInJsThread(Func &&func, std::string callerContextName = {}) -> decltype(func(std::declval<JsState &>()));

template<typename T>
T evalInJsThreadWithPromise(
    std::function<void(QtOhos::JsState &, QOhosTaskPromise<T>)> evalFunc,
    std::string callerContextName = {});

template<typename T>
T evalInJsThreadWithConsumer(std::function<void(QtOhos::JsState &, QOhosTaskPromise<T>)> evalFunc);

// Invokes the task inside the Qt thread and blocks the caller's thread until either:
//  - the "continue" function (std::function<void()>, passed as second argument to
//    the task) is called on the Qt side and returns,
//  - timeout occurs (we don't receive response from the finished task within the
//    specified time limit),
//  - deadlock is detected (both Qt and JS thread use synchronous calls at the same
//    time).
// It can be called from the JS thread. Calling it from the Qt thread is illegal.
//
// Returns true iff the caller receives confirmation about finishing the task within
// the time limit.
//
// Note:
// If the function returns false then:
//   - either the task wasn't started at all (timeout or deadlock)
//   - or the task was still running (and may be still running!) in the Qt thread
// after reaching the timeout.
Q_REQUIRED_RESULT bool tryInvokeInQtThreadAndTryWaitForContinue(
    std::function<void(std::function<void()>)> &&task,
    std::chrono::milliseconds timeout);

template<typename T>
std::enable_if_t<std::is_default_constructible<T>::value, T> &JsState::getAttachedObjectWithLazyCreate()
{
    auto *objectPtr = reinterpret_cast<T *>(
        getAttachedObjectWithLazyCreate(typeid(T), &std::make_shared<T>));
    return *objectPtr;
}

template<typename T>
QNapi::Symbol JsState::getJsSymbolForType()
{
    return getJsSymbolForType(typeid(T));
}

template<typename Func>
auto evalInJsThread(Func &&func, std::string callerContextName) -> decltype(func(std::declval<JsState &>()))
{
    return QOhosJsThreadGateway::eval(
        [&func](QOhosJsState &jsState) {
            return func(static_cast<JsState &>(jsState));
        },
        std::move(callerContextName));
}

template<typename T>
T evalInJsThreadWithPromise(
    std::function<void(QtOhos::JsState &, QOhosTaskPromise<T>)> evalFunc,
    std::string callerContextName)
{
    return QOhosJsThreadGateway::evalWithPromise<T>(
        [evalFunc = std::move(evalFunc)](QOhosJsState &jsState, QOhosTaskPromise<T> promise) {
            evalFunc(static_cast<JsState &>(jsState), std::move(promise));
        },
        std::move(callerContextName));
}

template<typename T>
T evalInJsThreadWithConsumer(std::function<void(QtOhos::JsState &, QOhosTaskPromise<T>)> evalFunc)
{
    return evalInJsThreadWithPromise<T>(std::move(evalFunc));
}

template<>
struct OhosEnumMeta<QOhosAbilityOnContinueResult>
{
    static constexpr const char *fullTypeName = "@ohos.app.ability.AbilityConstant.OnContinueResult";
    static constexpr std::array<std::pair<QOhosAbilityOnContinueResult, const char *>, 3> enumeratorsNames = {{
        {QOhosAbilityOnContinueResult::AGREE, "AGREE"},
        {QOhosAbilityOnContinueResult::REJECT, "REJECT"},
        {QOhosAbilityOnContinueResult::MISMATCH, "MISMATCH"},
    }};
};

}

QT_END_NAMESPACE

#endif
