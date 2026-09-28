// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYTABLEMODELCHANGEEVENTSHANDLER_H
#define QOHOSACCESSIBILITYTABLEMODELCHANGEEVENTSHANDLER_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <accessibility/qohosaccessiblewrappers.h>
#include <accessibility/qohosaccessibleeventsconsumer.h>
#include <accessibility/qohosaccessibleinterfaceschildrentracker.h>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QOhos {

struct QAccessibleTableModelChangeEventInfo
{
    QAccessibleTableModelChangeEvent::ModelChangeType modelChangeType;
    int firstRow;
    int firstColumn;
    int lastRow;
    int lastColumn;

    QAccessibleInterface *tableInterfaceWrapper;
};

void handleTableModelChangeEvent(
    AccessibleEventsConsumer *accessibleEventsConsumer,
    AccessibleInterfacesChildrenTracker &interfacesChildrenTracker,
    const QAccessibleTableModelChangeEventInfo &eventInfo);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBILITYTABLEMODELCHANGEEVENTSHANDLER_H
