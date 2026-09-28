// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSPLATFORMACCESSIBILITY_H
#define QOHOSPLATFORMACCESSIBILITY_H

#include "qohosaccessibilitytree.h"

#include <QtCore/private/qohoscommon_p.h>
#include <QtGui/qaccessible.h>
#include <accessibility/qohosaccessibleeventsconsumer.h>
#include <accessibility/qohosaccessiblewidgetinterfacesregistry.h>
#include <accessibility/qohosaccessiblewrappers.h>
#include <accessibility/qohosaccessibilitymenuactions.h>
#include <functional>
#include <memory>
#include <qpa/qplatformaccessibility.h>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

class QOhosPlatformAccessibility : public QPlatformAccessibility
{
public:
    QOhosPlatformAccessibility(
        std::shared_ptr<QOhos::AccessibilityTreeEventsConsumer> accessibilityTreeEventsConsumer);
    void notifyAccessibilityUpdate(QAccessibleEvent *event) override;
    void setRootObject(QObject *qObject) override;

    void notifyAccessibleInterfaceDeleted(QAccessible::Id interfaceId);

private:
    void synthesizeCommonChildrenEventsFromParentEventIfNecessary(
        const QOhos::QAccessibleEventWithWrappedInterfaceHolder &eventAndInterfaceHolder,
        const std::function<bool(QAccessible::Role)> &parentRolePredicate,
        const std::function<bool(QAccessible::Role)> &childRolePredicate);

    void handleDestroyedInterface(QAccessible::Id destroyedInterfaceId);

    void notifyAccessibilityUpdateImpl(
        const QOhos::QAccessibleEventWithWrappedInterfaceHolder &eventAndInterfaceHolder);

    std::shared_ptr<QOhos::AccessibleEventsConsumer> m_accessibleEventsConsumer;
    QOhosConsumer<std::function<void()>> m_accessibleTasksHandler;
    QOhos::AccessibleWidgetInterfacesRegistry m_widgetInterfacesRegistry;
    QOhos::AccessibleInterfacesChildrenTracker m_interfacesChildrenTracker;

    bool m_isInitialized { false };
};

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif
