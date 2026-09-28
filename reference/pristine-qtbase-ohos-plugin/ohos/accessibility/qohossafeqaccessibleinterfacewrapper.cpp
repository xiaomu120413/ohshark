// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/private/qohoslogger_p.h>
#include <QtCore/qglobal.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qaccessible.h>
#include <QtGui/qcolor.h>
#include <QtGui/qwindow.h>
#include <accessibility/qohossafeqaccessibleinterfacewrapper.h>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

class SafeQAccessibleInterfaceWrapper : public QAccessibleInterface
{
public:
    static void destroyInstance(SafeQAccessibleInterfaceWrapper *instance);

    SafeQAccessibleInterfaceWrapper(
        std::shared_ptr<QAccessibleInterface> baseWrapper, QAccessible::Id interfaceId);

    bool isValid() const override;
    QObject *object() const override;
    QWindow *window() const override;

    QVector<QPair<QAccessibleInterface *, QAccessible::Relation>> relations(
        QAccessible::Relation match = QAccessible::AllRelations) const override;
    QAccessibleInterface *focusChild() const override;
    QAccessibleInterface *childAt(int x, int y) const override;

    QAccessibleInterface *parent() const override;
    QAccessibleInterface *child(int index) const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface *child) const override;

    QString text(QAccessible::Text t) const override;
    void setText(QAccessible::Text t, const QString &text) override;
    QRect rect() const override;
    QAccessible::Role role() const override;
    QAccessible::State state() const override;

    QColor foregroundColor() const override;
    QColor backgroundColor() const override;

    void virtual_hook(int id, void *data) override;
    void *interface_cast(QAccessible::InterfaceType interfaceType) override;

private:
    QAccessibleInterface *validInterfaceOrNull(const char *callerFuncName) const;

    QOhosOptional<std::pair<QAccessible::Id, QAccessibleInterface *>> m_idInterfacePair;
    std::shared_ptr<QAccessibleInterface> m_baseWrapper;
};

void SafeQAccessibleInterfaceWrapper::destroyInstance(SafeQAccessibleInterfaceWrapper *instance)
{
    delete instance;
}

SafeQAccessibleInterfaceWrapper::SafeQAccessibleInterfaceWrapper(
    std::shared_ptr<QAccessibleInterface> baseWrapper, QAccessible::Id interfaceId)
{
    auto *interface = QAccessible::accessibleInterface(interfaceId);
    if (interface == nullptr) {
        return;
    }

    m_idInterfacePair = std::make_pair(interfaceId, interface);
    m_baseWrapper = std::move(baseWrapper);
}

QAccessibleInterface *SafeQAccessibleInterfaceWrapper::validInterfaceOrNull(
    const char *callerFuncName) const
{
    if (m_idInterfacePair.hasValue()) {
        auto *interface = QAccessible::accessibleInterface(m_idInterfacePair.value().first);
        if (interface == m_idInterfacePair.value().second) {
            return interface;
        }
    }

    qOhosPrintfError(
        "%s: the underlying QAccessibleInterface has expired. Returning default value / doing nothing.",
        callerFuncName);

    return nullptr;
}

bool SafeQAccessibleInterfaceWrapper::isValid() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return false;
    }

    return m_baseWrapper->isValid();
}

QObject *SafeQAccessibleInterfaceWrapper::object() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->object();
}

QWindow *SafeQAccessibleInterfaceWrapper::window() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->window();
}

QVector<QPair<QAccessibleInterface *, QAccessible::Relation>>
SafeQAccessibleInterfaceWrapper::relations(QAccessible::Relation match) const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->relations(match);
}

QAccessibleInterface *SafeQAccessibleInterfaceWrapper::focusChild() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->focusChild();
}

QAccessibleInterface *SafeQAccessibleInterfaceWrapper::childAt(int x, int y) const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->childAt(x, y);
}

QAccessibleInterface *SafeQAccessibleInterfaceWrapper::parent() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->parent();
}

QAccessibleInterface *SafeQAccessibleInterfaceWrapper::child(
    int index) const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->child(index);
}

int SafeQAccessibleInterfaceWrapper::childCount() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->childCount();
}

int SafeQAccessibleInterfaceWrapper::indexOfChild(const QAccessibleInterface *child) const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return -1;
    }

    return m_baseWrapper->indexOfChild(child);
}

QString SafeQAccessibleInterfaceWrapper::text(QAccessible::Text t) const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->text(t);
}

void SafeQAccessibleInterfaceWrapper::setText(QAccessible::Text t, const QString &text)
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return;
    }

    m_baseWrapper->setText(t, text);
}

QRect SafeQAccessibleInterfaceWrapper::rect() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->rect();
}

QAccessible::Role SafeQAccessibleInterfaceWrapper::role() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->role();
}

QAccessible::State SafeQAccessibleInterfaceWrapper::state() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->state();
}

QColor SafeQAccessibleInterfaceWrapper::foregroundColor() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->foregroundColor();
}

QColor SafeQAccessibleInterfaceWrapper::backgroundColor() const
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return {};
    }

    return m_baseWrapper->backgroundColor();
}

void SafeQAccessibleInterfaceWrapper::virtual_hook(int id, void *data)
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return;
    }

    m_baseWrapper->virtual_hook(id, data);
}

void *SafeQAccessibleInterfaceWrapper::interface_cast(QAccessible::InterfaceType interfaceType)
{
    auto *interface = validInterfaceOrNull(Q_FUNC_INFO);
    if (interface == nullptr) {
        return nullptr;
    }

    return m_baseWrapper->interface_cast(interfaceType);
}

}

std::shared_ptr<QAccessibleInterface> makeSafeQAccessibleInterfaceWrapper(
    std::shared_ptr<QAccessibleInterface> baseInterfaceWrapper,
    QAccessible::Id wrappedInterfaceId)
{
    return std::shared_ptr<SafeQAccessibleInterfaceWrapper>(
        new SafeQAccessibleInterfaceWrapper(std::move(baseInterfaceWrapper), wrappedInterfaceId),
        &SafeQAccessibleInterfaceWrapper::destroyInstance);
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
