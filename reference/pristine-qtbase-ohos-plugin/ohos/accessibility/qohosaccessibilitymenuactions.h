// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYMENUACTIONS_H
#define QOHOSACCESSIBILITYMENUACTIONS_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <accessibility/qohosaccessibilityevents.h>
#include <accessibility/qohosaccessibleeventsconsumer.h>
#include <accessibility/qohosaccessibleinterfaceschildrentracker.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

struct QAccessibleActionEventInfo
{
    enum class ActionEventType 
    {
        ActionsSync,
        ActionsUpdate,
    };

    QAccessibleInterface *menuLikeInterface;
    ActionEventType actionEventType;
};

void handleQAccessibleMenuActionsEvent(
    AccessibleEventsConsumer &accessibleEventsConsumer,
    AccessibleInterfacesChildrenTracker &interfacesChildrenTracker,
    const QAccessibleActionEventInfo &actionEvent);

QAccessibleActionEventInfo::ActionEventType mapOhosPlatformEventTypeToActionEventType(
    OhosAccessiblePlatformEventType ohosPlatformEventType);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBILITYMENUACTIONS_H
