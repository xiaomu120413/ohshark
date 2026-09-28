// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYTREEEVENTSCONSUMERCOMMAND_H
#define QOHOSACCESSIBILITYTREEEVENTSCONSUMERCOMMAND_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <accessibility/qohosaccessibilitytree.h>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QOhos {

class AccessibilityTreeEventsConsumerCommand
{
public:
    enum class Type
    {
        AddNode,
        ProcessAccessibilityEvent,
        ProcessAccessibilityWindowEvent,
        RemoveNodeAndTryNotifyWindow,
        UpdateNodeDescription,
        UpdateNodeGeometry,
        UpdateNodeHelp,
        UpdateNodeName,
        UpdateNodeParent,
        UpdateNodeState,
        UpdateNodeValue,
    };

    virtual ~AccessibilityTreeEventsConsumerCommand();

    virtual void apply(AccessibilityTreeEventsConsumer &consumer) const = 0;

    virtual Type getType() const = 0;
    virtual AccessibilityNode::Id getTargetNodeId() const = 0;
    virtual QOhosOptional<AccessibilityEvent> getOptEventType() const = 0;
    virtual QOhosOptional<AccessibilityWindowEvent> getOptWindowEventType() const = 0;

protected:
    AccessibilityTreeEventsConsumerCommand();
};

std::shared_ptr<AccessibilityTreeEventsConsumer> makeAccessibilityTreeEventsConsumerCommandsRecorder(
    QOhosConsumer<std::unique_ptr<AccessibilityTreeEventsConsumerCommand>> commandsConsumer);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif
