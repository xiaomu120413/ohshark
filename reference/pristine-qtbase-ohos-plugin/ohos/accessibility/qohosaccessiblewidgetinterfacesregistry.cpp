// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <accessibility/qohosaccessiblewidgetinterfacesregistry.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

void AccessibleWidgetInterfacesRegistry::registerWidgetInterfaceWithIdOrFail(QAccessible::Id widgetInterfaceId)
{
    auto *interface = QAccessible::accessibleInterface(widgetInterfaceId);

    if (interface == nullptr) {
        qOhosReportFatalErrorAndAbort("%s: interface is nullptr.", Q_FUNC_INFO);
    }

    if (interface->object() == nullptr || !interface->object()->isWidgetType()) {
        qOhosReportFatalErrorAndAbort(
            "%s: interface %p does not represent a widget.", Q_FUNC_INFO, interface);
    }

    m_widgetInterfaceIdsToInfosMap[widgetInterfaceId] = 
        {
            .role = interface->role(),
        };
}

void AccessibleWidgetInterfacesRegistry::unregisterWidgetInterfaceWithIdOrFail(QAccessible::Id widgetInterfaceId)
{
    if (!tracksInterfaceWithId(widgetInterfaceId)) {
        qOhosReportFatalErrorAndAbort(
            "%s: cannot unregister non-exisiting id %u", Q_FUNC_INFO, widgetInterfaceId);
    }

    m_widgetInterfaceIdsToInfosMap.erase(widgetInterfaceId);
}

bool AccessibleWidgetInterfacesRegistry::tracksInterfaceWithId(QAccessible::Id interfaceId) const
{
    return m_widgetInterfaceIdsToInfosMap.find(interfaceId) != m_widgetInterfaceIdsToInfosMap.end();
}

AccessibleWidgetInterfacesRegistry::WidgetInterfaceInfo
AccessibleWidgetInterfacesRegistry::getWidgetInterfaceInfoWithIdOrFail(
    QAccessible::Id interfaceId) const
{
    if (m_widgetInterfaceIdsToInfosMap.find(interfaceId) == m_widgetInterfaceIdsToInfosMap.end()) {
        qOhosReportFatalErrorAndAbort(
            "%s: interface %u is untracked. Cannot obtain role.", Q_FUNC_INFO, interfaceId);
    }

    return m_widgetInterfaceIdsToInfosMap.at(interfaceId);
}

}

#endif // QT_NO_ACCESSIBILITY
