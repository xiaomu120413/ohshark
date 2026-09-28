// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QXCOMPONENTREGISTRY_H
#define QXCOMPONENTREGISTRY_H

#include <QtCore/private/qnapi_p.h>
#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qglobal.h>
#include <map>
#include <optional>
#include <render/qxcomponent.h>

QT_BEGIN_NAMESPACE

namespace QArkUi {

class QXComponentRegistry
{
public:
    static QXComponentRegistry &instance();

    std::optional<QXComponentNode> tryTakeNodeByXComponentId(const QXComponentId &id);

    QXComponentRegistry(const QXComponentRegistry &) = delete;
    QXComponentRegistry(QXComponentRegistry &&) = delete;
    QXComponentRegistry &operator=(const QXComponentRegistry &) = delete;
    QXComponentRegistry &operator=(QXComponentRegistry &&) = delete;

    static bool Init(napi_env env, napi_value exports);

private:
    QXComponentRegistry() = default;

    std::map<QXComponentId, QXComponentNode> m_xComponents;
};

// Returns the parent XComponent's native window surface (allocated by
// the ArkTS <XComponent> component), or nullptr if not yet allocated.
OHNativeWindow *parentSurfaceOrNull();

// Returns the native window surface captured from the ArkTS SURFACE-type
// XComponent ("<id>_surf") embedded in a subwindow / floating-window page,
// keyed by the full "_surf"-suffixed id, or nullptr if not yet allocated.
OHNativeWindow *capturedSurfaceOrNull(const std::string &surfXComponentId);

}

QT_END_NAMESPACE

#endif
