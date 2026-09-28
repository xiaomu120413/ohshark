// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSQPAFUNCTIONSIMPL_H
#define QOHOSQPAFUNCTIONSIMPL_H

#include <QtCore/private/qohosqpafunctions_p.h>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QtOhos {

std::shared_ptr<QOhosQpaFunctions> makeQOhosQpaFunctionsImpl();

}

QT_END_NAMESPACE

#endif // QOHOSQPAFUNCTIONSIMPL_H
