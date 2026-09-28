// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosaccessibilitytree.h"

#include <QtCore/private/qohoslogger_p.h>
#include <QtGui/qaccessible.h>
#include <QtGui/qguiapplication.h>
#include <accessibility/qohosaccessibilityarkuihelpers.h>
#include <accessibility/qohosaccessibilitytreeeventsconsumercommand.h>
#include <accessibility/qohosaccessiblewrappers.h>
#include <qarkui/qarkuiutils.h>
#include <qohosplugincore.h>
#include <qohosutils.h>
#include <render/qohosbatchingrequestshandler.h>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

bool isAccessibilityOpen()
{
    return QtOhos::evalInJsThread(
        [](QtOhos::JsState &jsState) -> bool {
            return jsState.eval<QNapi::Boolean>("@ohos.accessibility.isOpenAccessibilitySync()");
        },
        Q_FUNC_INFO);
}

template<typename T, typename CreateInvoker, typename DestroyInvoker>
std::shared_ptr<T> makeSharedFromNativeCreateFuncOrFail(
    QOhosNamedFunc<T *(*)(), CreateInvoker> namedCreateFunc,
    QOhosNamedFunc<void (*)(T *), DestroyInvoker> namedDestroyFunc)
{
    return std::shared_ptr<T>(
        QArkUi::callArkUiOrFailOnNullResult(namedCreateFunc),
        [namedDestroyFunc](T *objNativePtr) {
            QArkUi::callArkUi(namedDestroyFunc, objNativePtr);
        });
}

std::shared_ptr<ArkUI_AccessibilityEventInfo> makeEventInfoFromNode(
    AccessibilityNode::Id rootNodeId, const AccessibilityTree &accessibilityTree,
    ::ArkUI_AccessibilityEventType eventType, const AccessibilityNode &node)
{
    auto eventInfo = makeSharedFromNativeCreateFuncOrFail(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_CreateAccessibilityEventInfo),
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_DestoryAccessibilityEventInfo));

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityEventSetEventType),
        eventInfo.get(), eventType);

    auto elementInfo = makeSharedFromNativeCreateFuncOrFail(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_CreateAccessibilityElementInfo),
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_DestoryAccessibilityElementInfo));

    fillAccessibilityElementInfoWithNodeState(
        rootNodeId, accessibilityTree, elementInfo.get(), node);

    QArkUi::callArkUiOrFailOnErrorResult(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_AccessibilityEventSetElementInfo),
        eventInfo.get(), elementInfo.get());

    return eventInfo;
}

::ArkUI_AccessibilityEventType mapAccessibilityEventToOhosAccessibilityEvent(
    AccessibilityEvent eventType)
{
    switch (eventType) {
    case AccessibilityEvent::ElementScrolled:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_SCROLLED;
    case AccessibilityEvent::ElementClicked:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_CLICKED;
    case AccessibilityEvent::ElementFocusCleared:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_ACCESSIBILITY_FOCUS_CLEARED;
    case AccessibilityEvent::ElementFocused:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_ACCESSIBILITY_FOCUSED;
    case AccessibilityEvent::FocusedElementUpdated:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_FOCUS_NODE_UPDATE;
    }

    qOhosReportFatalErrorAndAbort(
        "%s: cannot find a proper mapping. This shouldn't have happened!", Q_FUNC_INFO);
}

::ArkUI_AccessibilityEventType mapAccessibilityWindowEventToOhosAccessibilityEvent(
    AccessibilityWindowEvent eventType)
{
    switch (eventType) {
    case AccessibilityWindowEvent::WindowStateUpdated:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_PAGE_STATE_UPDATE;
    case AccessibilityWindowEvent::WindowContentUpdated:
        return ::ARKUI_ACCESSIBILITY_NATIVE_EVENT_TYPE_PAGE_CONTENT_UPDATE;
    }

    qOhosReportFatalErrorAndAbort(
        "%s: cannot find a proper mapping. This shouldn't have happened!", Q_FUNC_INFO);
}

std::string mapAccessibilityEventToString(AccessibilityEvent eventType)
{
    switch (eventType) {
    case AccessibilityEvent::ElementScrolled:
        return "ElementScrolled";
    case AccessibilityEvent::ElementClicked:
        return "ElementClicked";
    case AccessibilityEvent::ElementFocusCleared:
        return "ElementFocusCleared";
    case AccessibilityEvent::ElementFocused:
        return "ElementFocused";
    case AccessibilityEvent::FocusedElementUpdated:
        return "FocusedElementUpdated";
    }

    qOhosReportFatalErrorAndAbort(
        "%s: cannot find a proper mapping. This shouldn't have happened!", Q_FUNC_INFO);
}

std::string mapAccessibilityWindowEventToString(AccessibilityWindowEvent eventType)
{
    switch (eventType) {
    case AccessibilityWindowEvent::WindowStateUpdated:
        return "WindowStateUpdated";
    case AccessibilityWindowEvent::WindowContentUpdated:
        return "WindowContentUpdated";
    }

    qOhosReportFatalErrorAndAbort(
        "%s: cannot find a proper mapping. This shouldn't have happened!", Q_FUNC_INFO);
}

void setNodeState(AccessibilityNode &node, QAccessible::State state)
{
    node.enabled = !static_cast<bool>(state.disabled);
    node.visible = !static_cast<bool>(state.invisible);
    node.checked = static_cast<bool>(state.checked);
    node.focused = static_cast<bool>(state.focused);
    node.selected = static_cast<bool>(state.selected);
    node.passwordEdit = static_cast<bool>(state.passwordEdit);
    node.editable = static_cast<bool>(state.editable);
}

void removeRedundantGeometryCommandsFromSequenceTail(
    std::vector<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> &sequence)
{
    static constexpr std::size_t tailSize = 4;

    if (sequence.size() < tailSize) {
        return;
    }

    auto tail = makeQOhosSpan(sequence.data() + sequence.size() - tailSize, tailSize);

    bool eligableForRemoval = 
        tail[0]->getType() == AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeGeometry
        && tail[1]->getType() == AccessibilityTreeEventsConsumerCommand::Type::ProcessAccessibilityWindowEvent
        && tail[1]->getOptWindowEventType() == AccessibilityWindowEvent::WindowStateUpdated
        && tail[2]->getType() == AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeGeometry
        && tail[3]->getType() == AccessibilityTreeEventsConsumerCommand::Type::ProcessAccessibilityWindowEvent
        && tail[3]->getOptWindowEventType() == AccessibilityWindowEvent::WindowStateUpdated
        && std::all_of(
            tail.begin() + 1, tail.end(), 
            [&](const auto &command) {
                return command->getTargetNodeId() == tail[0]->getTargetNodeId();
            });

    if (eligableForRemoval) {
        std::swap(tail[0], tail[2]);
        std::swap(tail[1], tail[3]);
        sequence.pop_back();
        sequence.pop_back();
    }
}

void optimizeTailOfCommandsSequence(
    std::vector<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> &commandsSequence)
{
    removeRedundantGeometryCommandsFromSequenceTail(commandsSequence);
}

class AccessibilityNodesTree
{
public:
    bool tryAddNode(const AccessibilityNode &node);
    bool tryRemoveNode(AccessibilityNode::Id nodeId);
    bool tryReparentNode(AccessibilityNode::Id nodeId, AccessibilityNode::Id newParentNodeId);

    const AccessibilityNode &at(AccessibilityNode::Id id) const;
    AccessibilityNode &operator[](AccessibilityNode::Id id);
    std::set<AccessibilityNode::Id> getChildIds(AccessibilityNode::Id nodeId) const;
    bool containsNode(AccessibilityNode::Id nodeId) const;
    bool canBeAddedIntoTree(const AccessibilityNode &node);

private:
    bool isDescendantOf(const AccessibilityNode &node, AccessibilityNode::Id ancestorNodeId);

    std::map<AccessibilityNode::Id, AccessibilityNode> m_nodesMap;
    std::map<AccessibilityNode::Id, std::set<AccessibilityNode::Id>> m_nodesChildIdsMap;
};

bool AccessibilityNodesTree::tryAddNode(const AccessibilityNode &node)
{
    if (!canBeAddedIntoTree(node)) {
        return false;
    }

    m_nodesMap[node.id] = node;
    if (node.parentId.hasValue()) {
        m_nodesChildIdsMap[node.parentId.value()].insert(node.id);
    }

    return true;
}

bool AccessibilityNodesTree::tryRemoveNode(AccessibilityNode::Id nodeId)
{
    if (!containsNode(nodeId)) {
        qOhosPrintfError("%s: Failed to remove node: %u - it doesn't exist", Q_FUNC_INFO, nodeId.value());
        return false;
    }

    for (auto childId : getChildIds(nodeId)) {
        tryRemoveNode(childId);
    }

    const auto parentId = m_nodesMap[nodeId].parentId;
    if (parentId.hasValue()) {
        m_nodesChildIdsMap[parentId.value()].erase(nodeId);
    }

    m_nodesChildIdsMap.erase(nodeId);
    m_nodesMap.erase(nodeId);

    return true;
}

const AccessibilityNode &AccessibilityNodesTree::at(AccessibilityNode::Id id) const
{
    return m_nodesMap.at(id);
}

AccessibilityNode &AccessibilityNodesTree::operator[](AccessibilityNode::Id id)
{
    return m_nodesMap[id];
}

std::set<AccessibilityNode::Id> AccessibilityNodesTree::getChildIds(AccessibilityNode::Id nodeId) const
{
    auto childIdsIter = m_nodesChildIdsMap.find(nodeId);
    return childIdsIter != m_nodesChildIdsMap.end()
        ? childIdsIter->second
        : std::set<AccessibilityNode::Id>();
}

bool AccessibilityNodesTree::containsNode(AccessibilityNode::Id nodeId) const
{
    return (m_nodesMap.find(nodeId) != m_nodesMap.end());
}

bool AccessibilityNodesTree::canBeAddedIntoTree(const AccessibilityNode &node)
{
    if (!node.parentId.hasValue() && !m_nodesMap.empty()) {
        qOhosPrintfError(
            "%s: cannot add root node %u, another root already exists.",
            Q_FUNC_INFO, node.id.value());
        return false;
    }

    if (containsNode(node.id)) {
        qOhosPrintfDebug(
            "%s: cannot add node %u, it already exists", Q_FUNC_INFO, node.id.value());
        return false;
    }

    if (node.parentId.hasValue() && !containsNode(node.parentId.value())) {
        qOhosPrintfError(
            "%s: cannot add node: %u, parent: %u doesn't exist", Q_FUNC_INFO, node.id.value(),
            node.parentId.value().value());
        return false;
    }

    return true;
}

bool AccessibilityNodesTree::isDescendantOf(
    const AccessibilityNode &node, AccessibilityNode::Id ancestorNodeId)
{
    for (const auto *nodePtr = &node; nodePtr->parentId.hasValue(); nodePtr = &m_nodesMap[nodePtr->parentId.value()]) {
        if (nodePtr->parentId == ancestorNodeId) {
            return true;
        }
    }

    return false;
}

bool AccessibilityNodesTree::tryReparentNode(
    AccessibilityNode::Id nodeId, AccessibilityNode::Id newParentNodeId)
{
    if (!containsNode(nodeId)) {
        qOhosPrintfError(
            "%s: failed to reparent non-existing node %u.", Q_FUNC_INFO, nodeId.value());
        return false;
    }

    if (!containsNode(newParentNodeId)) {
        qOhosPrintfError(
            "%s: failed to reparent node %u to non-existing parent node %u.",
            Q_FUNC_INFO, nodeId.value(), newParentNodeId.value());
        return false;
    }

    auto &node = m_nodesMap[nodeId];
    if (!node.parentId.hasValue()) {
        qOhosPrintfError("%s: cannot reparent the root node %u.", Q_FUNC_INFO, node.id.value());
        return false;
    }

    if (node.id == newParentNodeId) {
        qOhosPrintfError("%s: cannot reparent node %u to itself.", Q_FUNC_INFO, node.id.value());
        return false;
    }

    if (isDescendantOf(m_nodesMap[newParentNodeId], nodeId)) {
        qOhosPrintfError(
            "%s: new parent node %u cannot be a descendant of the reparented node %u.",
            Q_FUNC_INFO, newParentNodeId.value(), node.id.value());
        return false;
    }

    m_nodesChildIdsMap[node.parentId.value()].erase(node.id);
    m_nodesChildIdsMap[newParentNodeId].insert(node.id);
    node.parentId = newParentNodeId;

    return true;
}

class GuardedUpdateTreeEventsConsumer : public AccessibilityTreeEventsConsumer
{
public:
    virtual ~GuardedUpdateTreeEventsConsumer();

    void updateNodeGeometry(AccessibilityNode::Id nodeId, const QRect &geometry) override;
    void updateNodeName(AccessibilityNode::Id nodeId, const std::string &name) override;
    void updateNodeHelp(AccessibilityNode::Id nodeId, const std::string &help) override;
    void updateNodeDescription(
        AccessibilityNode::Id nodeId, const std::string &description) override;
    void updateNodeValue(
        AccessibilityNode::Id nodeId, const AccessibilityNode::ValueInfo &valueInfo) override;
    void updateNodeState(AccessibilityNode::Id nodeId, QAccessible::State state) override;
    void updateNodeParent(
        AccessibilityNode::Id nodeId, AccessibilityNode::Id parentNodeId) override;
    void tryProcessAccessibilityEvent(AccessibilityNode::Id nodeId, AccessibilityEvent event) override;
    void tryProcessAccessibilityWindowEvent(
        AccessibilityNode::Id nodeId, AccessibilityWindowEvent event) override;

protected:
    GuardedUpdateTreeEventsConsumer();

    virtual void updateNodeGeometryImpl(AccessibilityNode &node, const QRect &geometry) = 0;
    virtual void updateNodeNameImpl(AccessibilityNode &node, const std::string &name) = 0;
    virtual void updateNodeHelpImpl(AccessibilityNode &node, const std::string &help) = 0;
    virtual void updateNodeDescriptionImpl(
        AccessibilityNode &node, const std::string &description) = 0;
    virtual void updateNodeValueImpl(
        AccessibilityNode &node, const AccessibilityNode::ValueInfo &valueInfo) = 0;
    virtual void updateNodeStateImpl(AccessibilityNode &node, QAccessible::State state) = 0;
    virtual void updateNodeParentImpl(
        AccessibilityNode &node, AccessibilityNode::Id parentNodeId) = 0;
    virtual void tryProcessAccessibilityEventImpl(
        AccessibilityNode &node, AccessibilityEvent event) = 0;
    virtual void tryProcessAccessibilityWindowEventImpl(
        AccessibilityNode &node, AccessibilityWindowEvent event) = 0;
    virtual AccessibilityNode *tryFindNodeOrNull(AccessibilityNode::Id nodeId) = 0;

private:
    AccessibilityNode *tryFindNodeForUpdateOrNull(
        AccessibilityNode::Id nodeId, const char *updateFuncName);
};

class GeometryValidatingTreeEventsConsumer : public GuardedUpdateTreeEventsConsumer
{
public:
    GeometryValidatingTreeEventsConsumer(
        std::shared_ptr<AccessibilityTreeEventsConsumer> baseTreeEventsConsumer);

protected:
    void updateNodeGeometryImpl(AccessibilityNode &node, const QRect &geometry) override;
    void updateNodeNameImpl(AccessibilityNode &node, const std::string &name) override;
    void updateNodeHelpImpl(AccessibilityNode &node, const std::string &help) override;
    void updateNodeDescriptionImpl(
        AccessibilityNode &node, const std::string &description) override;
    void updateNodeValueImpl(
        AccessibilityNode &node, const AccessibilityNode::ValueInfo &valueInfo) override;
    void updateNodeStateImpl(AccessibilityNode &node, QAccessible::State state) override;
    void updateNodeParentImpl(AccessibilityNode &node, AccessibilityNode::Id parentNodeId) override;
    void tryProcessAccessibilityEventImpl(AccessibilityNode &node, AccessibilityEvent event) override;
    void tryProcessAccessibilityWindowEventImpl(
        AccessibilityNode &node, AccessibilityWindowEvent event) override;
    AccessibilityNode *tryFindNodeOrNull(AccessibilityNode::Id nodeId) override;
    void addNode(const AccessibilityNode &node) override;
    void removeNodeAndTryNotifyWindow(AccessibilityNode::Id nodeId) override;

private:
    void validateSubTreeAndPropagateIfNeeded(const AccessibilityNode &node);
    void invalidateSubTreeAndPropagateIfNeeded(const AccessibilityNode &subTreeRootNode);
    bool isNodeAccepted(const AccessibilityNode &node) const;

    std::shared_ptr<AccessibilityTreeEventsConsumer> m_baseTreeEventsConsumer;
    AccessibilityNodesTree m_nodesTree;
    std::set<AccessibilityNode::Id> m_acceptedNodesIds;
};

class QtThreadBasedAccessibilityActionsConsumer : public AccessibilityActionsConsumer
{
public:
    QtThreadBasedAccessibilityActionsConsumer(
        std::shared_ptr<AccessibilityActionsConsumer> baseActionsConsumer);

    void dispatchAction(AccessibilityNode::Id nodeId, AccessibilityAction action) const override;

private:
     std::shared_ptr<AccessibilityActionsConsumer> m_baseActionsConsumer;
};

class AccessibilityActionsConsumerImpl : public AccessibilityActionsConsumer
{
public:
    AccessibilityActionsConsumerImpl(
        std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer);

    void dispatchAction(AccessibilityNode::Id nodeId, AccessibilityAction action) const override;

private:
    void dispatchActionImpl(AccessibilityNode::Id nodeId, AccessibilityAction action) const;

    std::shared_ptr<AccessibilityTreeEventsConsumer> m_treeEventsConsumer;
};

class AccessibilityTreeImpl :
    public GuardedUpdateTreeEventsConsumer, public AccessibilityTree, public AccessibilityXComponentRegistry
{
public:
    AccessibilityNode getNodeById(AccessibilityNode::Id nodeId) const override;
    std::set<AccessibilityNode::Id> getChildIds(AccessibilityNode::Id nodeId) const override;
    bool containsNode(AccessibilityNode::Id nodeId) const override;

    void registerNodeWithXComponent(AccessibilityNode::Id nodeId, QXComponentRender xComponentRender) override;
    void unregisterNodeWithXComponent(AccessibilityNode::Id nodeId) override;

    QOhosOptional<std::pair<AccessibilityNode::Id, QXComponentRender>>
    tryFindNodeUpwardsWithXComponent(AccessibilityNode::Id nodeId) override;

protected:
    void updateNodeGeometryImpl(AccessibilityNode &node, const QRect &geometry) override;
    void updateNodeNameImpl(AccessibilityNode &node, const std::string &name) override;
    void updateNodeHelpImpl(AccessibilityNode &node, const std::string &help) override;
    void updateNodeDescriptionImpl(AccessibilityNode &node, const std::string &description) override;
    void updateNodeValueImpl(
        AccessibilityNode &node, const AccessibilityNode::ValueInfo &valueInfo) override;
    void updateNodeStateImpl(AccessibilityNode &node, QAccessible::State state) override;
    void updateNodeParentImpl(AccessibilityNode &node, AccessibilityNode::Id parentNodeId) override;
    void tryProcessAccessibilityEventImpl(AccessibilityNode &node, AccessibilityEvent event) override;
    void tryProcessAccessibilityWindowEventImpl(
        AccessibilityNode &node, AccessibilityWindowEvent event) override;
    AccessibilityNode *tryFindNodeOrNull(AccessibilityNode::Id nodeId) override;
    void addNode(const AccessibilityNode &node) override;
    void removeNodeAndTryNotifyWindow(AccessibilityNode::Id nodeId) override;

private:
    bool containsNodeWithXComponent(AccessibilityNode::Id nodeId);

    struct AccessibilityEventCreateInfo
    {
         AccessibilityNode::Id nodeId;
         AccessibilityNode::Id rootNodeId;
        ::ArkUI_AccessibilityProvider *accessibilityProvider;
        ::ArkUI_AccessibilityEventType ohosEventType;
    };

    void processAccessibilityEventWithCreateInfo(const AccessibilityEventCreateInfo &createInfo);

    AccessibilityNodesTree m_nodesTree;
    std::map<AccessibilityNode::Id, QXComponentRender> m_nodesXComponentMap;
};

GuardedUpdateTreeEventsConsumer::GuardedUpdateTreeEventsConsumer() = default;
GuardedUpdateTreeEventsConsumer::~GuardedUpdateTreeEventsConsumer() = default;

void GuardedUpdateTreeEventsConsumer::updateNodeGeometry(
    AccessibilityNode::Id nodeId, const QRect &geometry)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeGeometryImpl(*node, geometry);
    }
}

void GuardedUpdateTreeEventsConsumer::updateNodeName(
    AccessibilityNode::Id nodeId, const std::string &name)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeNameImpl(*node, name);
    }
}

void GuardedUpdateTreeEventsConsumer::updateNodeHelp(
    AccessibilityNode::Id nodeId, const std::string &help)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeHelpImpl(*node, help);
    }
}

void GuardedUpdateTreeEventsConsumer::updateNodeDescription(
    AccessibilityNode::Id nodeId, const std::string &description)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeDescriptionImpl(*node, description);
    }
}

void GuardedUpdateTreeEventsConsumer::updateNodeValue(
    AccessibilityNode::Id nodeId, const AccessibilityNode::ValueInfo &valueInfo)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeValueImpl(*node, valueInfo);
    }
}

void GuardedUpdateTreeEventsConsumer::updateNodeState(
    AccessibilityNode::Id nodeId, QAccessible::State state)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeStateImpl(*node, state);
    }
}

void GuardedUpdateTreeEventsConsumer::updateNodeParent(
    AccessibilityNode::Id nodeId, AccessibilityNode::Id parentNodeId)
{
    auto *node = tryFindNodeForUpdateOrNull(nodeId, Q_FUNC_INFO);
    if (node != nullptr) {
        updateNodeParentImpl(*node, parentNodeId);
    }
}

void GuardedUpdateTreeEventsConsumer::tryProcessAccessibilityEvent(
    AccessibilityNode::Id nodeId, AccessibilityEvent event)
{
    auto *node = tryFindNodeOrNull(nodeId);
    if (node != nullptr) {
        tryProcessAccessibilityEventImpl(*node, event);
    } else {
        qOhosPrintfError(
            "%s: got event %d for non-existing node %u, ignoring",
            Q_FUNC_INFO, static_cast<int>(event), nodeId.value());
    }
}

void GuardedUpdateTreeEventsConsumer::tryProcessAccessibilityWindowEvent(
    AccessibilityNode::Id nodeId, AccessibilityWindowEvent event)
{
    auto *node = tryFindNodeOrNull(nodeId);
    if (node != nullptr) {
        tryProcessAccessibilityWindowEventImpl(*node, event);
    } else {
        qOhosPrintfError(
            "%s: got window event %d for non-existing node %u, ignoring",
            Q_FUNC_INFO, static_cast<int>(event), nodeId.value());
    }
}

AccessibilityNode *GuardedUpdateTreeEventsConsumer::tryFindNodeForUpdateOrNull(
    AccessibilityNode::Id nodeId, const char *updateFuncName)
{
    auto *node = tryFindNodeOrNull(nodeId);
    if (node == nullptr) {
        qOhosPrintfError(
            "Failed to update a non-existing node %u with: %s.",
            nodeId.value(), updateFuncName);
    }
    return node;
}

GeometryValidatingTreeEventsConsumer::GeometryValidatingTreeEventsConsumer(
    std::shared_ptr<AccessibilityTreeEventsConsumer> baseTreeEventsConsumer)
    : m_baseTreeEventsConsumer(std::move(baseTreeEventsConsumer))
{
}

bool GeometryValidatingTreeEventsConsumer::isNodeAccepted(const AccessibilityNode &node) const
{
    auto acceptedIdIter = std::find(m_acceptedNodesIds.begin(), m_acceptedNodesIds.end(), node.id);
    return acceptedIdIter != m_acceptedNodesIds.end();
}

void GeometryValidatingTreeEventsConsumer::validateSubTreeAndPropagateIfNeeded(
    const AccessibilityNode &node)
{
    if (isNodeAccepted(node)) {
        return;
    }

    bool canAcceptNode =
        !node.parentId.hasValue()
        || (node.geometry.isValid() && isNodeAccepted(m_nodesTree[node.parentId.value()]));
    if (!canAcceptNode) {
        return;
    }

    m_acceptedNodesIds.insert(node.id);
    m_baseTreeEventsConsumer->addNode(node);

    for (auto childId : m_nodesTree.getChildIds(node.id)) {
        validateSubTreeAndPropagateIfNeeded(m_nodesTree[childId]);
    }
}

void GeometryValidatingTreeEventsConsumer::invalidateSubTreeAndPropagateIfNeeded(
    const AccessibilityNode &subTreeRootNode)
{
    for (auto childId : m_nodesTree.getChildIds(subTreeRootNode.id)) {
        invalidateSubTreeAndPropagateIfNeeded(m_nodesTree[childId]);
    }

    if (isNodeAccepted(subTreeRootNode)) {
        m_acceptedNodesIds.erase(subTreeRootNode.id);
        m_baseTreeEventsConsumer->removeNodeAndTryNotifyWindow(subTreeRootNode.id);
    }
}

void GeometryValidatingTreeEventsConsumer::addNode(const AccessibilityNode &node)
{
    if (!m_nodesTree.tryAddNode(node)) {
        return;
    }

    validateSubTreeAndPropagateIfNeeded(node);
}

void GeometryValidatingTreeEventsConsumer::removeNodeAndTryNotifyWindow(
    AccessibilityNode::Id nodeId)
{
    if (m_nodesTree.tryRemoveNode(nodeId)) {
        auto acceptedIdIt = std::find(m_acceptedNodesIds.begin(), m_acceptedNodesIds.end(), nodeId);
        if (acceptedIdIt != m_acceptedNodesIds.end()) {
            m_acceptedNodesIds.erase(acceptedIdIt);
            m_baseTreeEventsConsumer->removeNodeAndTryNotifyWindow(nodeId);
        }
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeGeometryImpl(
    AccessibilityNode &node, const QRect &geometry)
{
    node.geometry = geometry;

    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->updateNodeGeometry(node.id, geometry);
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeNameImpl(
    AccessibilityNode &node, const std::string &name)
{
    node.name = name;

    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->updateNodeName(node.id, name);
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeHelpImpl(
    AccessibilityNode &node, const std::string &help)
{
    node.help = help;

    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->updateNodeHelp(node.id, help);
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeDescriptionImpl(
    AccessibilityNode &node, const std::string &description)
{
    node.description = description;

    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->updateNodeDescription(node.id, description);
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeValueImpl(
    AccessibilityNode &node, const AccessibilityNode::ValueInfo &valueInfo)
{
    node.valueInfo = valueInfo;

    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->updateNodeValue(node.id, valueInfo);
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeStateImpl(
    AccessibilityNode &node, QAccessible::State state)
{
    setNodeState(node, state);

    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->updateNodeState(node.id, state);
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

void GeometryValidatingTreeEventsConsumer::updateNodeParentImpl(
    AccessibilityNode &node, AccessibilityNode::Id parentNodeId)
{
    if (!m_nodesTree.tryReparentNode(node.id, parentNodeId)) {
        return;
    }

    if (isNodeAccepted(node)) {
        if (isNodeAccepted(m_nodesTree[parentNodeId])) {
            m_baseTreeEventsConsumer->updateNodeParent(node.id, parentNodeId);
        } else {
            invalidateSubTreeAndPropagateIfNeeded(node);
        }
    } else {
        validateSubTreeAndPropagateIfNeeded(node);
    }
}

AccessibilityNode *GeometryValidatingTreeEventsConsumer::tryFindNodeOrNull(
    AccessibilityNode::Id nodeId)
{
    return m_nodesTree.containsNode(nodeId) ? &m_nodesTree[nodeId] : nullptr;
}

void GeometryValidatingTreeEventsConsumer::tryProcessAccessibilityEventImpl(
    AccessibilityNode &node, AccessibilityEvent eventType)
{
    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->tryProcessAccessibilityEvent(node.id, eventType);
    } else {
        qOhosPrintfDebug(
            "%s: ignoring the event '%s' for a malformed node: %u.", 
            Q_FUNC_INFO, mapAccessibilityEventToString(eventType).c_str(), node.id.value());
    }
}

void GeometryValidatingTreeEventsConsumer::tryProcessAccessibilityWindowEventImpl(
    AccessibilityNode &node, AccessibilityWindowEvent eventType)
{
    if (isNodeAccepted(node)) {
        m_baseTreeEventsConsumer->tryProcessAccessibilityWindowEvent(node.id, eventType);
    } else {
        qOhosPrintfDebug(
            "%s: ignoring the window event '%s' for a malformed node: %u.",
            Q_FUNC_INFO, mapAccessibilityWindowEventToString(eventType).c_str(), node.id.value());
    }
}

QtThreadBasedAccessibilityActionsConsumer::QtThreadBasedAccessibilityActionsConsumer(
    std::shared_ptr<AccessibilityActionsConsumer> baseActionsConsumer)
    : m_baseActionsConsumer(baseActionsConsumer)
{
}

void QtThreadBasedAccessibilityActionsConsumer::dispatchAction(
    AccessibilityNode::Id nodeId, AccessibilityAction action) const
{
    QtOhos::invokeInQtThread(
        [baseActionsConsumer = m_baseActionsConsumer, nodeId, action]() {
            baseActionsConsumer->dispatchAction(nodeId, action);
        });
}

AccessibilityActionsConsumerImpl::AccessibilityActionsConsumerImpl(
    std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer)
    : m_treeEventsConsumer(treeEventsConsumer)
{
}

void AccessibilityActionsConsumerImpl::dispatchAction(
    AccessibilityNode::Id nodeId, AccessibilityAction action) const
{
    QAccessibleWrapper::startSession(
        [&]() {
            dispatchActionImpl(nodeId, action);
        });
}

void AccessibilityActionsConsumerImpl::dispatchActionImpl(
    AccessibilityNode::Id nodeId, AccessibilityAction action) const
{
    auto *interface = QAccessibleWrapper::accessibleInterface(nodeId.value());
    if (interface == nullptr) {
        return;
    }
    auto *actionInterface = interface->actionInterface();
    if (actionInterface == nullptr) {
        return;
    }

    const auto actionNames = actionInterface->actionNames();

    switch (action) {
    case QOhos::AccessibilityAction::ClearFocus:
        break;

    case QOhos::AccessibilityAction::Click: {
        if (actionNames.contains(QAccessibleActionInterface::pressAction())) {
            actionInterface->doAction(QAccessibleActionInterface::pressAction());
        } else if (actionNames.contains(QAccessibleActionInterface::toggleAction())) {
            actionInterface->doAction(QAccessibleActionInterface::toggleAction());
        } else if (actionNames.contains(QAccessibleActionInterface::showMenuAction())) {
            actionInterface->doAction(QAccessibleActionInterface::showMenuAction());
        }

        if (interface->textInterface() != nullptr) {
            QGuiApplication::inputMethod()->show();
        }

        m_treeEventsConsumer->tryProcessAccessibilityEvent(
            nodeId, QOhos::AccessibilityEvent::ElementClicked);
        break;
    }

    case QOhos::AccessibilityAction::GainFocus:
        actionInterface->doAction(QAccessibleActionInterface::setFocusAction());
        break;

    case QOhos::AccessibilityAction::ScrollForward:
        if (actionNames.contains(QAccessibleActionInterface::scrollUpAction())) {
            actionInterface->doAction(QAccessibleActionInterface::scrollUpAction());
        } else {
            actionInterface->doAction(QAccessibleActionInterface::scrollRightAction());
        }
        m_treeEventsConsumer->tryProcessAccessibilityEvent(
            nodeId, QOhos::AccessibilityEvent::ElementScrolled);
        break;

    case QOhos::AccessibilityAction::ScrollBackward:
        if (actionNames.contains(QAccessibleActionInterface::scrollDownAction())) {
            actionInterface->doAction(QAccessibleActionInterface::scrollDownAction());
        } else {
            actionInterface->doAction(QAccessibleActionInterface::scrollLeftAction());
        }
        m_treeEventsConsumer->tryProcessAccessibilityEvent(
            nodeId, QOhos::AccessibilityEvent::ElementScrolled);
        break;
    }
}

void AccessibilityTreeImpl::addNode(const AccessibilityNode &node)
{
    m_nodesTree.tryAddNode(node);
}

void AccessibilityTreeImpl::removeNodeAndTryNotifyWindow(AccessibilityNode::Id nodeId)
{
    if (!m_nodesTree.containsNode(nodeId)) {
        return;
    }

    auto parentNodeId = m_nodesTree.at(nodeId).parentId;

    if (!m_nodesTree.tryRemoveNode(nodeId)) {
        return;
    }
    m_nodesXComponentMap.erase(nodeId);

    if (!parentNodeId.hasValue()) {
        return;
    }

    auto &parentNode = m_nodesTree[parentNodeId.value()];
    if (parentNode.parentId.hasValue()) {
        tryProcessAccessibilityWindowEventImpl(
            parentNode, AccessibilityWindowEvent::WindowContentUpdated);
    }
}

void AccessibilityTreeImpl::updateNodeGeometryImpl(AccessibilityNode &node, const QRect &geometry)
{
    node.geometry = geometry;
}

void AccessibilityTreeImpl::updateNodeNameImpl(AccessibilityNode &node, const std::string &name)
{
    node.name = name;
}

void AccessibilityTreeImpl::updateNodeHelpImpl(AccessibilityNode &node, const std::string &help)
{
    node.help = help;
}

void AccessibilityTreeImpl::updateNodeDescriptionImpl(
    AccessibilityNode &node, const std::string &description)
{
    node.description = description;
}

void AccessibilityTreeImpl::updateNodeValueImpl(
    AccessibilityNode &node, const AccessibilityNode::ValueInfo &valueInfo)
{
    node.valueInfo = valueInfo;
}

void AccessibilityTreeImpl::updateNodeStateImpl(AccessibilityNode &node, QAccessible::State state)
{
    setNodeState(node, state);
}

void AccessibilityTreeImpl::updateNodeParentImpl(
    AccessibilityNode &node, AccessibilityNode::Id parentNodeId)
{
    m_nodesTree.tryReparentNode(node.id, parentNodeId);
}

AccessibilityNode *AccessibilityTreeImpl::tryFindNodeOrNull(AccessibilityNode::Id nodeId)
{
    return m_nodesTree.containsNode(nodeId) ? &m_nodesTree[nodeId] : nullptr;
}

bool AccessibilityTreeImpl::containsNodeWithXComponent(AccessibilityNode::Id nodeId)
{
    return m_nodesXComponentMap.find(nodeId) != m_nodesXComponentMap.end();
}

void AccessibilityTreeImpl::processAccessibilityEventWithCreateInfo(
    const AccessibilityEventCreateInfo &createInfo)
{
    auto eventInfo = makeEventInfoFromNode(
        createInfo.rootNodeId, *this, createInfo.ohosEventType, getNodeById(createInfo.nodeId));

    QArkUi::callArkUi(
        Q_OHOS_NAMED_FUNC(::OH_ArkUI_SendAccessibilityAsyncEvent),
        createInfo.accessibilityProvider, eventInfo.get(),
        [](std::int32_t errorCode) {
            if (errorCode != ::ARKUI_ACCESSIBILITY_NATIVE_RESULT_SUCCESSFUL) {
                qOhosPrintfError(
                    "OH_ArkUI_SendAccessibilityAsyncEvent: encountered error (code: '%d') after sending an event.",
                    errorCode);
            }
        });
}

QOhosOptional<std::pair<AccessibilityNode::Id, QXComponentRender>>
AccessibilityTreeImpl::tryFindNodeUpwardsWithXComponent(AccessibilityNode::Id nodeId)
{
    if (!containsNode(nodeId)) {
        qOhosPrintfError("%s: Node %u doesn't exist", Q_FUNC_INFO, nodeId.value());
        return makeEmptyQOhosOptional();
    }

    auto iterNodeId = makeQOhosOptional(nodeId);
    while (iterNodeId.hasValue() && !containsNodeWithXComponent(iterNodeId.value())) {
        iterNodeId = m_nodesTree[iterNodeId.value()].parentId;
    }

    return iterNodeId.hasValue()
        ? makeQOhosOptional(
            std::make_pair(iterNodeId.value(), m_nodesXComponentMap.at(iterNodeId.value())))
        : makeEmptyQOhosOptional();
}

AccessibilityNode AccessibilityTreeImpl::getNodeById(AccessibilityNode::Id nodeId) const
{
    if (!containsNode(nodeId)) {
        qOhosPrintfError("%s: Node %u doesn't exist", Q_FUNC_INFO, nodeId.value());
        return {};
    }

    return m_nodesTree.at(nodeId);
}

std::set<AccessibilityNode::Id> AccessibilityTreeImpl::getChildIds(AccessibilityNode::Id nodeId) const
{
    return m_nodesTree.getChildIds(nodeId);
}

bool AccessibilityTreeImpl::containsNode(AccessibilityNode::Id nodeId) const
{
    return m_nodesTree.containsNode(nodeId);
}

void AccessibilityTreeImpl::registerNodeWithXComponent(
    AccessibilityNode::Id nodeId, QXComponentRender xComponent)
{
    if (containsNodeWithXComponent(nodeId)) {
        qOhosPrintfError(
            "%s: Failed to register node with XComponent: node %u is already registered with an XComponent.",
            Q_FUNC_INFO, nodeId.value());
        return;
    }

    m_nodesXComponentMap.insert({nodeId, xComponent});
}

void AccessibilityTreeImpl::unregisterNodeWithXComponent(AccessibilityNode::Id nodeId)
{
    if (!containsNodeWithXComponent(nodeId)) {
        qOhosPrintfError(
            "%s: Failed to unregister node with XComponent: node %u hasn't been registered with an XComponent.",
            Q_FUNC_INFO, nodeId.value());
        return;
    }

    m_nodesXComponentMap.erase(nodeId);
}

void AccessibilityTreeImpl::tryProcessAccessibilityEventImpl(
    AccessibilityNode &node, AccessibilityEvent eventType)
{
    if (!isAccessibilityOpen()) {
        return;
    }

    auto maybeRootNodeIdXComponentPair = tryFindNodeUpwardsWithXComponent(node.id);
    if (!maybeRootNodeIdXComponentPair.hasValue()) {
        qOhosPrintfError(
            "%s: No root node found for the given node id: %u. Ignoring the event...",
            Q_FUNC_INFO, node.id.value());
        return;
    }

    processAccessibilityEventWithCreateInfo(
        {
            .nodeId = node.id,
            .rootNodeId = maybeRootNodeIdXComponentPair.value().first,
            .accessibilityProvider = getNativeAccessibilityProviderFromXComponentOrFail(
                maybeRootNodeIdXComponentPair.value().second),
            .ohosEventType = mapAccessibilityEventToOhosAccessibilityEvent(eventType),
        });
}

void AccessibilityTreeImpl::tryProcessAccessibilityWindowEventImpl(
    AccessibilityNode &node, AccessibilityWindowEvent eventType)
{
    if (!isAccessibilityOpen()) {
        return;
    }

    auto maybeRootNodeIdXComponentPair = tryFindNodeUpwardsWithXComponent(node.id);
    if (!maybeRootNodeIdXComponentPair.hasValue()) {
        qOhosPrintfError(
            "%s: No root node found for the given node id: %u. Ignoring the window event...",
            Q_FUNC_INFO, node.id.value());
        return;
    }

    processAccessibilityEventWithCreateInfo(
        {
            .nodeId = maybeRootNodeIdXComponentPair.value().first,
            .rootNodeId = maybeRootNodeIdXComponentPair.value().first,
            .accessibilityProvider = getNativeAccessibilityProviderFromXComponentOrFail(
                maybeRootNodeIdXComponentPair.value().second),
            .ohosEventType = mapAccessibilityWindowEventToOhosAccessibilityEvent(eventType),
        });
}

}

AccessibilityTreeEventsConsumer::AccessibilityTreeEventsConsumer() = default;
AccessibilityTreeEventsConsumer::~AccessibilityTreeEventsConsumer() = default;

AccessibilityTree::AccessibilityTree() = default;
AccessibilityTree::~AccessibilityTree() = default;

AccessibilityXComponentRegistry::AccessibilityXComponentRegistry() = default;
AccessibilityXComponentRegistry::~AccessibilityXComponentRegistry() = default;

AccessibilityActionsConsumer::AccessibilityActionsConsumer() = default;
AccessibilityActionsConsumer::~AccessibilityActionsConsumer() = default;

AccessibilityTreeContext makeAccessibilityTreeContext()
{
    auto tree = std::make_shared<AccessibilityTreeImpl>();
    return {
        .accessibilityTree = tree,
        .accessibilityTreeEventsConsumer = tree,
        .accessibilityXComponentRegistry = tree,
    };
}

std::shared_ptr<AccessibilityTreeEventsConsumer>
makeGeometryValidatingAccessibilityTreeEventsConsumerDecorator(
    std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer)
{
    return std::make_shared<GeometryValidatingTreeEventsConsumer>(treeEventsConsumer);
}

std::shared_ptr<AccessibilityTreeEventsConsumer>
makeAccessibilityTreeEventsConsumerJsDecorator(std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer)
{
    auto requestsHandler =
        makeQtOhosBatchingMTRequestsHandler<std::vector<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>>>(
            [](std::function<void()> task) {
                QtOhos::invokeInJsThread(
                    [task = std::move(task)](QtOhos::JsState &) {
                        task();
                    });
            },
            [treeEventsConsumer](std::vector<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> commandsBatch) {
                for (const auto &command : commandsBatch) {
                    command->apply(*treeEventsConsumer);
                }
            });

    return makeAccessibilityTreeEventsConsumerCommandsRecorder(
        [requestsHandler = std::move(requestsHandler)](std::unique_ptr<AccessibilityTreeEventsConsumerCommand> command) {
            requestsHandler(
                [&](std::vector<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> &commandsBatch) {
                    commandsBatch.push_back(std::move(command));
                    optimizeTailOfCommandsSequence(commandsBatch);
                });
        });
}

std::unique_ptr<AccessibilityActionsConsumer>
makeAccessibilityActionsConsumer(std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer)
{
    return std::make_unique<AccessibilityActionsConsumerImpl>(treeEventsConsumer);
}

std::unique_ptr<AccessibilityActionsConsumer>
makeAccessibilityActionsQtDecorator(std::shared_ptr<AccessibilityActionsConsumer> actionsConsumer)
{
    return std::make_unique<QtThreadBasedAccessibilityActionsConsumer>(actionsConsumer);
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
