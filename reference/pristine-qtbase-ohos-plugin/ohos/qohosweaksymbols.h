// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSWEAKSYMBOLS_H
#define QOHOSWEAKSYMBOLS_H

QT_BEGIN_NAMESPACE

/*
    Re-declares an already-declared C function as a weak symbol.

    The OHOS SDK headers do not annotate when each function was
    introduced, so a Qt binary built against a newer SDK may reference
    symbols that are absent from the system libraries on older devices.
    Without weak linkage, the dynamic loader would refuse to load the
    Qt library on such devices.

    This macro must be invoked at namespace scope, after the SDK header
    declaring `name` has been included. The function's signature is
    borrowed from the existing declaration via decltype, so the SDK
    header remains the single source of truth for the prototype.
*/
#define Q_OHOS_REDECLARE_C_FUNC_AS_WEAK_SYMBOL(name) \
    extern "C" __attribute__((weak)) decltype(name) name;

QT_END_NAMESPACE

#endif
