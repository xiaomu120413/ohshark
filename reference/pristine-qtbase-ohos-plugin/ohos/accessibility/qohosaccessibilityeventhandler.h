// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef ACCESSIBILITYEVENTHANDLER_H
#define ACCESSIBILITYEVENTHANDLER_H

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qobject.h>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QOhos {

std::unique_ptr<QObject> makeAccessibilityEventHandler();

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY

#endif // ACCESSIBILITYEVENTHANDLER_H
