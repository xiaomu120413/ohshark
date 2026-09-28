// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosplatformaccessibility.h"

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/qaccessible.h>
#include <QtGui/qwindow.h>
#include <accessibility/qohosaccessibilityevents.h>
#include <accessibility/qohosaccessibilitymenuactions.h>
#include <accessibility/qohosaccessibilitynodehelpers.h>
#include <accessibility/qohosaccessibilitysafeeventcopysupplier.h>
#include <accessibility/qohosaccessibilitytabbars.h>
#include <accessibility/qohosaccessibilitytablemodelchangeeventshandler.h>
#include <qohosplugincore.h>
#include <render/qohosbatchingrequestshandler.h>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

namespace {

bool isMenuLikeRole(QAccessible::Role role)
{
    return role == QAccessible::Role::MenuBar || role == QAccessible::Role::PopupMenu;
}

bool shouldBeAddedWithAccessibleEventsLater(const QAccessibleInterface &interface)
{
    return QOhos::isTableInterfaceRole(interface.role())
        || isMenuLikeRole(interface.role())
        || interface.role() == QAccessible::Role::PageTabList;
}

void insertChildrenRecursively(
    QOhos::AccessibleEventsConsumer &accessibleEventsConsumer,
    QAccessibleInterface *interface)
{
    const auto childCount = interface->childCount();
    for (int i = 0; i < childCount; ++i) {
        auto *childInterface = interface->child(i);
        auto childNode = QOhos::makeAccessibilityNode(childInterface);
        if (shouldBeAddedWithAccessibleEventsLater(*childInterface)) {
            continue;
        }
        accessibleEventsConsumer.notifyInterfaceAdded(
            QOhos::QAccessibleWrapper::uniqueId(childInterface));
        insertChildrenRecursively(accessibleEventsConsumer, childInterface);
    }
}

void mapAccessibilityTree(
    QOhos::AccessibleEventsConsumer &accessibleEventsConsumer,
    QAccessibleInterface *interface)
{
    if (interface != nullptr) {
        accessibleEventsConsumer.notifyInterfaceAdded(
            QOhos::QAccessibleWrapper::uniqueId(interface));
        insertChildrenRecursively(accessibleEventsConsumer, interface);
    }
}

bool isTableEntryRole(QAccessible::Role role)
{
    return role == QAccessible::Role::Cell
        || role == QAccessible::Role::ListItem
        || role == QAccessible::Role::TreeItem;
}

bool representsWidgetInterface(QAccessibleInterface *interface)
{
    return interface->object() != nullptr && interface->object()->isWidgetType();
}

bool isEligibleForChildrenTracking(QAccessible::Role role)
{
    return isMenuLikeRole(role) || QOhos::isTableInterfaceRole(role)
        || role == QAccessible::Role::PageTabList;
}

}

QOhosPlatformAccessibility::QOhosPlatformAccessibility(
    std::shared_ptr<QOhos::AccessibilityTreeEventsConsumer> accessibilityTreeEventsConsumer)
    : m_accessibleEventsConsumer(
        QOhos::makeAccessibleEventsConsumer(std::move(accessibilityTreeEventsConsumer)))
{
    m_accessibleTasksHandler =
        makeQtOhosSimpleBatchingMTRequestsHandler<std::function<void()>>(
            &QtOhos::invokeInQtThread,
            [](auto tasksBatch) {
                QOhos::QAccessibleWrapper::startSession(
                    [&]() {
                        for (auto &task : tasksBatch) {
                            task();
                        }
                    });
            });

    setActive(true);
}

void QOhosPlatformAccessibility::notifyAccessibilityUpdate(QAccessibleEvent *event)
{
    m_accessibleTasksHandler(
        [this, eventSupplier = QOhos::makeSafeQAccessibleEventCopySupplier(*event)]() {
            auto accessibleEvent = eventSupplier();
            if (accessibleEvent) {
                auto interfaceId = accessibleEvent->uniqueId();
                notifyAccessibilityUpdateImpl(
                    {
                        .event = std::move(accessibleEvent),
                        .interfaceWrapper = QOhos::QAccessibleWrapper::accessibleInterface(interfaceId),
                    });
            } else {
                qOhosPrintfDebug("Ignoring an expired QAccessible event...");
            }
        });
}

void QOhosPlatformAccessibility::notifyAccessibleInterfaceDeleted(QAccessible::Id interfaceId)
{
    m_accessibleTasksHandler(
        [this, interfaceId]() {
            handleDestroyedInterface(interfaceId);
        });
}

void QOhosPlatformAccessibility::synthesizeCommonChildrenEventsFromParentEventIfNecessary(
    const QOhos::QAccessibleEventWithWrappedInterfaceHolder &eventAndInterfaceHolder,
    const std::function<bool(QAccessible::Role)> &parentRolePredicate,
    const std::function<bool(QAccessible::Role)> &childRolePredicate)
{
    auto *interface = eventAndInterfaceHolder.interfaceWrapper;

    if (!parentRolePredicate(interface->role())) {
        return;
    }

    auto storedChildrenIds = m_interfacesChildrenTracker.getStoredChildrenIds(
        QOhos::QAccessibleWrapper::uniqueId(interface));

    for (auto storedChildId : storedChildrenIds) {
        auto *childInterface = QOhos::QAccessibleWrapper::accessibleInterface(storedChildId);

        if (!childRolePredicate(childInterface->role())) {
            continue;
        }

        auto childInterfaceId = QOhos::QAccessibleWrapper::uniqueId(childInterface);
        switch (eventAndInterfaceHolder.event->type()) {
        case QAccessible::LocationChanged:
            m_accessibleEventsConsumer->notifyInterfaceGeometryChanged(childInterfaceId);
            m_accessibleEventsConsumer->notifyInterfaceAccessibleStateChanged(childInterfaceId);
            break;
        case QAccessible::StateChanged:
        case QAccessible::ObjectShow:
        case QAccessible::ObjectHide:
            m_accessibleEventsConsumer->notifyInterfaceAccessibleStateChanged(childInterfaceId);
            break;
        default:
            break;
        }
    }
}

void QOhosPlatformAccessibility::handleDestroyedInterface(QAccessible::Id destroyedInterfaceId)
{
    if (m_widgetInterfacesRegistry.tracksInterfaceWithId(destroyedInterfaceId)) {
        auto widgetInterfaceRole =
            m_widgetInterfacesRegistry
                .getWidgetInterfaceInfoWithIdOrFail(destroyedInterfaceId).role;

        if (isEligibleForChildrenTracking(widgetInterfaceRole)) {
            m_interfacesChildrenTracker.removeTree(destroyedInterfaceId);
        }

        m_widgetInterfacesRegistry.unregisterWidgetInterfaceWithIdOrFail(
            destroyedInterfaceId);
        m_accessibleEventsConsumer->notifyInterfaceRemoved(destroyedInterfaceId);
    }
}

void QOhosPlatformAccessibility::notifyAccessibilityUpdateImpl(
    const QOhos::QAccessibleEventWithWrappedInterfaceHolder &eventAndInterfaceHolder)
{
    if (!m_isInitialized) {
        return;
    }

    auto *event = eventAndInterfaceHolder.event.get();
    auto *interface = eventAndInterfaceHolder.interfaceWrapper;

    if (!interface->isValid()) {
        return;
    }

    switch (event->type()) {
    case QAccessible::OhosPlatformEvent: {
        auto ohosPlatformEventType =
            static_cast<QOhos::QOhosAccessiblePlatformEvent *>(event)->platformEventType();

        switch (ohosPlatformEventType) {
        case QOhos::OhosAccessiblePlatformEventType::ChildrenSync: 
        case QOhos::OhosAccessiblePlatformEventType::ChildrenUpdate:
            if (isMenuLikeRole(interface->role())) {
                QOhos::handleQAccessibleMenuActionsEvent(
                    *m_accessibleEventsConsumer, m_interfacesChildrenTracker,
                    {
                        .menuLikeInterface = interface,
                        .actionEventType = QOhos::mapOhosPlatformEventTypeToActionEventType(
                            ohosPlatformEventType),
                    });
            } else if (interface->role() == QAccessible::Role::PageTabList) {
                QOhos::handleQAccessibleTabBarEvent(
                    *m_accessibleEventsConsumer, m_interfacesChildrenTracker,
                    {
                        .tabBarInterface = interface,
                        .tabBarEventType = QOhos::mapOhosPlatformEventTypeToTabBarEventType(
                            ohosPlatformEventType),
                    });
            }
        }
        break;
    }
    case QAccessible::TableModelChanged: {
        auto *tableModelChangeEvent = static_cast<QAccessibleTableModelChangeEvent *>(event);
        QOhos::handleTableModelChangeEvent(
            m_accessibleEventsConsumer.get(),
            m_interfacesChildrenTracker,
            {
                .modelChangeType = tableModelChangeEvent->modelChangeType(),
                .firstRow = tableModelChangeEvent->firstRow(),
                .firstColumn = tableModelChangeEvent->firstColumn(),
                .lastRow = tableModelChangeEvent->lastRow(),
                .lastColumn = tableModelChangeEvent->lastColumn(),
                .tableInterfaceWrapper = interface,
            });
        break;
    }
    case QAccessible::ObjectCreated:
        if (representsWidgetInterface(interface)) {
            m_widgetInterfacesRegistry.registerWidgetInterfaceWithIdOrFail(
                QOhos::QAccessibleWrapper::uniqueId(interface));
        }

        if (isEligibleForChildrenTracking(interface->role())) {
            m_interfacesChildrenTracker.addTree(event->uniqueId());
        }

        m_accessibleEventsConsumer->notifyInterfaceAdded(event->uniqueId());
        break;
    case QAccessible::LocationChanged:
        m_accessibleEventsConsumer->notifyInterfaceGeometryChanged(event->uniqueId());
        break;
    case QAccessible::NameChanged:
        m_accessibleEventsConsumer->notifyInterfaceNameChanged(event->uniqueId());
        break;
    case QAccessible::HelpChanged:
        m_accessibleEventsConsumer->notifyInterfaceHelpChanged(event->uniqueId());
        break;
    case QAccessible::DescriptionChanged:
        m_accessibleEventsConsumer->notifyInterfaceDescriptionChanged(event->uniqueId());
        break;
    case QAccessible::ValueChanged:
        m_accessibleEventsConsumer->notifyInterfaceValueChanged(event->uniqueId());
        break;
    case QAccessible::ObjectShow:
    case QAccessible::ObjectHide:
        m_accessibleEventsConsumer->notifyInterfaceVisibilityChanged(event->uniqueId());
        break;
    case QAccessible::StateChanged:
        m_accessibleEventsConsumer->notifyInterfaceAccessibleStateChanged(event->uniqueId());
        break;
    case QAccessible::ParentChanged:
        if (interface->parent() != nullptr) {
            m_accessibleEventsConsumer->notifyInterfaceGeometryChanged(event->uniqueId());
            m_accessibleEventsConsumer->notifyInterfaceParentChanged(event->uniqueId());
        }
        break;
    case QAccessible::Focus:
        m_accessibleEventsConsumer->notifyInterfaceFocused(event->uniqueId());
        break;
    default:
        break;
    }

    synthesizeCommonChildrenEventsFromParentEventIfNecessary(
        eventAndInterfaceHolder, &QOhos::isTableInterfaceRole, &isTableEntryRole);

    synthesizeCommonChildrenEventsFromParentEventIfNecessary(
        eventAndInterfaceHolder, &isMenuLikeRole,
        [](QAccessible::Role childRole) {
            return childRole == QAccessible::MenuItem;
        });

    synthesizeCommonChildrenEventsFromParentEventIfNecessary(
        eventAndInterfaceHolder,
        [](QAccessible::Role parentRole) {
            return parentRole == QAccessible::PageTabList;
        },
        [](QAccessible::Role childRole) {
            return childRole == QAccessible::PageTab;
        });
}

void QOhosPlatformAccessibility::setRootObject(QObject *qObject)
{
    QPlatformAccessibility::setRootObject(qObject);
    QOhos::QAccessibleWrapper::startSession(
        [&]() {
            auto *interface = QOhos::QAccessibleWrapper::accessibleInterface(
                QAccessible::uniqueId(QAccessible::queryAccessibleInterface(qObject)));
            mapAccessibilityTree(*m_accessibleEventsConsumer.get(), interface);
        });
    m_isInitialized = true;
}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
