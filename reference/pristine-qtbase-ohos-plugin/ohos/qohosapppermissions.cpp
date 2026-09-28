// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosapppermissions_p.h"
#include <qohosplugincore.h>
#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/private/qohoslogger_p.h>

QT_BEGIN_NAMESPACE

namespace QOhosAppPermissions {

namespace {

void tryGetBundleAccessTokenIdWithConsumer(
    QtOhos::JsState &jsState, QOhosConsumer<QtOhos::JsState &, QOhosOptional<int>> resultConsumer)
{
    auto bundleFlags = jsState.eval<QNapi::Number>(
        "@ohos.bundle.bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION");

    jsState.evalToPromiseOrRejectOnThrow(
        "@ohos.bundle.bundleManager.getBundleInfoForSelf(*)", {bundleFlags})
    .withContext(std::move(resultConsumer))
    .onThenWithContext([](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
        QNapi::Object bundleInfo = cbInfo.getFirstArg<QNapi::Object>(Q_FUNC_INFO);
        resultConsumer(
            cbInfo.jsState(),
            QOhosOptional<int>(bundleInfo.eval<QNapi::Number>("appInfo.accessTokenId")));
    })
    .onCatchWithContext([](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
        QtOhos::logJsCallbackError(cbInfo, "Got error from getBundleInfoForSelf()");
        resultConsumer(cbInfo.jsState(), makeEmptyQOhosOptional());
    });
}

void checkAppPermissionStatusGrantedWithConsumer(
    QtOhos::JsState &jsState, int bundleAccessToken, const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, bool> resultConsumer)
{
    jsState.evalToPromiseOrRejectOnThrow(
        "@ohos.abilityAccessCtrl.createAtManager().checkAccessToken(*)",
        {bundleAccessToken, permissionName})
    .withContext(std::move(resultConsumer))
    .onThenWithContext([](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
        auto status = cbInfo.getFirstArg<QNapi::Number>(Q_FUNC_INFO);
        auto permissionGrantedStatus = cbInfo.jsState().eval<QNapi::Number>(
            "@ohos.abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED");
        resultConsumer(cbInfo.jsState(), status == permissionGrantedStatus);
    })
    .onCatchWithContext([](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
        QtOhos::logJsCallbackError(cbInfo, "Got error from checkAccessToken()");
        resultConsumer(cbInfo.jsState(), false);
    });
}

}

void checkAppPermissionGrantedWithConsumer(
    QtOhos::JsState &jsState, const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, bool> resultConsumer)
{
    tryGetBundleAccessTokenIdWithConsumer(
        jsState,
        [permissionName, resultConsumer = std::move(resultConsumer)](
            QtOhos::JsState &jsState, QOhosOptional<int> bundleAccessTokenId) mutable {
            if (bundleAccessTokenId.hasValue()) {
                checkAppPermissionStatusGrantedWithConsumer(
                    jsState, bundleAccessTokenId.value(), permissionName,
                    std::move(resultConsumer));
            } else {
                qOhosPrintfError(
                    "Cannot check permission: '%s': cannot get bundle access token id.",
                    permissionName.c_str());
                resultConsumer(jsState, false);
            }
        });
}

void requestAppPermissionFromUser(
    QtOhos::JsState &jsState, const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, bool> resultConsumer)
{
    return requestAppPermissionFromUser(
        jsState, jsState.defaultQAbilityPeer(), permissionName,
        std::move(resultConsumer));
}

void requestAppPermissionFromUser(
    QtOhos::JsState &jsState, std::shared_ptr<QtOhos::QAbilityPeer> abilityPeer,
    const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, bool> resultConsumer)
{
    requestAppPermissionFromUserWithResult(
        jsState, abilityPeer, permissionName,
        [resultConsumer = std::move(resultConsumer)](QtOhos::JsState &jsState, QOhosAppPermissions::AppPermissionResult result) {
            resultConsumer(jsState, result.permissionGranted);
        });
}

void requestAppPermissionFromUserWithResult(
    QtOhos::JsState &jsState, const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, AppPermissionResult> resultConsumer)
{
    requestAppPermissionFromUserWithResult(
        jsState, jsState.defaultQAbilityPeer(), permissionName,
        std::move(resultConsumer));
}

void requestAppPermissionFromUserWithResult(
    QtOhos::JsState &jsState, std::shared_ptr<QtOhos::QAbilityPeer> abilityPeer,
    const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, AppPermissionResult> resultConsumer)
{
    jsState.evalToPromiseOrRejectOnThrow(
        "@ohos.abilityAccessCtrl.createAtManager().requestPermissionsFromUser(*)",
        {
            abilityPeer->qAbility().eval<QNapi::Object>("context"),
            QNapi::makeArray(jsState.env(), {permissionName})
        })
    .withContext(std::move(resultConsumer))
    .onThenWithContext(
        [permissionName](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
            QNapi::Object resultObj = cbInfo.getFirstArg<QNapi::Object>(Q_FUNC_INFO);

            auto resultPermissionsNames =
                QNapi::getArrayElements<std::vector<std::string>, QNapi::String>(
                    resultObj.get<QNapi::Array>("permissions"));
            auto resultAuthResults =
                QNapi::getArrayElements<std::vector<int>, QNapi::Number>(
                    resultObj.get<QNapi::Array>("authResults"));

            auto dialogShownResultsOrEmpty = QNapi::getOptionalPropOrEmpty<QNapi::Array>(resultObj, "dialogShownResults");
            auto dialogShownResultsVector =
                !dialogShownResultsOrEmpty.IsEmpty()
                    ? QNapi::getArrayElements<std::vector<bool>, QNapi::Boolean>(dialogShownResultsOrEmpty)
                    : std::vector<bool>();

            int permissionGrantedStatus = cbInfo.jsState().eval<QNapi::Number>(
                "@ohos.abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED");

            bool permissionGranted =
                resultPermissionsNames.size() == 1 && resultPermissionsNames.front() == permissionName
                && resultAuthResults.size() == 1 && resultAuthResults.front() == permissionGrantedStatus;

            bool dialogShown =
                dialogShownResultsVector.size() == 1
                    ? dialogShownResultsVector.front()
                    : false;

            resultConsumer(
                cbInfo.jsState(),
                AppPermissionResult{
                    .permissionGranted = permissionGranted,
                    .dialogShown = dialogShown,
                });
        })
    .onCatchWithContext(
        [](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
            QtOhos::logJsCallbackError(cbInfo, "Got error from requestPermissionsFromUser()");
            resultConsumer(
                cbInfo.jsState(),
                AppPermissionResult{
                    .permissionGranted = false,
                    .dialogShown = false,
                });
        });
}

void requestAppPermissionOnSetting(
    QtOhos::JsState &jsState, const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, bool> resultConsumer)
{
    requestAppPermissionOnSetting(
        jsState, jsState.defaultQAbilityPeer(), permissionName, std::move(resultConsumer));
}

void requestAppPermissionOnSetting(
    QtOhos::JsState &jsState, std::shared_ptr<QtOhos::QAbilityPeer> abilityPeer,
    const std::string &permissionName,
    QOhosConsumer<QtOhos::JsState &, bool> resultConsumer)
{
    jsState.evalToPromiseOrRejectOnThrow(
        "@ohos.abilityAccessCtrl.createAtManager().requestPermissionOnSetting(*)",
        {
            abilityPeer->qAbility().eval<QNapi::Object>("context"),
            QNapi::makeArray(jsState.env(), {permissionName})
        })
    .withContext(std::move(resultConsumer))
    .onThenWithContext(
        [permissionName](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
            QNapi::Array resultArray = cbInfo.getFirstArg<QNapi::Array>(Q_FUNC_INFO);

            auto resultAuthResults = QNapi::getArrayElements<std::vector<int>, QNapi::Number>(resultArray);

            int permissionGrantedStatus = cbInfo.jsState().eval<QNapi::Number>(
                "@ohos.abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED");

            bool permissionGranted = resultAuthResults.size() == 1 && resultAuthResults.front() == permissionGrantedStatus;

            resultConsumer(cbInfo.jsState(), permissionGranted);
        })
    .onCatchWithContext(
        [](const QtOhos::CallbackInfo &cbInfo, auto &resultConsumer) {
            QtOhos::logJsCallbackError(cbInfo, "Got error from requestPermissionOnSetting()");
            resultConsumer(cbInfo.jsState(), false);
        });
}

}

QT_END_NAMESPACE
