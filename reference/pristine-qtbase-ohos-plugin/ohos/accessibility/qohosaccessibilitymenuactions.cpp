// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <accessibility/qohosaccessibilitymenuactions.h>
#include <accessibility/qohosaccessibilitynodehelpers.h>
#include <accessibility/qohosaccessiblewrappers.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

bool representsMenuBarHiddenActionsMenu(QAccessibleInterface *interface)
{
    if (interface->object() == nullptr) {
        return false;
    }

    if (qstrcmp(interface->object()->metaObject()->className(), "QMenu") != 0) {
        return false;
    }

    auto *menuParentObject = interface->object()->parent();
    if (menuParentObject == nullptr) {
        return false;
    }

    if (qstrcmp(menuParentObject->metaObject()->className(), "QMenuBar") != 0) {
        return false;
    }

    auto *menuBarInterface = QOhos::QAccessibleWrapper::accessibleInterface(
        QAccessible::uniqueId(QAccessible::queryAccessibleInterface(menuParentObject)));

    for (int i = 0; i < menuBarInterface->childCount(); ++i) {
        auto *childMenuItem = menuBarInterface->child(i);
        auto *childMenuItemRelatedSubMenu = childMenuItem->child(0);
        if (childMenuItemRelatedSubMenu != nullptr
            && childMenuItemRelatedSubMenu->object() == interface->object()) {
            return false;
        }
    }

    return true;
}

}

void handleQAccessibleMenuActionsEvent(
    AccessibleEventsConsumer &accessibleEventsConsumer,
    AccessibleInterfacesChildrenTracker &interfacesChildrenTracker,
    const QAccessibleActionEventInfo &actionEventInfo)
{
    if (representsMenuBarHiddenActionsMenu(actionEventInfo.menuLikeInterface)) {
        qOhosPrintfDebug(
            "%s: ignoring QMenuBar's QMenu %d with hidden actions. This functionality is unsupported by QAccessibleMenuBar.",
            Q_FUNC_INFO, QAccessibleWrapper::uniqueId(actionEventInfo.menuLikeInterface));
        return;
    }

    auto menuLikeInterfaceId = QAccessibleWrapper::uniqueId(actionEventInfo.menuLikeInterface);

    switch (actionEventInfo.actionEventType) {
    case QAccessibleActionEventInfo::ActionEventType::ActionsSync:
        interfacesChildrenTracker.syncStoredChildrenWithQAccessible(
            menuLikeInterfaceId,
            [&](std::set<QAccessible::Id> removedActionsIds, std::set<QAccessible::Id> addedActionsIds) {
                for (auto removedActionId : removedActionsIds) {
                    accessibleEventsConsumer.notifyInterfaceRemoved(removedActionId);
                }
                for (auto addedActionId : addedActionsIds) {
                    accessibleEventsConsumer.notifyInterfaceAdded(addedActionId);
                }
            });
        break;

    case QAccessibleActionEventInfo::ActionEventType::ActionsUpdate:
        for (auto actionId : interfacesChildrenTracker.getStoredChildrenIds(menuLikeInterfaceId)) {
            accessibleEventsConsumer.notifyInterfaceAccessibleStateChanged(actionId);
            accessibleEventsConsumer.notifyInterfaceGeometryChanged(actionId);
            accessibleEventsConsumer.notifyInterfaceNameChanged(actionId);
        }
        break;
    }
}

QAccessibleActionEventInfo::ActionEventType mapOhosPlatformEventTypeToActionEventType(
    OhosAccessiblePlatformEventType ohosPlatformEventType)
{
    switch (ohosPlatformEventType) {
    case OhosAccessiblePlatformEventType::ChildrenSync:
        return QAccessibleActionEventInfo::ActionEventType::ActionsSync;
    case OhosAccessiblePlatformEventType::ChildrenUpdate:
        return QAccessibleActionEventInfo::ActionEventType::ActionsUpdate;
    };

    qOhosReportFatalErrorAndAbort(
        "%s: cannot map platform event type %d to ActionEventType", 
        Q_FUNC_INFO, ohosPlatformEventType);
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
