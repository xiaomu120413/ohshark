// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSSAFEQACCESSIBLEINTERFACEWRAPPER_H
#define QOHOSSAFEQACCESSIBLEINTERFACEWRAPPER_H

#ifndef QT_NO_ACCESSIBILITY

#include <accessibility/qohosaccessiblewrappers.h>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QOhos {

std::shared_ptr<QAccessibleInterface> makeSafeQAccessibleInterfaceWrapper(
    std::shared_ptr<QAccessibleInterface> baseInterfaceWrapper,
    QAccessible::Id wrappedInterfaceId);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif
