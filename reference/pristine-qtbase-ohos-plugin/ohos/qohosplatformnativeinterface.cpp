// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosplatformnativeinterface.h"
#include "qohosplatformclipboard.h"
#include "qohosplatformintegration.h"
#include "qohosplatformwindow.h"
#include <qohosjsenv_p.h>

#include <QtCore/qmutex.h>
#include <QtGui/private/qguiapplication_p.h>
QT_BEGIN_NAMESPACE

void QOhosPlatformNativeInterface::customEvent(QEvent *event)
{
    auto __dbg = make_QCScopedDebug("QOhosPlatformNativeInterface::customEvent");
    if (event->type() != QEvent::User)
        return;

#ifndef QT_NO_ACCESSIBILITY
    // HACK
    // Ohos accessibility activation event might have been already received
    // api->accessibility()->setActive(QtOhosAccessibility::isActive());
#endif // QT_NO_ACCESSIBILITY
}

QFunctionPointer QOhosPlatformNativeInterface::platformFunction(const QByteArray &functionName) const
{
    if (functionName == "tagWindowOrWidgetAsSubWindowOf") {
        return reinterpret_cast<QFunctionPointer>(&QOhosPlatformWindow::tagWindowOrWidgetAsSubWindowOf);
    } else if (functionName == "getWindowOrWidgetAsSubWindowOfTagValue") {
        return reinterpret_cast<QFunctionPointer>(&QOhosPlatformWindow::getWindowOrWidgetAsSubWindowOfTagValue);
    } else if (functionName == "tagWindowOrWidgetAsMainWindow") {
        return reinterpret_cast<QFunctionPointer>(&QOhosPlatformWindow::tagWindowOrWidgetAsMainWindow);
    } else if (functionName == "setInAppOnlyPasteboardShareOption") {
        return reinterpret_cast<QFunctionPointer>(
            &QOhosPlatformClipboard::setInAppOnlyPasteboardShareOption);
    } else if (functionName == "tagWidgetWindowAsHostOfWidget") {
        return reinterpret_cast<QFunctionPointer>(&QOhosPlatformWindow::tagWidgetWindowAsHostOfWidget);
    } else if (functionName == "setSurfaceConsumer") {
        return reinterpret_cast<QFunctionPointer>(&QOhosPlatformWindow::setSurfaceConsumer);
    }

    return QPlatformNativeInterface::platformFunction(functionName);
}

QT_END_NAMESPACE
