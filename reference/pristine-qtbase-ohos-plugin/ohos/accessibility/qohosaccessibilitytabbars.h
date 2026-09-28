// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYTABBARS_H
#define QOHOSACCESSIBILITYTABBARS_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <accessibility/qohosaccessibilityevents.h>
#include <accessibility/qohosaccessibleeventsconsumer.h>
#include <accessibility/qohosaccessibleinterfaceschildrentracker.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

struct QAccessibleTabBarEventInfo
{
    enum class TabBarEventType 
    {
        TabBarContentSync,
        TabBarContentUpdate,
    };

    QAccessibleInterface *tabBarInterface;
    TabBarEventType tabBarEventType;
};

void handleQAccessibleTabBarEvent(
    AccessibleEventsConsumer &accessibleEventsConsumer,
    AccessibleInterfacesChildrenTracker &interfacesChildrenTracker,
    const QAccessibleTabBarEventInfo &tabBarEventInfo);

QAccessibleTabBarEventInfo::TabBarEventType mapOhosPlatformEventTypeToTabBarEventType(
    OhosAccessiblePlatformEventType ohosPlatformEventType);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBILITYTABBARS_H
