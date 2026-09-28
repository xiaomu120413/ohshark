// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoslogger_p.h>
#include <accessibility/qohosaccessibleinterfaceschildrentracker.h>
#include <accessibility/qohosaccessiblewrappers.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

template<typename T>
std::set<T> evaluateDifference(const std::set<T> &a, const std::set<T> &b)
{
    std::set<T> result;

    for (const auto &aElement : a) {
        auto it = b.find(aElement);
        if (it == b.end()) {
            result.insert(aElement);
        }
    }

    return result;
}

}

void AccessibleInterfacesChildrenTracker::addTree(QAccessible::Id treeId)
{
    if (m_rootNodeToChildrenIdsMap.find(treeId) != m_rootNodeToChildrenIdsMap.end()) {
        qOhosPrintfError(
            "%s: a tree with provided id %u is already in the register.", Q_FUNC_INFO, treeId);
        return;
    }

    m_rootNodeToChildrenIdsMap[treeId] = {};
}

void AccessibleInterfacesChildrenTracker::removeTree(QAccessible::Id treeId)
{
    if (m_rootNodeToChildrenIdsMap.find(treeId) == m_rootNodeToChildrenIdsMap.end()) {
        qOhosPrintfError(
            "%s: cannot remove non-existing tree with id %u the register.", Q_FUNC_INFO, treeId);
        return;
    }

    m_rootNodeToChildrenIdsMap.erase(treeId);
}

void AccessibleInterfacesChildrenTracker::syncStoredChildrenWithQAccessible(
    QAccessible::Id treeId,
    std::function<void(std::set<QAccessible::Id>, std::set<QAccessible::Id>)> removedAddedChildrenConsumer)
{
    if (m_rootNodeToChildrenIdsMap.find(treeId) == m_rootNodeToChildrenIdsMap.end()) {
        qOhosPrintfError(
            "%s: cannot sync children for an untracked tree with id %u.", Q_FUNC_INFO, treeId);
        removedAddedChildrenConsumer({}, {});
        return;
    }

    auto *treeNodeInterface = QAccessibleWrapper::accessibleInterface(treeId);

    std::set<QAccessible::Id> fetchedChildrenIds;
    for (int i = 0; i < treeNodeInterface->childCount(); ++i) {
        fetchedChildrenIds.insert(QAccessibleWrapper::uniqueId(treeNodeInterface->child(i)));
    }

    auto removedChildrenIds =
        evaluateDifference(m_rootNodeToChildrenIdsMap[treeId], fetchedChildrenIds);
    auto addedChildrenIds =
        evaluateDifference(fetchedChildrenIds, m_rootNodeToChildrenIdsMap[treeId]);

    m_rootNodeToChildrenIdsMap[treeId] = fetchedChildrenIds;

    removedAddedChildrenConsumer(removedChildrenIds, addedChildrenIds);
}

std::set<QAccessible::Id> AccessibleInterfacesChildrenTracker::getStoredChildrenIds(
    QAccessible::Id treeId) const
{
    if (m_rootNodeToChildrenIdsMap.find(treeId) == m_rootNodeToChildrenIdsMap.end()) {
        qOhosPrintfError(
            "%s: cannot get children ids of a non-existing tree with id %u.", Q_FUNC_INFO, treeId);
        return {};
    }

    return m_rootNodeToChildrenIdsMap.at(treeId);
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
