// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosaccessibilityeventhandler.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qaccessible.h>
#include <QtGui/qevent.h>
#include <accessibility/qohosaccessibilityevents.h>
#include <functional>
#include <qohosplatformintegration.h>
#include <qohosplugincore.h>
#include <set>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

void visitWidgetDescendantsWithPreorderTraversal(
    QObject *qObject, const std::function<void(QObject *)> &visitFunc)
{
    for (auto *child : qObject->children()) {
        const auto *childSuperClass = child->metaObject()->superClass();
        if (childSuperClass != nullptr && qstrcmp(childSuperClass->className(), "QLayout") == 0) {
            visitWidgetDescendantsWithPreorderTraversal(child, visitFunc);
        } else if (child->isWidgetType()) {
            visitFunc(child);
            visitWidgetDescendantsWithPreorderTraversal(child, visitFunc);
        }
    }
}

class AccessibilityEventFilter : public QObject
{
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    std::set<QObject *> m_fullyCreatedWidgets;

    void updateAccessibility(QAccessibleEvent *accessibleEvent);
};


void AccessibilityEventFilter::updateAccessibility(QAccessibleEvent *accessibleEvent)
{
    auto *accessibility = QGuiApplicationPrivate::platformIntegration()->accessibility();
    if (accessibility == nullptr) {
        qOhosPrintfError("%s: QPlatformAccessibility is null.", Q_FUNC_INFO);
        return;
    }

    if (m_fullyCreatedWidgets.find(accessibleEvent->object()) == m_fullyCreatedWidgets.end()) {
        qOhosPrintfDebug(
            "%s: attempting to send QAccessible event %d for a non-fully created widget %p. Ignoring.",
            Q_FUNC_INFO, accessibleEvent->type(), accessibleEvent->object());
        return;
    }

    accessibility->notifyAccessibilityUpdate(accessibleEvent);
}

bool AccessibilityEventFilter::eventFilter(QObject *qObject, QEvent *event)
{
    if (!qObject->isWidgetType()) {
        return QObject::eventFilter(qObject, event);
    }

    switch (event->type()) {
    case QEvent::ActionRemoved:
    case QEvent::ActionAdded: {
        QOhosAccessiblePlatformEvent accessibleEvent(
            qObject, OhosAccessiblePlatformEventType::ChildrenSync);
        updateAccessibility(&accessibleEvent);
        break;
    }
    case QEvent::ActionChanged: {
        QOhosAccessiblePlatformEvent accessibleEvent(
            qObject, OhosAccessiblePlatformEventType::ChildrenUpdate);
        updateAccessibility(&accessibleEvent);
        break;
    }
    case QEvent::Create: {
        m_fullyCreatedWidgets.insert(qObject);
        QAccessibleEvent event(qObject, QAccessible::ObjectCreated);
        updateAccessibility(&event);
        break;
    }
    case QEvent::Destroy:
        m_fullyCreatedWidgets.erase(qObject);
        break;
    case QEvent::ParentChange: {
        QAccessibleEvent event(qObject, QAccessible::ParentChanged);
        updateAccessibility(&event);
        break;
    }
    case QEvent::LayoutRequest:
        visitWidgetDescendantsWithPreorderTraversal(
            qObject,
            [this](QObject *descendant) {
                QAccessibleEvent event(descendant, QAccessible::LocationChanged);
                updateAccessibility(&event);
            });
        break;
    case QEvent::Resize:
    case QEvent::Move: {
        QAccessibleEvent event(qObject, QAccessible::LocationChanged);
        updateAccessibility(&event);
        break;
    }
    case QEvent::Paint:
        if (qstrcmp(qObject->metaObject()->className(), "QTabBar") == 0) {
            QOhosAccessiblePlatformEvent syncTabBarChildrenEvent(
                qObject, OhosAccessiblePlatformEventType::ChildrenSync);
            updateAccessibility(&syncTabBarChildrenEvent);

            QOhosAccessiblePlatformEvent updateTabBarChildrenEvent(
                qObject, OhosAccessiblePlatformEventType::ChildrenUpdate);
            updateAccessibility(&updateTabBarChildrenEvent);
        }
        break; 
    default:
        break;
    }

    return QObject::eventFilter(qObject, event);
}

}

std::unique_ptr<QObject> makeAccessibilityEventHandler()
{
    return std::make_unique<AccessibilityEventFilter>();
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
