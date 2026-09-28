// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <accessibility/qohosaccessibilitytabbars.h>
#include <accessibility/qohosaccessibilitynodehelpers.h>
#include <accessibility/qohosaccessiblewrappers.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {
    
bool representsTabLeftRightNavigationWidgetButton(const QAccessibleInterface &interface)
{
    return interface.object() != nullptr
        && interface.object()->isWidgetType()
        && qstrcmp(interface.object()->metaObject()->className(), "QToolButton") == 0;
}

}

void handleQAccessibleTabBarEvent(
    AccessibleEventsConsumer &accessibleEventsConsumer,
    AccessibleInterfacesChildrenTracker &interfacesChildrenTracker,
    const QAccessibleTabBarEventInfo &tabBarEventInfo)
{
    switch (tabBarEventInfo.tabBarEventType) {
    case QAccessibleTabBarEventInfo::TabBarEventType::TabBarContentSync:
        interfacesChildrenTracker.syncStoredChildrenWithQAccessible(
            QAccessibleWrapper::uniqueId(tabBarEventInfo.tabBarInterface),
            [&](std::set<QAccessible::Id> removedChildrenIds, std::set<QAccessible::Id> addedChildrenIds) {
                for (auto removedChildId : removedChildrenIds) {
                    auto *childInterface = QAccessibleWrapper::accessibleInterface(removedChildId);
                    if (representsTabLeftRightNavigationWidgetButton(*childInterface)) {
                        continue;
                    }
                    accessibleEventsConsumer.notifyInterfaceRemoved(removedChildId);
                }
                for (auto addedChildId : addedChildrenIds) {
                    auto *childInterface = QAccessibleWrapper::accessibleInterface(addedChildId);
                    if (representsTabLeftRightNavigationWidgetButton(*childInterface)) {
                        continue;
                    }
                    accessibleEventsConsumer.notifyInterfaceAdded(addedChildId);
                }
            });
        break;
    case QAccessibleTabBarEventInfo::TabBarEventType::TabBarContentUpdate:
        auto tabBarInterfaceId = QAccessibleWrapper::uniqueId(tabBarEventInfo.tabBarInterface);

        for (auto tabBarElementId : interfacesChildrenTracker.getStoredChildrenIds(tabBarInterfaceId)) {
            auto *tabBarElement = QAccessibleWrapper::accessibleInterface(tabBarElementId);
            if (representsTabLeftRightNavigationWidgetButton(*tabBarElement)) {
                continue;
            }

            accessibleEventsConsumer.notifyInterfaceAccessibleStateChanged(tabBarElementId);
            accessibleEventsConsumer.notifyInterfaceGeometryChanged(tabBarElementId);
            accessibleEventsConsumer.notifyInterfaceNameChanged(tabBarElementId);
        }
        break;
    }
}

QAccessibleTabBarEventInfo::TabBarEventType mapOhosPlatformEventTypeToTabBarEventType(
    OhosAccessiblePlatformEventType ohosPlatformEventType)
{
    switch (ohosPlatformEventType) {
    case OhosAccessiblePlatformEventType::ChildrenSync:
        return QAccessibleTabBarEventInfo::TabBarEventType::TabBarContentSync;
    case OhosAccessiblePlatformEventType::ChildrenUpdate:
        return QAccessibleTabBarEventInfo::TabBarEventType::TabBarContentUpdate;
    };

    qOhosReportFatalErrorAndAbort(
        "%s: cannot map platform event type %d to TabBarEventType", 
        Q_FUNC_INFO, ohosPlatformEventType);
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
