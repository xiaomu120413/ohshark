// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYEVENTS_H
#define QOHOSACCESSIBILITYEVENTS_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qglobal.h>
#include <QtGui/qaccessible.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

enum class OhosAccessiblePlatformEventType
{
    ChildrenSync,
    ChildrenUpdate,
};

class QOhosAccessiblePlatformEvent : public QAccessibleEvent
{
public:
    QOhosAccessiblePlatformEvent(QObject *object, OhosAccessiblePlatformEventType platformEventType);

    OhosAccessiblePlatformEventType platformEventType() const;

private:
    OhosAccessiblePlatformEventType m_platformEventType;
};

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif
