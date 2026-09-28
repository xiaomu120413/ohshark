// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef ACCESSIBILITYTREE_H
#define ACCESSIBILITYTREE_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qrect.h>
#include <QtGui/qaccessible.h>
#include <map>
#include <memory>
#include <qohosutils.h>
#include <render/qxcomponent.h>
#include <set>
#include <string>
#include <utility>

QT_BEGIN_NAMESPACE

namespace QOhos {

struct AccessibilityNode
{
    using Id = QtOhos::TypedId<std::uint32_t, struct IdTag>;

    struct ValueInfo
    {
        QOhosOptional<std::pair<std::string, std::string>> minMaxRange;
        std::string current;
    };

    Id id;
    QOhosOptional<Id> parentId;

    QRect geometry;
    std::string componentType;

    std::string name;
    std::string help;
    std::string description;
    ValueInfo valueInfo;

    bool enabled;
    bool visible;
    bool checked;
    bool focused;
    bool selected;
    bool passwordEdit;

    bool editable;
    bool clickable;
    bool checkable;
    bool focusable;
    bool scrollable;

    std::string backgroundColor;
};

enum class AccessibilityWindowEvent
{
    WindowContentUpdated,
    WindowStateUpdated,
};

enum class AccessibilityEvent
{
    ElementClicked,
    ElementFocusCleared,
    ElementFocused,
    ElementScrolled,
    FocusedElementUpdated,
};

enum class AccessibilityAction
{
    ClearFocus,
    Click,
    GainFocus,
    ScrollBackward,
    ScrollForward,
};

class AccessibilityTreeEventsConsumer
{
public:
    virtual ~AccessibilityTreeEventsConsumer();

    virtual void addNode(const AccessibilityNode &node) = 0;
    virtual void removeNodeAndTryNotifyWindow(AccessibilityNode::Id nodeId) = 0;
    virtual void updateNodeGeometry(AccessibilityNode::Id nodeId, const QRect &geometry) = 0;
    virtual void updateNodeName(AccessibilityNode::Id nodeId, const std::string &name) = 0;
    virtual void updateNodeHelp(AccessibilityNode::Id nodeId, const std::string &help) = 0;
    virtual void updateNodeDescription(
        AccessibilityNode::Id nodeId, const std::string &description) = 0;
    virtual void updateNodeValue(
        AccessibilityNode::Id nodeId, const AccessibilityNode::ValueInfo &valueInfo) = 0;
    virtual void updateNodeState(
        AccessibilityNode::Id nodeId, QAccessible::State state) = 0;
    virtual void updateNodeParent(
        AccessibilityNode::Id nodeId, AccessibilityNode::Id parentNodeId) = 0;
    virtual void tryProcessAccessibilityEvent(AccessibilityNode::Id nodeId, AccessibilityEvent event) = 0;
    virtual void tryProcessAccessibilityWindowEvent(
        AccessibilityNode::Id nodeId, AccessibilityWindowEvent event) = 0;

protected:
    AccessibilityTreeEventsConsumer();
};

class AccessibilityActionsConsumer
{
public:
    virtual ~AccessibilityActionsConsumer();

    virtual void dispatchAction(AccessibilityNode::Id nodeId, AccessibilityAction action) const = 0;

protected:
    AccessibilityActionsConsumer();
};

class AccessibilityTree
{
public:
    virtual ~AccessibilityTree();

    virtual AccessibilityNode getNodeById(AccessibilityNode::Id nodeId) const = 0;
    virtual std::set<AccessibilityNode::Id> getChildIds(AccessibilityNode::Id nodeId) const = 0;
    virtual bool containsNode(AccessibilityNode::Id nodeId) const = 0;

protected:
    AccessibilityTree();
};

class AccessibilityXComponentRegistry
{
public:
    virtual ~AccessibilityXComponentRegistry();

    virtual QOhosOptional<std::pair<AccessibilityNode::Id, QXComponentRender>>
    tryFindNodeUpwardsWithXComponent(AccessibilityNode::Id nodeId) = 0;

    virtual void registerNodeWithXComponent(AccessibilityNode::Id nodeId, QXComponentRender xComponent) = 0;
    virtual void unregisterNodeWithXComponent(AccessibilityNode::Id nodeId) = 0;

protected:
    AccessibilityXComponentRegistry();
};

struct AccessibilityTreeContext
{
public:
    std::shared_ptr<AccessibilityTree> accessibilityTree;
    std::shared_ptr<AccessibilityTreeEventsConsumer> accessibilityTreeEventsConsumer;
    std::shared_ptr<AccessibilityXComponentRegistry> accessibilityXComponentRegistry;
};

AccessibilityTreeContext makeAccessibilityTreeContext();

std::shared_ptr<AccessibilityTreeEventsConsumer>
makeGeometryValidatingAccessibilityTreeEventsConsumerDecorator(
    std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer);

std::shared_ptr<AccessibilityTreeEventsConsumer>
makeAccessibilityTreeEventsConsumerJsDecorator(std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer);

std::unique_ptr<AccessibilityActionsConsumer>
makeAccessibilityActionsConsumer(std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer);

std::unique_ptr<AccessibilityActionsConsumer>
makeAccessibilityActionsQtDecorator(std::shared_ptr<AccessibilityActionsConsumer> actionsConsumer);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // ACCESSIBILITYTREE_H
