// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYSAFEEVENTCOPYSUPPLIER_H
#define QOHOSACCESSIBILITYSAFEEVENTCOPYSUPPLIER_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>
#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QOhos {

QOhosSupplier<std::shared_ptr<QAccessibleEvent>> makeSafeQAccessibleEventCopySupplier(
    const QAccessibleEvent &event);

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif
