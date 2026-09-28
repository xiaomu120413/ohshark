// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <accessibility/qohosaccessibilitytreeeventsconsumercommand.h>
#include <utility>

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

struct TreeEventConsumerCommandInfo
{
    AccessibilityNode::Id targetNodeId;
    AccessibilityTreeEventsConsumerCommand::Type type;
    QOhosOptional<AccessibilityEvent> optEventType;
    QOhosOptional<AccessibilityWindowEvent> optWindowEventType;
};

class AccessibilityTreeEventsConsumerCommandImpl : public AccessibilityTreeEventsConsumerCommand
{
public:
    AccessibilityTreeEventsConsumerCommandImpl(
        TreeEventConsumerCommandInfo commandInfo,
        std::function<void(AccessibilityTreeEventsConsumer &)> action);

    void apply(AccessibilityTreeEventsConsumer &consumer) const override;

    Type getType() const override;
    AccessibilityNode::Id getTargetNodeId() const override;
    QOhosOptional<AccessibilityEvent> getOptEventType() const override;
    QOhosOptional<AccessibilityWindowEvent> getOptWindowEventType() const override;

private:
    TreeEventConsumerCommandInfo m_commandInfo;
    std::function<void(AccessibilityTreeEventsConsumer &)> m_action;
};

class AccessibilityTreeEventsConsumerCommandsRecorder : public AccessibilityTreeEventsConsumer
{
public:
    AccessibilityTreeEventsConsumerCommandsRecorder(
        QOhosConsumer<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> commandsConsumer);

    void addNode(const AccessibilityNode &node) override;
    void removeNodeAndTryNotifyWindow(AccessibilityNode::Id nodeId) override;
    void updateNodeGeometry(AccessibilityNode::Id nodeId, const QRect &geometry) override;
    void updateNodeName(AccessibilityNode::Id nodeId, const std::string &name) override;
    void updateNodeHelp(AccessibilityNode::Id nodeId, const std::string &help) override;
    void updateNodeDescription(AccessibilityNode::Id nodeId, const std::string &description) override;
    void updateNodeValue(
        AccessibilityNode::Id nodeId, const AccessibilityNode::ValueInfo &valueInfo) override;
    void updateNodeState(AccessibilityNode::Id nodeId, QAccessible::State state) override;
    void updateNodeParent(AccessibilityNode::Id nodeId, AccessibilityNode::Id parentNodeId) override;
    void tryProcessAccessibilityEvent(AccessibilityNode::Id nodeId, AccessibilityEvent event) override;
    void tryProcessAccessibilityWindowEvent(
        AccessibilityNode::Id nodeId, AccessibilityWindowEvent event) override;

private:
    template<typename... MemFuncParams, typename... Args>
    void emitCommand(
        TreeEventConsumerCommandInfo commandMetaInfo,
        void (AccessibilityTreeEventsConsumer::*actionMemFunc)(MemFuncParams...),
        Args &&...args)
    {
        m_commandsConsumer(
            std::make_unique<AccessibilityTreeEventsConsumerCommandImpl>(
                commandMetaInfo,
                [actionMemFunc, args...](AccessibilityTreeEventsConsumer &consumer) {
                    (consumer.*actionMemFunc)(args...);
                }));
    }

    QOhosConsumer<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> m_commandsConsumer;
};

AccessibilityTreeEventsConsumerCommandImpl::AccessibilityTreeEventsConsumerCommandImpl(
    TreeEventConsumerCommandInfo metaInfo, std::function<void(AccessibilityTreeEventsConsumer &)> action)
    : m_commandInfo(metaInfo)
    , m_action(std::move(action))
{
}

void AccessibilityTreeEventsConsumerCommandImpl::apply(AccessibilityTreeEventsConsumer &consumer) const
{
    m_action(consumer);
}

AccessibilityTreeEventsConsumerCommand::Type AccessibilityTreeEventsConsumerCommandImpl::getType() const
{
    return m_commandInfo.type;
}

AccessibilityNode::Id AccessibilityTreeEventsConsumerCommandImpl::getTargetNodeId() const
{
    return m_commandInfo.targetNodeId;
}

QOhosOptional<AccessibilityEvent> AccessibilityTreeEventsConsumerCommandImpl::getOptEventType() const
{
    return m_commandInfo.optEventType;
}

QOhosOptional<AccessibilityWindowEvent>
AccessibilityTreeEventsConsumerCommandImpl::getOptWindowEventType() const
{
    return m_commandInfo.optWindowEventType;
}

AccessibilityTreeEventsConsumerCommandsRecorder::AccessibilityTreeEventsConsumerCommandsRecorder(
    QOhosConsumer<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> commandsConsumer)
    : m_commandsConsumer(std::move(commandsConsumer))
{
}

void AccessibilityTreeEventsConsumerCommandsRecorder::addNode(const AccessibilityNode &node)
{
    emitCommand(
        {
            .targetNodeId = node.id,
            .type = AccessibilityTreeEventsConsumerCommand::Type::AddNode,
        },
        &AccessibilityTreeEventsConsumer::addNode, node);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::removeNodeAndTryNotifyWindow(
    AccessibilityNode::Id nodeId)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::RemoveNodeAndTryNotifyWindow,
        },
        &AccessibilityTreeEventsConsumer::removeNodeAndTryNotifyWindow, nodeId);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeGeometry(
    AccessibilityNode::Id nodeId, const QRect &geometry)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeGeometry,
        },
        &AccessibilityTreeEventsConsumer::updateNodeGeometry, nodeId, geometry);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeName(
    AccessibilityNode::Id nodeId, const std::string &name)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeName,
        },
        &AccessibilityTreeEventsConsumer::updateNodeName, nodeId, name);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeHelp(
    AccessibilityNode::Id nodeId, const std::string &help)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeHelp,
        },
        &AccessibilityTreeEventsConsumer::updateNodeHelp, nodeId, help);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeDescription(
    AccessibilityNode::Id nodeId, const std::string &description)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeDescription,
        },
        &AccessibilityTreeEventsConsumer::updateNodeDescription, nodeId, description);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeValue(
    AccessibilityNode::Id nodeId, const AccessibilityNode::ValueInfo &valueInfo)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeValue,
        },
        &AccessibilityTreeEventsConsumer::updateNodeValue, nodeId, valueInfo);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeState(
    AccessibilityNode::Id nodeId, QAccessible::State state)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeState,
        },
        &AccessibilityTreeEventsConsumer::updateNodeState, nodeId, state);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::updateNodeParent(
    AccessibilityNode::Id nodeId, AccessibilityNode::Id parentNodeId)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::UpdateNodeParent,
        },
        &AccessibilityTreeEventsConsumer::updateNodeParent, nodeId, parentNodeId);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::tryProcessAccessibilityEvent(
    AccessibilityNode::Id nodeId, AccessibilityEvent event)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::ProcessAccessibilityEvent,
            .optEventType = makeQOhosOptional(event),
        },
        &AccessibilityTreeEventsConsumer::tryProcessAccessibilityEvent, nodeId, event);
}

void AccessibilityTreeEventsConsumerCommandsRecorder::tryProcessAccessibilityWindowEvent(
    AccessibilityNode::Id nodeId, AccessibilityWindowEvent event)
{
    emitCommand(
        {
            .targetNodeId = nodeId,
            .type = AccessibilityTreeEventsConsumerCommand::Type::ProcessAccessibilityWindowEvent,
            .optWindowEventType = makeQOhosOptional(event),
        },
        &AccessibilityTreeEventsConsumer::tryProcessAccessibilityWindowEvent, nodeId, event);
}

}

AccessibilityTreeEventsConsumerCommand::AccessibilityTreeEventsConsumerCommand() = default;

AccessibilityTreeEventsConsumerCommand::~AccessibilityTreeEventsConsumerCommand() = default;

std::shared_ptr<AccessibilityTreeEventsConsumer> makeAccessibilityTreeEventsConsumerCommandsRecorder(
    QOhosConsumer<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> commandsConsumer)
{
    return std::make_shared<AccessibilityTreeEventsConsumerCommandsRecorder>(std::move(commandsConsumer));
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
