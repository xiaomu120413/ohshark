// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBLEEVENTSCONSUMER_H
#define QOHOSACCESSIBLEEVENTSCONSUMER_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <accessibility/qohosaccessibilitytree.h>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QOhos {

class AccessibleEventsConsumer
{
public:
    virtual ~AccessibleEventsConsumer();

    virtual void notifyInterfaceAdded(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceRemoved(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceGeometryChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceNameChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceHelpChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceDescriptionChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceValueChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceVisibilityChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceAccessibleStateChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceParentChanged(QAccessible::Id interfaceId) = 0;
    virtual void notifyInterfaceFocused(QAccessible::Id interfaceId) = 0;
};

std::shared_ptr<AccessibleEventsConsumer> makeAccessibleEventsConsumer(
    std::shared_ptr<AccessibilityTreeEventsConsumer> treeEventsConsumer);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBLEEVENTSCONSUMER_H
