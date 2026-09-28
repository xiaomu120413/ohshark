// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSNATIVEDRAGEVENTSHANDLER_H
#define QOHOSNATIVEDRAGEVENTSHANDLER_H

#include <arkui/native_node.h>
#include <QtCore/private/qohoscommon_p.h>
#include <QtGui/qwindow.h>
#include <qohosplugincore.h>
#include <sdkwrapper/arkui/drag_and_drop_qtwrapper.h>

QT_BEGIN_NAMESPACE

QOhosConsumer<::ArkUI_NodeEvent *> makeQOhosNativeDragEventsHandler(
    QtOhos::QThreadSafeRef<QWindow> qWindowRef);

QT_END_NAMESPACE

#endif
