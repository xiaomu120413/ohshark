// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef UI_INPUT_EVENT_QTWRAPPER_H
#define UI_INPUT_EVENT_QTWRAPPER_H

#include <arkui/ui_input_event.h>
#include <info/application_target_sdk_version.h>
#include <qohosweaksymbols.h>

#if OH_CURRENT_API_VERSION >= 22

Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(OH_ArkUI_CoastingAxisEvent_GetDeltaX)
Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(OH_ArkUI_CoastingAxisEvent_GetDeltaY)
Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(OH_ArkUI_CoastingAxisEvent_GetEventTime)
Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(OH_ArkUI_CoastingAxisEvent_GetPhase)
Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(OH_ArkUI_UIInputEvent_GetCoastingAxisEvent)

#endif

#endif
