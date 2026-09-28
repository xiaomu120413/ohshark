// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYNODEHELPERS_H
#define QOHOSACCESSIBILITYNODEHELPERS_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtGui/qaccessible.h>
#include <QtCore/qrect.h>
#include <accessibility/qohosaccessibilitytree.h>
#include <accessibility/qohosaccessiblewrappers.h>

namespace QOhos {

QRect getRelativeGeometryOrNull(QAccessibleInterface *interface);

QOhos::AccessibilityNode::ValueInfo makeValueInfo(QAccessibleInterface *interface);

QOhos::AccessibilityNode makeAccessibilityNode(QAccessibleInterface *interface);

bool isTableInterfaceRole(QAccessible::Role role);

std::string getNameTextOrFallbackText(const QAccessibleInterface &interface);

}

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBILITYNODEHELPERS_H
