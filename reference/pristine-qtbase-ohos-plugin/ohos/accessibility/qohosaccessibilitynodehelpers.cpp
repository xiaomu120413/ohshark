// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/qwindow.h>
#include <accessibility/qohosaccessibilitynodehelpers.h>
#include <string>
#include <utility>

#ifndef QT_NO_ACCESSIBILITY

namespace QOhos {

namespace {

std::string getValueTextOrFallbackText(const QAccessibleInterface &interface)
{
    auto interfaceValueText = interface.text(QAccessible::Value).toStdString();

    return !interfaceValueText.empty()
        ? interfaceValueText
        : interface.text(QAccessible::Name).toStdString();
}

QWindow *getAncestorWindowOrNull(const QAccessibleInterface &interface)
{
    auto *parent = interface.parent();
    while (parent != nullptr && parent->window() == nullptr) {
        parent = parent->parent();
    }

    return parent != nullptr ? parent->window() : nullptr;
}

std::string getAssociatedQObjectClassNameOrEmptyString(const QAccessibleInterface &interface)
{
    return interface.object() != nullptr ? interface.object()->metaObject()->className() : "";
}

bool getOverriddenFocusabilityOrDefaultForInterface(const QAccessibleInterface &interface)
{
    switch (interface.role()) {
    case QAccessible::MenuItem:
    case QAccessible::ProgressBar:
    case QAccessible::ScrollBar:
    case QAccessible::Slider:
    case QAccessible::StaticText:
    case QAccessible::ColumnHeader:
    case QAccessible::RowHeader:
        return true;
    default:
        break;
    }

    return static_cast<bool>(interface.state().focusable);
}

}

std::string getNameTextOrFallbackText(const QAccessibleInterface &interface)
{
    auto interfaceNameText = interface.text(QAccessible::Name).toStdString();

    return !interfaceNameText.empty()
        ? interfaceNameText
        : getAssociatedQObjectClassNameOrEmptyString(interface);
}

QOhos::AccessibilityNode::ValueInfo makeValueInfo(QAccessibleInterface *interface)
{
    auto *optValueInterface = interface->valueInterface();
    if (optValueInterface != nullptr) {
        auto currentValue = optValueInterface->currentValue();
        auto minimumValue = optValueInterface->minimumValue();
        auto maximumValue = optValueInterface->maximumValue();

        bool canConvertToString = currentValue.canConvert<QString>()
            && minimumValue.canConvert<QString>() && maximumValue.canConvert<QString>();

        if (canConvertToString) {
            return QOhos::AccessibilityNode::ValueInfo {
                .minMaxRange = makeQOhosOptional(
                    std::make_pair(
                        minimumValue.toString().toStdString(),
                        maximumValue.toString().toStdString())),
                .current = currentValue.toString().toStdString(),
            };
        } else {
            qOhosPrintfWarning("%s: cannot obtain value limits from the interface.", Q_FUNC_INFO);
        }
    }

    return QOhos::AccessibilityNode::ValueInfo {
        .current = getValueTextOrFallbackText(*interface),
    };
}

QRect getRelativeGeometryOrNull(QAccessibleInterface *interface)
{
    if (interface->parent() == nullptr) {
        return interface->rect();
    }

    auto *qWindow = interface->window() != nullptr
        ? interface->window()
        : getAncestorWindowOrNull(*interface);

    if (qWindow == nullptr) {
        return QRect {};
    }

    auto windowSpaceGeometry = QRect(
        qWindow->mapFromGlobal(interface->rect().topLeft()),
        interface->rect().size());

    return QHighDpi::toNativePixels(windowSpaceGeometry, qWindow);
}

QOhos::AccessibilityNode makeAccessibilityNode(QAccessibleInterface *interface)
{
    const auto state = interface->state();

    QOhos::AccessibilityNode node {
        .id = QOhos::AccessibilityNode::Id(QAccessibleWrapper::uniqueId(interface)),
        .geometry = getRelativeGeometryOrNull(interface),
        .componentType = getAssociatedQObjectClassNameOrEmptyString(*interface),
        .name = getNameTextOrFallbackText(*interface),
        .help = interface->text(QAccessible::Help).toStdString(),
        .description = interface->text(QAccessible::Description).toStdString(),
        .valueInfo = makeValueInfo(interface),
        .enabled = !static_cast<bool>(state.disabled),
        .visible = !static_cast<bool>(state.invisible),
        .checked = static_cast<bool>(state.checked),
        .focused = static_cast<bool>(state.focused),
        .selected = static_cast<bool>(state.selected),
        .passwordEdit = static_cast<bool>(state.passwordEdit),
        .editable = static_cast<bool>(state.editable),
        .clickable = static_cast<bool>(state.checkable),
        .checkable = static_cast<bool>(state.checkable),
        .focusable = getOverriddenFocusabilityOrDefaultForInterface(*interface),
        .backgroundColor = interface->backgroundColor().name(QColor::HexArgb).toStdString(),
    };

    auto *parent = interface->parent();
    if (parent != nullptr) {
        node.parentId =
            QOhos::AccessibilityNode::Id(QAccessibleWrapper::uniqueId(parent));
    }

    auto *actionInterface = interface->actionInterface();
    if (actionInterface != nullptr) {
        const auto actionNames = actionInterface->actionNames();
        node.clickable = node.clickable
            || actionNames.contains(QAccessibleActionInterface::pressAction())
            || actionNames.contains(QAccessibleActionInterface::showMenuAction())
            || actionNames.contains(QAccessibleActionInterface::toggleAction());
        node.scrollable = actionNames.contains(QAccessibleActionInterface::scrollLeftAction())
            || actionNames.contains(QAccessibleActionInterface::scrollRightAction())
            || actionNames.contains(QAccessibleActionInterface::scrollUpAction())
            || actionNames.contains(QAccessibleActionInterface::scrollDownAction());
    }

    return node;
}

bool isTableInterfaceRole(QAccessible::Role role)
{
    return role == QAccessible::Role::List
        || role == QAccessible::Role::Table
        || role == QAccessible::Role::Tree;
}

}

#endif // QT_NO_ACCESSIBILITY
