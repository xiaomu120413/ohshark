// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/private/qohoslogger_p.h>
#include <accessibility/qohosaccessibilityevents.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

QOhosAccessiblePlatformEvent::QOhosAccessiblePlatformEvent(
    QObject *object, OhosAccessiblePlatformEventType platformEventType)
    : QAccessibleEvent(object, QAccessible::Event::OhosPlatformEvent)
    , m_platformEventType(platformEventType)
{
}

OhosAccessiblePlatformEventType QOhosAccessiblePlatformEvent::platformEventType() const
{
    return m_platformEventType;
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
