// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBLEWRAPPERS_H
#define QOHOSACCESSIBLEWRAPPERS_H

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpair.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtCore/qvector.h>
#include <QtGui/qaccessible.h>
#include <QtGui/qwindow.h>
#include <memory>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

namespace QOhos {

struct QAccessibleEventWithWrappedInterfaceHolder
{
    std::shared_ptr<QAccessibleEvent> event;
    QAccessibleInterface *interfaceWrapper;
};

namespace QAccessibleWrapper {

    void startSession(const std::function<void()> &task);

    QAccessible::Id uniqueId(QAccessibleInterface *interfaceWrapper);

    QAccessibleInterface *accessibleInterface(QAccessible::Id id);

};

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif
