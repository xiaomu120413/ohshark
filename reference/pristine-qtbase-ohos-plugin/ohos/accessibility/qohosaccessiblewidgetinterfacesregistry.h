// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBLEWIDGETINTERFACESREGISTRY_H
#define QOHOSACCESSIBLEWIDGETINTERFACESREGISTRY_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <map>

QT_BEGIN_NAMESPACE

namespace QOhos {

class AccessibleWidgetInterfacesRegistry
{
public:
    struct WidgetInterfaceInfo
    {
        QAccessible::Role role;
    };

    void registerWidgetInterfaceWithIdOrFail(QAccessible::Id widgetInterfaceId);
    void unregisterWidgetInterfaceWithIdOrFail(QAccessible::Id widgetInterfaceId);

    bool tracksInterfaceWithId(QAccessible::Id interfaceId) const;
    WidgetInterfaceInfo getWidgetInterfaceInfoWithIdOrFail(QAccessible::Id interfaceId) const;

private:
    std::map<QAccessible::Id, WidgetInterfaceInfo> m_widgetInterfaceIdsToInfosMap;
};

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBLEWIDGETINTERFACESREGISTRY_H
