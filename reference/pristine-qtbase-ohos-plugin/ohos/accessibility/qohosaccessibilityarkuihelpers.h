// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSACCESSIBILITYARKUIHELPERS_H
#define QOHOSACCESSIBILITYARKUIHELPERS_H

#ifndef QT_NO_ACCESSIBILITY

#include <accessibility/qohosaccessibilitytree.h>
#include <arkui/native_interface_accessibility.h>
#include <render/qxcomponent.h>

namespace QOhos {

using OhosElementId = QtOhos::TypedId<std::int32_t, struct OhosElementIdTag>;

::ArkUI_AccessibilityProvider *getNativeAccessibilityProviderFromXComponentOrFail(
    QXComponentRender xComponent);

QOhosOptional<AccessibilityNode::Id> tryMapOhosElementIdToNodeId(
    AccessibilityNode::Id accessibilityRootId, std::int64_t ohosElementId);

QOhosOptional<AccessibilityNode::Id> tryMapNumericIdToValidNodeId(
    AccessibilityNode::Id::ValueType value);

OhosElementId mapNodeIdToOhosElementId(AccessibilityNode::Id id);

void fillAccessibilityElementInfoWithNodeState(
    AccessibilityNode::Id accessibilityRootId,
    const AccessibilityTree &accessibilityTree,
    ::ArkUI_AccessibilityElementInfo *elementInfo, const AccessibilityNode &node);

}

#endif // QT_NO_ACCESSIBILITY

#endif // QOHOSACCESSIBILITYARKUIHELPERS_H
