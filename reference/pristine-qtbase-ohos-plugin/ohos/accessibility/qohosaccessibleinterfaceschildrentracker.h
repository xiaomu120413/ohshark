// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBLEINTERFACESCHILDRENTRACKER_H
#define QOHOSACCESSIBLEINTERFACESCHILDRENTRACKER_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <map>
#include <set>

QT_BEGIN_NAMESPACE

namespace QOhos {

class AccessibleInterfacesChildrenTracker
{
public:
    void addTree(QAccessible::Id treeId);
    void removeTree(QAccessible::Id treeId);
    void syncStoredChildrenWithQAccessible(
        QAccessible::Id treeId,
        std::function<void(std::set<QAccessible::Id>, std::set<QAccessible::Id>)> removedAddedChildrenConsumer);
    std::set<QAccessible::Id> getStoredChildrenIds(QAccessible::Id treeId) const;

private:
    std::map<QAccessible::Id, std::set<QAccessible::Id>> m_rootNodeToChildrenIdsMap;
};

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBLEINTERFACESCHILDRENTRACKER_H
