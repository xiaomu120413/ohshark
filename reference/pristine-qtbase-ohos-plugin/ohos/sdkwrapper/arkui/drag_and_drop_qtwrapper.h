// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef DRAG_AND_DROP_QTWRAPPER_H
#define DRAG_AND_DROP_QTWRAPPER_H

#include <arkui/drag_and_drop.h>
#include <info/application_target_sdk_version.h>
#include <qohosweaksymbols.h>

#if OH_CURRENT_API_VERSION >= 24

Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(OH_ArkUI_NotifySuggestedDropOperation)

#endif

#endif
