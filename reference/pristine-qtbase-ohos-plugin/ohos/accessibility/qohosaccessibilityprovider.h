// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef OHOSACCESSIBILITYPROVIDER_H
#define OHOSACCESSIBILITYPROVIDER_H

#ifndef QT_NO_ACCESSIBILITY

#include "qohosaccessibilitytree.h"

#include <memory>
#include <render/qxcomponent.h>

namespace QOhos {

::ArkUI_AccessibilityProvider *getNativeAccessibilityProviderFromXComponentOrFail(
    QXComponentRender xComponent);

std::shared_ptr<void> tryInitializeAccessibilityProvider(
    QXComponentRender xComponent, std::shared_ptr<AccessibilityTree> accessibilityTree,
    std::shared_ptr<AccessibilityXComponentRegistry> accessibilityXComponentRegistry,
    std::shared_ptr<AccessibilityActionsConsumer> accessibilityActionsConsumer,
    AccessibilityNode::Id windowAccessibilityId);

}

#endif // QT_NO_ACCESSIBILITY

#endif // OHOSACCESSIBILITYPROVIDER_H
