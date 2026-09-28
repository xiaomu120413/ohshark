// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosaccessibilityprovider.h"

#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qglobal.h>
#include <accessibility/qohosaccessibilityarkuihelpers.h>
#include <arkui/native_interface_accessibility.h>
#include <memory>
#include <qarkui/qarkuiutils.h>
#include <qohosutils.h>
#include <type_traits>

#ifndef QT_NO_ACCESSIBILITY

namespace QOhos {

namespace {

Q_STATIC_ASSERT((std::is_same<AccessibilityNode::Id::ValueType, std::uint32_t>::value));

struct AccessibilityProviderContext
{
    std::shared_ptr<AccessibilityActionsConsumer> accessibilityActionsConsumerPtr;
    std::shared_ptr<AccessibilityTree> accessibilityTreePtr;
    std::shared_ptr<void> providerCallbacksHandle;
    std::shared_ptr<void> xComponentNodeRegistrationHandle;
};

class ProviderContextRegistry : public std::enable_shared_from_this<ProviderContextRegistry>
{
public:
    AccessibilityProviderContext *getContextWithInstanceIdOrNull(const std::string &instanceId) const;
    std::shared_ptr<void> registerContextWithInstanceIdOrFail(
        const std::string &instanceId, std::unique_ptr<AccessibilityProviderContext> providerCtx);

private:
    std::map<std::string, std::unique_ptr<AccessibilityProviderContext>> m_providersContexts;
};

AccessibilityProviderContext *ProviderContextRegistry::getContextWithInstanceIdOrNull(
    const std::string &instanceId) const
{
    auto matchingContextIter = m_providersContexts.find(instanceId);
    if (matchingContextIter == m_providersContexts.end()) {
        qOhosPrintfError(
            "%s: No provider context with given instance id: '%s' exists.",
            Q_FUNC_INFO, instanceId.c_str());
        return nullptr;
    }

    return matchingContextIter->second.get();
}

std::shared_ptr<void> ProviderContextRegistry::registerContextWithInstanceIdOrFail(
    const std::string &instanceId, std::unique_ptr<AccessibilityProviderContext> providerCtx)
{
    if (m_providersContexts.find(instanceId) != m_providersContexts.end()) {
        qOhosReportFatalErrorAndAbort(
            "%s: A provider context with provided instance id: '%s' has been already registered.", 
            Q_FUNC_INFO, instanceId.c_str());
    }

    m_providersContexts.emplace(instanceId, std::move(providerCtx));

    auto weakSelf = QtOhos::makeWeakPtr(shared_from_this());

    return QtOhos::makeDestroyNotifier(
        [weakSelf, instanceId]() {
            auto self = weakSelf.lock();
            if (self) {
                self->m_providersContexts.erase(instanceId);
            }
        });
}

std::shared_ptr<ProviderContextRegistry> getProviderContextRegistry()
{
    static std::shared_ptr<ProviderContextRegistry> providerCtxRegistry =
        std::make_shared<ProviderContextRegistry>();

    return providerCtxRegistry;
}

QOhosOptional<AccessibilityAction> tryMapNativeActionTypeToAccessibilityActionType(
    ::ArkUI_Accessibility_ActionType actionType)
{
    switch (actionType) {
    case ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_CLEAR_ACCESSIBILITY_FOCUS:
        return makeQOhosOptional(AccessibilityAction::ClearFocus);
    case ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_CLICK:
        return makeQOhosOptional(AccessibilityAction::Click);
    case ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_GAIN_ACCESSIBILITY_FOCUS:
        return makeQOhosOptional(AccessibilityAction::GainFocus);
    case ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_SCROLL_FORWARD:
        return makeQOhosOptional(AccessibilityAction::ScrollForward);
    case ::ARKUI_ACCESSIBILITY_NATIVE_ACTION_TYPE_SCROLL_BACKWARD:
        return makeQOhosOptional(AccessibilityAction::ScrollBackward);
    default:
        break;
    }
    return makeEmptyQOhosOptional();
}

QOhosOptional<AccessibilityNode::Id> tryConvertInstanceIdToValidAccessibilityId(
    const AccessibilityTree &accessibilityTree, const char *instanceId)
{
    auto parsedValue =
        QtOhos::tryParseStringAsUnsignedInteger<AccessibilityNode::Id::ValueType>(instanceId);

    if (!parsedValue.hasValue()) {
        qOhosPrintfError(
            "%s: cannot convert instance id (of value: '%s') to %s number.",
            Q_FUNC_INFO, instanceId, typeid(AccessibilityNode::Id::ValueType).name());
        return makeEmptyQOhosOptional();
    }

    auto maybeId = QOhos::tryMapNumericIdToValidNodeId(parsedValue.value());

    if (!maybeId.hasValue()) {
        qOhosPrintfError(
            "%s: cannot convert parsed value: %u to a valid node id.",
            Q_FUNC_INFO, parsedValue.value());
        return makeEmptyQOhosOptional();
    }

    if (!accessibilityTree.containsNode(maybeId.value())) {
        qOhosPrintfError(
            "%s: node tree doesn't contain a node with id: %u obtained from the parsed value.",
            Q_FUNC_INFO, maybeId.value().value());
        return makeEmptyQOhosOptional();
    }

    return maybeId;
}

void addNodeToElementList(
    const AccessibilityTree &accessibilityTree, AccessibilityNode::Id rootNodeId,
    ::ArkUI_AccessibilityElementInfoList *elementList, AccessibilityNode::Id nodeId)
{
    auto node = accessibilityTree.getNodeById(nodeId);
    auto *elementInfo = QArkUi::callArkUiOrFailOnNullResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AddAndGetAccessibilityElementInfo),
        elementList);

    fillAccessibilityElementInfoWithNodeState(
        rootNodeId, accessibilityTree, elementInfo, node);
}

void addNodesToElementList(
    const AccessibilityTree &accessibilityTree,
    AccessibilityNode::Id rootNodeId, ::ArkUI_AccessibilityElementInfoList *elementList,
    const std::set<AccessibilityNode::Id> &nodeIds)
{
    for (const auto &nodeId : nodeIds) {
        addNodeToElementList(accessibilityTree, rootNodeId, elementList, nodeId);
    }
}

void addChildNodesRecursively(
    const AccessibilityTree &accessibilityTree,
    AccessibilityNode::Id rootNodeId, ::ArkUI_AccessibilityElementInfoList *elementList,
    AccessibilityNode::Id nodeId)
{
    const auto childIds = accessibilityTree.getChildIds(nodeId);

    addNodesToElementList(accessibilityTree, rootNodeId, elementList, childIds);

    for (const auto &childId : childIds) {
        addChildNodesRecursively(accessibilityTree, rootNodeId, elementList, childId);
    }
}

void addParentNodeToElementList(
    const AccessibilityTree &accessibilityTree,
    AccessibilityNode::Id rootNodeId, ::ArkUI_AccessibilityElementInfoList *elementList,
    AccessibilityNode::Id nodeId)
{
    const auto node = accessibilityTree.getNodeById(nodeId);
    if (node.parentId.hasValue()) {
        addNodeToElementList(accessibilityTree, rootNodeId, elementList, node.parentId.value());
    }
}

std::set<AccessibilityNode::Id> getNodeSiblings(
    const AccessibilityTree &accessibilityTree, AccessibilityNode::Id nodeId)
{
    const auto node = accessibilityTree.getNodeById(nodeId);
    if (!node.parentId.hasValue()) {
        return {};
    }

    auto parentId = node.parentId.value();

    auto parentChildIds = accessibilityTree.getChildIds(parentId);
    parentChildIds.erase(nodeId);

    return parentChildIds;
}

void findAccessibilityNodeInfosByIdImpl(
    const AccessibilityTree &accessibilityTree,
    AccessibilityNode::Id rootNodeId, AccessibilityNode::Id nodeId,
    ::ArkUI_AccessibilitySearchMode mode, ::ArkUI_AccessibilityElementInfoList *elementList)
{
    addNodeToElementList(accessibilityTree, rootNodeId, elementList, nodeId);

    switch (mode) {
    case ::ARKUI_ACCESSIBILITY_NATIVE_SEARCH_MODE_PREFETCH_CURRENT:
        break;
    case ::ARKUI_ACCESSIBILITY_NATIVE_SEARCH_MODE_PREFETCH_PREDECESSORS:
        addParentNodeToElementList(accessibilityTree, rootNodeId, elementList, nodeId);
        break;
    case ::ARKUI_ACCESSIBILITY_NATIVE_SEARCH_MODE_PREFETCH_SIBLINGS:
        addNodesToElementList(
            accessibilityTree, rootNodeId, elementList, getNodeSiblings(accessibilityTree, nodeId));
        break;
    case ::ARKUI_ACCESSIBILITY_NATIVE_SEARCH_MODE_PREFETCH_CHILDREN:
        addNodesToElementList(
            accessibilityTree, rootNodeId, elementList, accessibilityTree.getChildIds(nodeId));
        break;
    case ::ARKUI_ACCESSIBILITY_NATIVE_SEARCH_MODE_PREFETCH_RECURSIVE_CHILDREN:
        addChildNodesRecursively(accessibilityTree, rootNodeId, elementList, nodeId);
        break;
    default:
        qOhosPrintfWarning("%s: mode %d not supported", Q_FUNC_INFO, mode);
    }
}

std::shared_ptr<void> makeXComponentNodeRegistrationHandle(
    std::shared_ptr<AccessibilityXComponentRegistry> accessibilityXComponentRegistry, 
    QXComponentRender xComponent,
    AccessibilityNode::Id windowAccessibilityId)
{
    accessibilityXComponentRegistry->registerNodeWithXComponent(windowAccessibilityId, xComponent);

    return QtOhos::makeDestroyNotifier(
        [accessibilityXComponentRegistry, windowAccessibilityId]() {
            accessibilityXComponentRegistry->unregisterNodeWithXComponent(windowAccessibilityId);
        });
}

std::int32_t findAccessibilityNodeInfosById(
    const char *instanceId, std::int64_t elementId, ::ArkUI_AccessibilitySearchMode mode,
    std::int32_t requestId, ::ArkUI_AccessibilityElementInfoList *elementList)
{
    auto *providerCtx = getProviderContextRegistry()->getContextWithInstanceIdOrNull(instanceId);
    if (providerCtx == nullptr) {
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }

    if (elementList == nullptr) {
        qOhosPrintfError(
            "%s: accessibility request id: %d - invalid elementList", Q_FUNC_INFO, requestId);
        return ::OH_NATIVEXCOMPONENT_RESULT_BAD_PARAMETER;
    }

    auto accessibilityRootId = tryConvertInstanceIdToValidAccessibilityId(
        *providerCtx->accessibilityTreePtr, instanceId);
    if (!accessibilityRootId.hasValue()) {
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }

    auto maybeNodeId = QOhos::tryMapOhosElementIdToNodeId(accessibilityRootId.value(), elementId);
    if (!maybeNodeId.hasValue() || !providerCtx->accessibilityTreePtr->containsNode(maybeNodeId.value())) {
        qOhosPrintfError(
            "%s: accessibility tree replication doesn't contain node: %d", Q_FUNC_INFO, maybeNodeId.value().value());
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }

    findAccessibilityNodeInfosByIdImpl(
        *providerCtx->accessibilityTreePtr, accessibilityRootId.value(), maybeNodeId.value(), mode,
        elementList);

    return ::OH_NATIVEXCOMPONENT_RESULT_SUCCESS;
}

std::int32_t findAccessibilityNodeInfosByText(
    const char *, std::int64_t, const char *, std::int32_t, ::ArkUI_AccessibilityElementInfoList *)
{
    return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

std::int32_t findFocusedAccessibilityNode(
    const char *, std::int64_t, ::ArkUI_AccessibilityFocusType, std::int32_t,
    ::ArkUI_AccessibilityElementInfo *)
{
    return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

std::int32_t findNextFocusAccessibilityNode(
    const char *, std::int64_t, ::ArkUI_AccessibilityFocusMoveDirection, std::int32_t,
    ::ArkUI_AccessibilityElementInfo *)
{
    return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

std::int32_t executeAccessibilityAction(
    const char *instanceId, std::int64_t elementId, ::ArkUI_Accessibility_ActionType action,
    ::ArkUI_AccessibilityActionArguments *, std::int32_t)
{
    auto *providerCtx = getProviderContextRegistry()->getContextWithInstanceIdOrNull(instanceId);
    if (providerCtx == nullptr) {
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }

    auto accessibilityRootId = tryConvertInstanceIdToValidAccessibilityId(
        *providerCtx->accessibilityTreePtr, instanceId);
    if (!accessibilityRootId.hasValue()) {
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }

    auto maybeNodeId = QOhos::tryMapOhosElementIdToNodeId(accessibilityRootId.value(), elementId);
    if (!maybeNodeId.hasValue()) {
        qOhosPrintfError(
            "%s: Unknown node id. Omitting dispatch of action with code: %d.",
            Q_FUNC_INFO, action);
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }
    auto maybeAccessibilityAction = tryMapNativeActionTypeToAccessibilityActionType(action);
    if (!maybeAccessibilityAction.hasValue()) {
        qOhosPrintfError(
            "%s: Unknown action. Omitting dispatch of action with code: %d.",
            Q_FUNC_INFO, action);
        return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
    }
    providerCtx->accessibilityActionsConsumerPtr->dispatchAction(
        maybeNodeId.value(), maybeAccessibilityAction.value());

    return ::OH_NATIVEXCOMPONENT_RESULT_SUCCESS;
}

std::int32_t clearFocusedFocusAccessibilityNode(const char *)
{
    return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

std::int32_t getAccessibilityNodeCursorPosition(
    const char *, std::int64_t, std::int32_t, std::int32_t *)
{
    return ::OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

}

std::shared_ptr<void> tryInitializeAccessibilityProvider(
    QXComponentRender xComponent, std::shared_ptr<AccessibilityTree> accessibilityTree,
    std::shared_ptr<AccessibilityXComponentRegistry> accessibilityXComponentRegistry,
    std::shared_ptr<AccessibilityActionsConsumer> accessibilityActionsConsumer,
    AccessibilityNode::Id windowAccessibilityId)
{
    auto providerCtx = std::make_unique<AccessibilityProviderContext>();

    auto *accessibilityProvider = getNativeAccessibilityProviderFromXComponentOrFail(xComponent);

    auto accessibilityProviderCallbacks = std::make_shared<::ArkUI_AccessibilityProviderCallbacksWithInstance>(
        ::ArkUI_AccessibilityProviderCallbacksWithInstance {
            .findAccessibilityNodeInfosById = &findAccessibilityNodeInfosById,
            .findAccessibilityNodeInfosByText = &findAccessibilityNodeInfosByText,
            .findFocusedAccessibilityNode = &findFocusedAccessibilityNode,
            .findNextFocusAccessibilityNode = &findNextFocusAccessibilityNode,
            .executeAccessibilityAction = &executeAccessibilityAction,
            .clearFocusedFocusAccessibilityNode = &clearFocusedFocusAccessibilityNode,
            .getAccessibilityNodeCursorPosition = &getAccessibilityNodeCursorPosition,
        });

    auto instanceId = std::to_string(windowAccessibilityId.value());
    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityProviderRegisterCallbackWithInstance),
        instanceId.c_str(), accessibilityProvider, accessibilityProviderCallbacks.get());

    providerCtx->accessibilityTreePtr = accessibilityTree;
    providerCtx->accessibilityActionsConsumerPtr = accessibilityActionsConsumer;
    providerCtx->providerCallbacksHandle = accessibilityProviderCallbacks;
    providerCtx->xComponentNodeRegistrationHandle = makeXComponentNodeRegistrationHandle(
        accessibilityXComponentRegistry, xComponent, windowAccessibilityId);

    return getProviderContextRegistry()->registerContextWithInstanceIdOrFail(
        instanceId, std::move(providerCtx));
}

}

#endif // QT_NO_ACCESSIBILITY
