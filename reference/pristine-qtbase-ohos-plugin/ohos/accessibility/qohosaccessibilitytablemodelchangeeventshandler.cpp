// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <accessibility/qohosaccessibilitynodehelpers.h>
#include <accessibility/qohosaccessibilitytablemodelchangeeventshandler.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

void handleTableModelChangeEvent(
    AccessibleEventsConsumer *accessibleEventsConsumer,
    AccessibleInterfacesChildrenTracker &interfacesChildrenTracker,
    const QAccessibleTableModelChangeEventInfo &eventInfo)
{
    if (eventInfo.tableInterfaceWrapper->tableInterface() == nullptr) {
        qOhosPrintfDebug(
            "%s: ignoring table event for interface %u that isn't a table interface.", 
            Q_FUNC_INFO, QAccessibleWrapper::uniqueId(eventInfo.tableInterfaceWrapper));
        return;
    }

    switch (eventInfo.modelChangeType) {
    case QAccessibleTableModelChangeEvent::ModelChangeType::ColumnsInserted:
    case QAccessibleTableModelChangeEvent::ModelChangeType::RowsInserted:
    case QAccessibleTableModelChangeEvent::ModelChangeType::ColumnsRemoved:
    case QAccessibleTableModelChangeEvent::ModelChangeType::RowsRemoved:
    case QAccessibleTableModelChangeEvent::ModelChangeType::ModelReset:
        interfacesChildrenTracker.syncStoredChildrenWithQAccessible(
            QAccessibleWrapper::uniqueId(eventInfo.tableInterfaceWrapper),
            [&](std::set<QAccessible::Id> removedCellsIds, std::set<QAccessible::Id> addedCellsIds) {
                for (auto removedCellId : removedCellsIds) {
                    accessibleEventsConsumer->notifyInterfaceRemoved(removedCellId);
                }
                for (auto addedCellId : addedCellsIds) {
                    accessibleEventsConsumer->notifyInterfaceAdded(addedCellId);
                }
            });
        break;
    case QAccessibleTableModelChangeEvent::ModelChangeType::DataChanged: {
        auto storedCellsIds = interfacesChildrenTracker.getStoredChildrenIds(
            QAccessibleWrapper::uniqueId(eventInfo.tableInterfaceWrapper));

        for (auto storedCellId : storedCellsIds) {
            accessibleEventsConsumer->notifyInterfaceNameChanged(storedCellId);
            accessibleEventsConsumer->notifyInterfaceHelpChanged(storedCellId);
            accessibleEventsConsumer->notifyInterfaceDescriptionChanged(storedCellId);
            accessibleEventsConsumer->notifyInterfaceAccessibleStateChanged(storedCellId);
        }
        break;
    }
    }
}

}

#endif // QT_NO_ACCESSIBILITY
