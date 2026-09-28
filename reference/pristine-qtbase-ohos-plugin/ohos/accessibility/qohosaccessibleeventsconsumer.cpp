// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <accessibility/qohosaccessibleeventsconsumer.h>
#include <accessibility/qohosaccessiblewrappers.h>
#include <accessibility/qohosaccessibilitynodehelpers.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

bool representsTopLevelWindow(QAccessibleInterface *interface)
{
    auto *parent = interface->parent();
    return parent != nullptr && parent->parent() == nullptr;
}

bool storedInterfaceMatchesQtCachedAccessibleInterfaceWithId(
    QAccessibleInterface *interface, QAccessible::Id interfaceId)
{
    return interface != nullptr && QAccessible::accessibleInterface(interfaceId) == interface;
}

class AccessibleEventsConsumerImpl : public AccessibleEventsConsumer
{
public:
    AccessibleEventsConsumerImpl(
        std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer);

    void notifyInterfaceAdded(QAccessible::Id interfaceId) override;
    void notifyInterfaceRemoved(QAccessible::Id interfaceId) override;
    void notifyInterfaceGeometryChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceNameChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceHelpChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceDescriptionChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceValueChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceVisibilityChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceAccessibleStateChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceParentChanged(QAccessible::Id interfaceId) override;
    void notifyInterfaceFocused(QAccessible::Id interfaceId) override;

private:
    void processUnfocusEventForRecentlyFocusedNodeIfPossible();
    void notifyTreeNodeUpdatedIfFocused(QAccessibleInterface *interface);

    std::shared_ptr<AccessibilityTreeEventsConsumer> m_treeEventsConsumer;
    QOhosOptional<std::pair<QAccessible::Id, QAccessibleInterface *>> m_lastFocusedNodeIdWithInterfacePtr;
};

AccessibleEventsConsumerImpl::AccessibleEventsConsumerImpl(
    std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer)
    : m_treeEventsConsumer(std::move(treeEventsConsumer))
{
}

void AccessibleEventsConsumerImpl::notifyInterfaceAdded(QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);

    m_treeEventsConsumer->addNode(makeAccessibilityNode(interface));
    m_treeEventsConsumer->tryProcessAccessibilityWindowEvent(
        AccessibilityNode::Id(interfaceId),
        AccessibilityWindowEvent::WindowContentUpdated);
}

void AccessibleEventsConsumerImpl::notifyInterfaceRemoved(QAccessible::Id interfaceId)
{
    m_treeEventsConsumer->removeNodeAndTryNotifyWindow(
        AccessibilityNode::Id(interfaceId));
}

void AccessibleEventsConsumerImpl::notifyInterfaceGeometryChanged(
    QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);
    auto nodeId = AccessibilityNode::Id(interfaceId);

    m_treeEventsConsumer->updateNodeGeometry(nodeId, getRelativeGeometryOrNull(interface));

    notifyTreeNodeUpdatedIfFocused(interface);

    if (representsTopLevelWindow(interface)) {
        m_treeEventsConsumer->tryProcessAccessibilityWindowEvent(
            nodeId, AccessibilityWindowEvent::WindowStateUpdated);
    }
}

void AccessibleEventsConsumerImpl::notifyInterfaceNameChanged(QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);

    m_treeEventsConsumer->updateNodeName(
        AccessibilityNode::Id(interfaceId),
        interface->text(QAccessible::Name).toStdString());

    notifyTreeNodeUpdatedIfFocused(interface);
}

void AccessibleEventsConsumerImpl::notifyInterfaceHelpChanged(QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);

    m_treeEventsConsumer->updateNodeHelp(
        AccessibilityNode::Id(interfaceId),
        interface->text(QAccessible::Help).toStdString());

    notifyTreeNodeUpdatedIfFocused(interface);
}

void AccessibleEventsConsumerImpl::notifyInterfaceDescriptionChanged(
    QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);

    m_treeEventsConsumer->updateNodeDescription(
        AccessibilityNode::Id(interfaceId),
        interface->text(QAccessible::Description).toStdString());

    notifyTreeNodeUpdatedIfFocused(interface);
}

void AccessibleEventsConsumerImpl::notifyInterfaceValueChanged(QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);

    m_treeEventsConsumer->updateNodeValue(
        AccessibilityNode::Id(interfaceId), makeValueInfo(interface));

    notifyTreeNodeUpdatedIfFocused(interface);
}

void AccessibleEventsConsumerImpl::notifyInterfaceVisibilityChanged(
    QAccessible::Id interfaceId)
{
    notifyInterfaceAccessibleStateChanged(interfaceId);

    if (representsTopLevelWindow(QAccessibleWrapper::accessibleInterface(interfaceId))) {
        m_treeEventsConsumer->tryProcessAccessibilityWindowEvent(
            AccessibilityNode::Id(interfaceId), AccessibilityWindowEvent::WindowStateUpdated);
    }
}

void AccessibleEventsConsumerImpl::notifyInterfaceAccessibleStateChanged(
    QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);

    m_treeEventsConsumer->updateNodeState(
        AccessibilityNode::Id(interfaceId), interface->state());

    notifyTreeNodeUpdatedIfFocused(interface);
}

void AccessibleEventsConsumerImpl::notifyInterfaceParentChanged(
    QAccessible::Id interfaceId)
{
    auto *interface = QAccessibleWrapper::accessibleInterface(interfaceId);
    auto nodeId = AccessibilityNode::Id(interfaceId);

    m_treeEventsConsumer->updateNodeParent(
        AccessibilityNode::Id(interfaceId),
        AccessibilityNode::Id(QAccessibleWrapper::uniqueId(interface->parent())));
    m_treeEventsConsumer->tryProcessAccessibilityWindowEvent(
        nodeId, AccessibilityWindowEvent::WindowContentUpdated);
}

void AccessibleEventsConsumerImpl::notifyInterfaceFocused(QAccessible::Id interfaceId)
{
    processUnfocusEventForRecentlyFocusedNodeIfPossible();
    m_lastFocusedNodeIdWithInterfacePtr =
        std::make_pair(interfaceId, QAccessible::accessibleInterface(interfaceId));
    m_treeEventsConsumer->tryProcessAccessibilityEvent(
        AccessibilityNode::Id(interfaceId), AccessibilityEvent::ElementFocused);
}

void AccessibleEventsConsumerImpl::processUnfocusEventForRecentlyFocusedNodeIfPossible()
{
    if (!m_lastFocusedNodeIdWithInterfacePtr.hasValue()) {
        return;
    }

    auto storedFocusedNodeId =
        static_cast<QAccessible::Id>(m_lastFocusedNodeIdWithInterfacePtr.value().first);
    auto *storedFocusedNodeInterface = m_lastFocusedNodeIdWithInterfacePtr.value().second;

    if (storedInterfaceMatchesQtCachedAccessibleInterfaceWithId(
        storedFocusedNodeInterface, storedFocusedNodeId)) {
        if (!storedFocusedNodeInterface->state().focused) {
            m_treeEventsConsumer->tryProcessAccessibilityEvent(
                QOhos::AccessibilityNode::Id(storedFocusedNodeId),
                QOhos::AccessibilityEvent::ElementFocusCleared);
        } else {
            qOhosPrintfWarning(
                "%s: the designated node %u is still focused by Qt. Cannot notify about the unfocus event.",
                Q_FUNC_INFO, storedFocusedNodeId);
        }
    } else {
        qOhosPrintfWarning(
            "%s: the designated node %u has expired. Cannot notify about the unfocus event.",
            Q_FUNC_INFO, QOhos::AccessibilityNode::Id(storedFocusedNodeId).value());
    }

    m_lastFocusedNodeIdWithInterfacePtr.reset();
}

void AccessibleEventsConsumerImpl::notifyTreeNodeUpdatedIfFocused(
    QAccessibleInterface *interface)
{
    auto interfaceId = QAccessibleWrapper::uniqueId(interface);

    if (m_lastFocusedNodeIdWithInterfacePtr.hasValue()
        && m_lastFocusedNodeIdWithInterfacePtr.value().first == interfaceId) {
        m_treeEventsConsumer->tryProcessAccessibilityEvent(
            AccessibilityNode::Id(interfaceId), QOhos::AccessibilityEvent::FocusedElementUpdated);
    }
}

}

AccessibleEventsConsumer::~AccessibleEventsConsumer() = default;

std::shared_ptr<AccessibleEventsConsumer> makeAccessibleEventsConsumer(
    std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer)
{
    return std::make_shared<AccessibleEventsConsumerImpl>(std::move(treeEventsConsumer));
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
