// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <accessibility/qohosaccessibilityarkuihelpers.h>
#include <limits>
#include <qarkui/qarkuiutils.h>

namespace QOhos {

namespace {

constexpr std::int32_t parentOfRootId = -2100000;

const auto minAllowedNodeId = AccessibilityNode::Id(
    std::numeric_limits<std::uint32_t>::max() - std::numeric_limits<std::int32_t>::max());

void setElementInfoParentId(
    AccessibilityNode::Id accessibilityRootId,
    ::ArkUI_AccessibilityElementInfo *elementInfo, const AccessibilityNode &node)
{
    if (accessibilityRootId == node.id) {
        QArkUi::callArkUiOrFailOnErrorResult(
            Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetParentId),
            elementInfo, parentOfRootId);
    } else if (node.parentId.hasValue()) {
        auto parentOhosElementId = mapNodeIdToOhosElementId(node.parentId.value()).value();
        QArkUi::callArkUiOrFailOnErrorResult(
            Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetParentId),
            elementInfo, parentOhosElementId);
    }
}


::ArkUI_AccessibleRect getArkUIAccessibleRect(const QRect &nodeGeometry)
{
    return ::ArkUI_AccessibleRect {
        .leftTopX = nodeGeometry.topLeft().x(),
        .leftTopY = nodeGeometry.topLeft().y(),
        .rightBottomX = nodeGeometry.x() + nodeGeometry.width(),
        .rightBottomY = nodeGeometry.y() + nodeGeometry.height(),
    };
}

QOhosOptional<::ArkUI_AccessibleRangeInfo> tryMapNodeValueInfoToArkUiRangeInfo(
    const AccessibilityNode::ValueInfo &valueInfo)
{
    if (!valueInfo.minMaxRange.hasValue()) {
        return {};
    }

    auto optMin = QtOhos::tryParseStringAsFiniteDouble(valueInfo.minMaxRange.value().first);
    auto optMax = QtOhos::tryParseStringAsFiniteDouble(valueInfo.minMaxRange.value().second);
    auto optCurrent = QtOhos::tryParseStringAsFiniteDouble(valueInfo.current);

    return optMin.hasValue() && optMax.hasValue() && optCurrent.hasValue()
        ? makeQOhosOptional(
            ::ArkUI_AccessibleRangeInfo {
               .min = optMin.value(),
               .max = optMax.value(),
               .current = optCurrent.value(),
            })
        : makeEmptyQOhosOptional();
}

void setElementInfoActions(
    ::ArkUI_AccessibilityElementInfo *elementInfo, const AccessibilityNode &node)
{
    std::vector<::ArkUI_AccessibleAction> actions;

    if (node.clickable) {
        actions.push_back({
            .actionType = ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_CLICK,
            .description = "",
        });
    }

    if (node.focusable) {
        actions.push_back({
            .actionType = ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_GAIN_ACCESSIBILITY_FOCUS,
            .description = "",
        });

        actions.push_back({
            .actionType = ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_CLEAR_ACCESSIBILITY_FOCUS,
            .description = "",
        });
    }

    if (node.scrollable) {
        actions.push_back({
            .actionType = ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_SCROLL_FORWARD,
            .description = "",
        });

        actions.push_back({
            .actionType = ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_SCROLL_BACKWARD,
            .description = "",
        });
    }

    if (!actions.empty()) {
        QArkUi::callArkUiOrFailOnErrorResult(
            Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetOperationActions),
            elementInfo, actions.size(), actions.data());
    }
}

}

::ArkUI_AccessibilityProvider *getNativeAccessibilityProviderFromXComponentOrFail(
    QXComponentRender xComponent)
{
    ::ArkUI_AccessibilityProvider *accessibilityProvider = nullptr;
    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_NativeXComponent_GetNativeAccessibilityProvider),
        xComponent.handle(), &accessibilityProvider);
    if (accessibilityProvider == nullptr) {
        qOhosReportFatalErrorAndAbort("Got null ArkUI_AccessibilityProvider");
    }

    return accessibilityProvider;
}

QOhosOptional<AccessibilityNode::Id> tryMapOhosElementIdToNodeId(
    AccessibilityNode::Id accessibilityRootId, std::int64_t ohosElementId)
{
    if (ohosElementId == -1) {
        return makeQOhosOptional(accessibilityRootId);
    } else if (ohosElementId < 0 || ohosElementId > std::numeric_limits<std::int32_t>::max()) {
        return makeEmptyQOhosOptional();
    } else {
        return makeQOhosOptional(
            AccessibilityNode::Id(
                minAllowedNodeId.value() + static_cast<AccessibilityNode::Id::ValueType>(ohosElementId)));
    }
}

QOhosOptional<AccessibilityNode::Id> tryMapNumericIdToValidNodeId(
    AccessibilityNode::Id::ValueType value)
{
    auto id = AccessibilityNode::Id(value);
    return !(id < minAllowedNodeId) ? makeQOhosOptional(id) : makeEmptyQOhosOptional();
}

OhosElementId mapNodeIdToOhosElementId(AccessibilityNode::Id id)
{
    if (id < minAllowedNodeId) {
        qOhosReportFatalErrorAndAbort(
            "%s: encountered AccessibilityNode::Id with illegal value: %u",
            Q_FUNC_INFO, id.value());
    }

    return OhosElementId(id.value() - minAllowedNodeId.value());
}

void fillAccessibilityElementInfoWithNodeState(
    AccessibilityNode::Id accessibilityRootId, const AccessibilityTree &accessibilityTree,
    ::ArkUI_AccessibilityElementInfo *elementInfo, const AccessibilityNode &node)
{
    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetElementId),
        elementInfo, mapNodeIdToOhosElementId(node.id).value());

    setElementInfoParentId(accessibilityRootId, elementInfo, node);

    auto rect = getArkUIAccessibleRect(node.geometry);
    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetScreenRect),
        elementInfo, &rect);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetComponentType),
        elementInfo, QArkUi::CZString(node.componentType.c_str()));

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetEnabled),
        elementInfo, node.enabled);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetVisible),
        elementInfo, node.visible);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetAccessibilityText),
        elementInfo, QArkUi::CZString(node.name.c_str()));

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetHintText),
        elementInfo, QArkUi::CZString(node.help.c_str()));

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetAccessibilityDescription),
        elementInfo, QArkUi::CZString(node.description.c_str()));

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetContents),
        elementInfo, QArkUi::CZString(node.valueInfo.current.c_str()));

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetChecked),
        elementInfo, node.checked);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetFocused),
        elementInfo, node.focused);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetSelected),
        elementInfo, node.selected);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetIsPassword),
        elementInfo, node.passwordEdit);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetEditable),
        elementInfo, node.editable);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetClickable),
        elementInfo, node.clickable);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetCheckable),
        elementInfo, node.checkable);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetFocusable),
        elementInfo, node.focusable);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetScrollable),
        elementInfo, node.scrollable);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetBackgroundColor),
        elementInfo, QArkUi::CZString(node.backgroundColor.c_str()));

    auto rangeInfo = tryMapNodeValueInfoToArkUiRangeInfo(node.valueInfo);
    if (rangeInfo.hasValue()) {
        QArkUi::callArkUiOrFailOnErrorResult(
            Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetRangeInfo),
            elementInfo, &rangeInfo.value());
    }

    const auto *accessibilityLevel =
        node.visible
            ? node.focusable
                ? "yes"
                : "auto"
            : "no-hide-descendants";
    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetAccessibilityLevel),
        elementInfo, accessibilityLevel);

    auto childIds = accessibilityTree.getChildIds(node.id);
    if (!childIds.empty()) {
        std::vector<std::int64_t> ohosChildIdsForCall;
        for (const auto &childId : childIds) {
            ohosChildIdsForCall.push_back(mapNodeIdToOhosElementId(childId).value());
        }
        QArkUi::callArkUiOrFailOnErrorResult(
            Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityElementInfoSetChildNodeIds),
            elementInfo, ohosChildIdsForCall.size(), ohosChildIdsForCall.data());
    }

    setElementInfoActions(elementInfo, node);
}

}
