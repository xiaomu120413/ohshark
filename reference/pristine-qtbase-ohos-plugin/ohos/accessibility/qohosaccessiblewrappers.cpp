// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtCore/qcoreapplication.h>
#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/private/qohoslogger_p.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>
#include <accessibility/qohosaccessiblewrappers.h>
#include <accessibility/qohossafeqaccessibleinterfacewrapper.h>
#include <map>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

constexpr int qAccessibleComboBoxListViewChildIndex = 0;

bool classNameRepresentsAccessibleTableWidget(const std::string &className)
{
    return className == "QListView" || className == "QTableView"
        || className == "QListWidget" || className == "QTableWidget"
        || className == "QTreeWidget" || className == "QTreeView";
}

class QAccessibleTableCellInterfaceWrapperImpl;

class QAccessibleInterfaceWrapperImpl : public QAccessibleInterface
{
public:
    static void destroyInstance(QAccessibleInterfaceWrapperImpl *instance);

    QAccessibleInterfaceWrapperImpl(QAccessibleInterface *wrappedInterface);

protected:
    QAccessibleInterfaceWrapperImpl(
        QAccessibleInterface *wrappedInterface,
        std::shared_ptr<QAccessibleTableInterface> optTableInterfaceWrapper,
        std::shared_ptr<QAccessibleTableCellInterface> optTableCellInterfaceWrapper,
        std::shared_ptr<QAccessibleActionInterface> optActionInterfaceWrapper);

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

protected:
    QAccessibleInterface *wrappedInterface() const;

private:
    QAccessibleInterface *m_wrappedInterface;
    std::shared_ptr<QAccessibleTableInterface> m_optTableInterfaceWrapper;
    std::shared_ptr<QAccessibleTableCellInterface> m_optTableCellInterfaceWrapper;
    std::shared_ptr<QAccessibleActionInterface> m_optActionInterfaceWrapper;
};

template<typename InterfaceType>
class SpecializedInterfaceHolder
{
public:
    SpecializedInterfaceHolder(InterfaceType *interface);

    InterfaceType *interface() const;

private:
    InterfaceType *m_interface;
};

class QAccessibleComboBoxPrivateContainerInterfaceWrapper : public QAccessibleInterfaceWrapperImpl
{
public:
    QAccessibleComboBoxPrivateContainerInterfaceWrapper(QAccessibleInterface *wrappedInterface);

protected:
    QObject *object() const override;

    QAccessibleInterface *child(int index) const override;

    int childCount() const override;

    QString text(QAccessible::Text t) const override;
    QRect rect() const override;
    QAccessible::Role role() const override;
    QAccessible::State state() const override;

    void *interface_cast(QAccessible::InterfaceType interfaceType) override;

private:
    QAccessibleInterface *getComboBoxListViewInterface(QAccessibleInterface *validInterface) const;
};

class QAccessibleComboBoxInterfaceWrapper : public QAccessibleInterfaceWrapperImpl
{
public:
    QAccessibleComboBoxInterfaceWrapper(QAccessibleInterface *wrappedInterface);

protected:
    QAccessibleInterface *child(int index) const override;
};

class QAccessibleComboBoxListViewChildInterfaceWrapper : public QAccessibleInterfaceWrapperImpl
{
public:
    QAccessibleComboBoxListViewChildInterfaceWrapper(QAccessibleInterface *wrappedInterface);

    QAccessibleInterface *parent() const override;
};

class QAccessibleTableWrapper : public QAccessibleInterfaceWrapperImpl
{
public:
    QAccessibleTableWrapper(QAccessibleInterface *wrappedInterface);

    QAccessibleInterface *parent() const override;
    QWindow *window() const override;
};

class QAccessibleQMenuInterfaceWrapper : public QAccessibleInterfaceWrapperImpl
{
public:
    QAccessibleQMenuInterfaceWrapper(QAccessibleInterface *wrappedInterface);

    QAccessibleInterface *parent() const override;
};

class QAccessibleTabButtonInterfaceWrapper : public QAccessibleInterfaceWrapperImpl
{
public:
    QAccessibleTabButtonInterfaceWrapper(QAccessibleInterface *wrappedInterface);

private:
    static std::shared_ptr<QAccessibleActionInterface> makeTabButtonActionInterfaceWrapper(
        QAccessibleInterface *tabButtonInterface);
};

class QAccessibleTableInterfaceWrapperImpl
    : public SpecializedInterfaceHolder<QAccessibleTableInterface>,
      public QAccessibleTableInterface
{
public:
    QAccessibleTableInterfaceWrapperImpl(QAccessibleTableInterface *interface);

    QAccessibleInterface *caption() const override;
    QAccessibleInterface *summary() const override;
    QAccessibleInterface *cellAt(int row, int column) const override;

    int selectedCellCount() const override;
    QList<QAccessibleInterface *> selectedCells() const override;
    QString columnDescription(int column) const override;
    QString rowDescription(int row) const override;
    int selectedColumnCount() const override;
    int selectedRowCount() const override;
    int columnCount() const override;
    int rowCount() const override;
    QList<int> selectedColumns() const override;
    QList<int> selectedRows() const override;
    bool isColumnSelected(int column) const override;
    bool isRowSelected(int row) const override;
    bool selectRow(int row) override;
    bool selectColumn(int column) override;
    bool unselectRow(int row) override;
    bool unselectColumn(int column) override;
    void modelChange(QAccessibleTableModelChangeEvent *event) override;
};

class QAccessibleTableCellInterfaceWrapperImpl
    : public SpecializedInterfaceHolder<QAccessibleTableCellInterface>
    , public QAccessibleTableCellInterface
{
public:
    QAccessibleTableCellInterfaceWrapperImpl(QAccessibleTableCellInterface *interface);

    bool isSelected() const override;
    QList<QAccessibleInterface *> columnHeaderCells() const override;
    QList<QAccessibleInterface *> rowHeaderCells() const override;
    int columnIndex() const override;
    int rowIndex() const override;
    int columnExtent() const override;
    int rowExtent() const override;
    QAccessibleInterface *table() const override;
};

class QAccessibleActionInterfaceWrapperImpl
    : public SpecializedInterfaceHolder<QAccessibleActionInterface>
    , public QAccessibleActionInterface
{
public:
    QAccessibleActionInterfaceWrapperImpl(QAccessibleActionInterface *interface);

    QStringList actionNames() const override;
    QString localizedActionName(const QString &name) const override;
    QString localizedActionDescription(const QString &name) const override;
    void doAction(const QString &actionName) override;
    QStringList keyBindingsForAction(const QString &actionName) const override;
};

class QAccessibleTabButtonActionInterfaceWrapperImpl : public QAccessibleActionInterfaceWrapperImpl
{
public:
    QAccessibleTabButtonActionInterfaceWrapperImpl(
        QAccessibleInterface *tabBarInterface, QAccessibleActionInterface *actionInterface);

    QStringList actionNames() const override;
    void doAction(const QString &actionName) override;

private:
    QAccessibleInterface *m_tabBarInterface;
};

struct InterfaceWrapperCreateInfo
{
    std::shared_ptr<QAccessibleInterface> wrapper;
    QAccessible::Id interfaceId;
};

class AccessibleInterfacesWrappersSession
{
public:
    QAccessibleInterface *getOrCreateWrapperFromRawInterfaceOrFail(QAccessibleInterface *interface);
    QAccessibleInterface *getRawInterfaceFromWrapperOrFail(const QAccessibleInterface *interface);

private:
    std::map<QAccessibleInterface *, std::shared_ptr<QAccessibleInterface>> m_rawInterfacesToWrappersMap;
};

QOhosOptional<AccessibleInterfacesWrappersSession> &getAccessibleInterfacesWrappersSessionRef()
{
    static QOhosOptional<AccessibleInterfacesWrappersSession> accessibleInterfacesWrappersSession;
    return accessibleInterfacesWrappersSession;
}

template<typename T>
InterfaceWrapperCreateInfo makeInterfaceWrapperWithCreateInfo(QAccessibleInterface *interface)
{
    auto interfaceId = QAccessible::uniqueId(interface);

    return {
        .wrapper =
            std::shared_ptr<T>(new T(interface), &QAccessibleInterfaceWrapperImpl::destroyInstance),
        .interfaceId = interfaceId,
    };
}

QOhosOptional<InterfaceWrapperCreateInfo> tryMakeQComboBoxListViewChildWrapper(
    QAccessibleInterface *baseInterface)
{
    auto *parentInterfaceQObject =
        baseInterface->parent() ? baseInterface->parent()->object() : nullptr;

    bool representsQComboBoxListViewChild = parentInterfaceQObject != nullptr
        && qstrcmp(parentInterfaceQObject->metaObject()->className(), "QComboBoxListView") == 0;

    return representsQComboBoxListViewChild
        ? makeQOhosOptional(
            makeInterfaceWrapperWithCreateInfo<QAccessibleComboBoxListViewChildInterfaceWrapper>(
                baseInterface))
        : makeEmptyQOhosOptional();
}

InterfaceWrapperCreateInfo makeSpecializedWrapperWithCreateInfoForNonQObjectInterfaceOrDefault(
    QAccessibleInterface *baseInterface)
{
    auto maybeQComboBoxListViewChildWrapper = tryMakeQComboBoxListViewChildWrapper(baseInterface);
    if (maybeQComboBoxListViewChildWrapper.hasValue()) {
        return maybeQComboBoxListViewChildWrapper.value();
    }

    if (baseInterface->role() == QAccessible::Role::PageTab) {
        return makeInterfaceWrapperWithCreateInfo<QAccessibleTabButtonInterfaceWrapper>(baseInterface);
    }

    return makeInterfaceWrapperWithCreateInfo<QAccessibleInterfaceWrapperImpl>(baseInterface);
}

InterfaceWrapperCreateInfo makeSpecializedWrapperWithCreateInfoFromQObjectOrDefault(
    QObject *qObject)
{
    std::string className = qObject->metaObject()->className();
    if (className == "QComboBoxPrivateContainer") {
        return makeInterfaceWrapperWithCreateInfo<QAccessibleComboBoxPrivateContainerInterfaceWrapper>(
            QAccessible::queryAccessibleInterface(qObject));
    } else if (className == "QComboBox") {
        return makeInterfaceWrapperWithCreateInfo<QAccessibleComboBoxInterfaceWrapper>(
            QAccessible::queryAccessibleInterface(qObject));
    } else if (classNameRepresentsAccessibleTableWidget(className)) {
        return makeInterfaceWrapperWithCreateInfo<QAccessibleTableWrapper>(
            QAccessible::queryAccessibleInterface(qObject));
    } else if (className == "QMenu") {
        return makeInterfaceWrapperWithCreateInfo<QAccessibleQMenuInterfaceWrapper>(
            QAccessible::queryAccessibleInterface(qObject));
    }

    return makeInterfaceWrapperWithCreateInfo<QAccessibleInterfaceWrapperImpl>(
        QAccessible::queryAccessibleInterface(qObject));
}

std::shared_ptr<QAccessibleInterface> makeFromRawInterface(
    QAccessibleInterface *interface)
{
    if (interface == nullptr) {
        return nullptr;
    }

    auto interfaceWrapperCreateInfo = interface->object() != nullptr
        ? makeSpecializedWrapperWithCreateInfoFromQObjectOrDefault(interface->object())
        : makeSpecializedWrapperWithCreateInfoForNonQObjectInterfaceOrDefault(interface);

    return makeSafeQAccessibleInterfaceWrapper(
        std::move(interfaceWrapperCreateInfo.wrapper), interfaceWrapperCreateInfo.interfaceId);
}

QAccessibleInterface *getWrappedInterfaceOrAbort(QAccessibleInterface *interface)
{
    if (!getAccessibleInterfacesWrappersSessionRef().hasValue()) {
        qOhosReportFatalErrorAndAbort("%s: no active wrappers session.", Q_FUNC_INFO);
    }

    return getAccessibleInterfacesWrappersSessionRef().value().getOrCreateWrapperFromRawInterfaceOrFail(interface);
}

QAccessibleInterface *getRawInterfaceOrFail(const QAccessibleInterface *interfaceWrapper)
{
    if (!getAccessibleInterfacesWrappersSessionRef().hasValue()) {
        qOhosReportFatalErrorAndAbort("%s: no active wrappers session.", Q_FUNC_INFO);
    }

    return getAccessibleInterfacesWrappersSessionRef().value().getRawInterfaceFromWrapperOrFail(
        interfaceWrapper);
}

std::shared_ptr<QAccessibleTableInterface> tryMakeTableInterfaceWrapperFromRawInterface(
    QAccessibleInterface *interface)
{
    auto *baseInterface = reinterpret_cast<QAccessibleTableInterface *>(
        interface->interface_cast(QAccessible::TableInterface));

    return baseInterface != nullptr
        ? std::make_shared<QAccessibleTableInterfaceWrapperImpl>(baseInterface)
        : nullptr;
}

std::shared_ptr<QAccessibleTableCellInterface> tryMakeTableCellInterfaceWrapperFromRawInterface(
    QAccessibleInterface *interface)
{
    auto *baseInterface = reinterpret_cast<QAccessibleTableCellInterface *>(
        interface->interface_cast(QAccessible::TableCellInterface));

    return baseInterface != nullptr
        ? std::make_shared<QAccessibleTableCellInterfaceWrapperImpl>(baseInterface)
        : nullptr;
}

std::shared_ptr<QAccessibleActionInterface> tryMakeActionInterfaceWrapperFromRawInterface(
    QAccessibleInterface *interface)
{
    auto *baseInterface = reinterpret_cast<QAccessibleActionInterface *>(
        interface->interface_cast(QAccessible::ActionInterface));

    return baseInterface != nullptr
        ? std::make_shared<QAccessibleActionInterfaceWrapperImpl>(baseInterface)
        : nullptr;
}

QAccessibleInterface *AccessibleInterfacesWrappersSession::getOrCreateWrapperFromRawInterfaceOrFail(
    QAccessibleInterface *interface)
{
    if (interface == nullptr) {
        qOhosReportFatalErrorAndAbort(
            "%s: given raw interface is a nullptr. This shouldn't have happened.",
            Q_FUNC_INFO);
    }

    if (m_rawInterfacesToWrappersMap.find(interface) == m_rawInterfacesToWrappersMap.end()) {
        m_rawInterfacesToWrappersMap[interface] = makeFromRawInterface(interface);
    }

    return m_rawInterfacesToWrappersMap[interface].get();
}

QAccessibleInterface *AccessibleInterfacesWrappersSession::getRawInterfaceFromWrapperOrFail(
    const QAccessibleInterface *interfaceWrapper)
{
    if (interfaceWrapper == nullptr) {
        qOhosReportFatalErrorAndAbort(
            "%s: given wrapper is a nullptr. This shouldn't have happened.",
            Q_FUNC_INFO);
    }

    auto foundRawInterfaceWithWrapperIt = std::find_if(
        m_rawInterfacesToWrappersMap.begin(), m_rawInterfacesToWrappersMap.end(),
        [&](const auto &rawInterfaceWithWrapperPair) {
            return rawInterfaceWithWrapperPair.second.get() == interfaceWrapper;
        });

    if (foundRawInterfaceWithWrapperIt == m_rawInterfacesToWrappersMap.end()) {
        qOhosReportFatalErrorAndAbort(
            "%s: wrapper '%p' isn't a part of this session. This shouldn't have happened.",
            Q_FUNC_INFO, interfaceWrapper);
    }

    return foundRawInterfaceWithWrapperIt->first;
}

void QAccessibleInterfaceWrapperImpl::destroyInstance(QAccessibleInterfaceWrapperImpl *instance)
{
    delete instance;
}

QAccessibleInterfaceWrapperImpl::QAccessibleInterfaceWrapperImpl(QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(
        wrappedInterface,
        tryMakeTableInterfaceWrapperFromRawInterface(wrappedInterface),
        tryMakeTableCellInterfaceWrapperFromRawInterface(wrappedInterface),
        tryMakeActionInterfaceWrapperFromRawInterface(wrappedInterface))
{
}

QAccessibleInterfaceWrapperImpl::QAccessibleInterfaceWrapperImpl(
    QAccessibleInterface *wrappedInterface,
    std::shared_ptr<QAccessibleTableInterface> optTableInterfaceWrapper,
    std::shared_ptr<QAccessibleTableCellInterface> optTableCellInterfaceWrapper,
    std::shared_ptr<QAccessibleActionInterface> optActionInterfaceWrapper)
    : m_wrappedInterface(wrappedInterface)
    , m_optTableInterfaceWrapper(std::move(optTableInterfaceWrapper))
    , m_optTableCellInterfaceWrapper(std::move(optTableCellInterfaceWrapper))
    , m_optActionInterfaceWrapper(std::move(optActionInterfaceWrapper))
{
}

bool QAccessibleInterfaceWrapperImpl::isValid() const
{
    return wrappedInterface()->isValid();
}

QObject *QAccessibleInterfaceWrapperImpl::object() const
{
    return wrappedInterface()->object();
}

QWindow *QAccessibleInterfaceWrapperImpl::window() const
{
    return wrappedInterface()->window();
}

QVector<QPair<QAccessibleInterface *, QAccessible::Relation>>
QAccessibleInterfaceWrapperImpl::relations(QAccessible::Relation match) const
{
    QVector<QPair<QAccessibleInterface *, QAccessible::Relation>> result;
    for (auto p : wrappedInterface()->relations(match)) {
        result.push_back(
            {
                getWrappedInterfaceOrAbort(p.first),
                p.second,
            });
    }
    return result;
}

QAccessibleInterface *QAccessibleInterfaceWrapperImpl::focusChild() const
{
    auto *focusChildInterface = wrappedInterface()->focusChild();

    return focusChildInterface != nullptr
        ? getWrappedInterfaceOrAbort(focusChildInterface)
        : nullptr;
}

QAccessibleInterface *QAccessibleInterfaceWrapperImpl::childAt(int x, int y) const
{
    auto *childInterface = wrappedInterface()->childAt(x, y);

    return childInterface != nullptr
        ? getWrappedInterfaceOrAbort(childInterface)
        : nullptr;
}

QAccessibleInterface *QAccessibleInterfaceWrapperImpl::parent() const
{
    auto *parentInterface = wrappedInterface()->parent();

    return parentInterface != nullptr
        ? getWrappedInterfaceOrAbort(parentInterface)
        : nullptr;
}

QAccessibleInterface *QAccessibleInterfaceWrapperImpl::child(int index) const
{
    auto *childInterface = wrappedInterface()->child(index);

    return childInterface != nullptr
        ? getWrappedInterfaceOrAbort(childInterface)
        : nullptr;
}

int QAccessibleInterfaceWrapperImpl::childCount() const
{
    return wrappedInterface()->childCount();
}

int QAccessibleInterfaceWrapperImpl::indexOfChild(const QAccessibleInterface *child) const
{
    return wrappedInterface()->indexOfChild(getRawInterfaceOrFail(child));
}

QString QAccessibleInterfaceWrapperImpl::text(QAccessible::Text t) const
{
    return wrappedInterface()->text(t);
}

void QAccessibleInterfaceWrapperImpl::setText(QAccessible::Text t, const QString &text)
{
    wrappedInterface()->setText(t, text);
}

QRect QAccessibleInterfaceWrapperImpl::rect() const
{
    return wrappedInterface()->rect();
}

QAccessible::Role QAccessibleInterfaceWrapperImpl::role() const
{
    return wrappedInterface()->role();
}

QAccessible::State QAccessibleInterfaceWrapperImpl::state() const
{
    return wrappedInterface()->state();
}

QColor QAccessibleInterfaceWrapperImpl::foregroundColor() const
{
    return wrappedInterface()->foregroundColor();
}

QColor QAccessibleInterfaceWrapperImpl::backgroundColor() const
{
    return wrappedInterface()->backgroundColor();
}

void QAccessibleInterfaceWrapperImpl::virtual_hook(int id, void *data)
{
    wrappedInterface()->virtual_hook(id, data);
}

void *QAccessibleInterfaceWrapperImpl::interface_cast(QAccessible::InterfaceType interfaceType)
{
    switch (interfaceType) {
    case QAccessible::ActionInterface:
        return wrappedInterface()->interface_cast(QAccessible::ActionInterface) != nullptr
            ? m_optActionInterfaceWrapper.get()
            : nullptr;
    case QAccessible::TableInterface:
        return wrappedInterface()->interface_cast(QAccessible::TableInterface) != nullptr
            ? m_optTableInterfaceWrapper.get()
            : nullptr;
    case QAccessible::TableCellInterface:
        return wrappedInterface()->interface_cast(QAccessible::TableCellInterface) != nullptr
            ? m_optTableCellInterfaceWrapper.get()
            : nullptr;
    default:
        break;
    }

    return wrappedInterface()->interface_cast(interfaceType);
}

QAccessibleInterface *QAccessibleInterfaceWrapperImpl::wrappedInterface() const
{
    return m_wrappedInterface;
}

template<typename InterfaceType>
SpecializedInterfaceHolder<InterfaceType>::SpecializedInterfaceHolder(InterfaceType *interface)
    : m_interface(interface)
{
}

template<typename InterfaceType>
InterfaceType *SpecializedInterfaceHolder<InterfaceType>::interface() const
{
    return m_interface;
}

QAccessibleComboBoxPrivateContainerInterfaceWrapper::QAccessibleComboBoxPrivateContainerInterfaceWrapper(
    QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(
        wrappedInterface,
        tryMakeTableInterfaceWrapperFromRawInterface(
            getComboBoxListViewInterface(wrappedInterface)),
        tryMakeTableCellInterfaceWrapperFromRawInterface(
            getComboBoxListViewInterface(wrappedInterface)),
        tryMakeActionInterfaceWrapperFromRawInterface(
            getComboBoxListViewInterface(wrappedInterface)))
{
}

QObject *QAccessibleComboBoxPrivateContainerInterfaceWrapper::object() const
{
    return getComboBoxListViewInterface(wrappedInterface())->object();
}

QAccessibleInterface *QAccessibleComboBoxPrivateContainerInterfaceWrapper::child(
    int index) const
{
    return getWrappedInterfaceOrAbort(getComboBoxListViewInterface(wrappedInterface())->child(index));
}

int QAccessibleComboBoxPrivateContainerInterfaceWrapper::childCount() const
{
    return getComboBoxListViewInterface(wrappedInterface())->childCount();
}

QString QAccessibleComboBoxPrivateContainerInterfaceWrapper::text(QAccessible::Text t) const
{
    return getComboBoxListViewInterface(wrappedInterface())->text(t);
}

QRect QAccessibleComboBoxPrivateContainerInterfaceWrapper::rect() const
{
    return getComboBoxListViewInterface(wrappedInterface())->rect();
}

QAccessible::Role QAccessibleComboBoxPrivateContainerInterfaceWrapper::role() const
{
    return getComboBoxListViewInterface(wrappedInterface())->role();
}

QAccessible::State QAccessibleComboBoxPrivateContainerInterfaceWrapper::state() const
{
    return getComboBoxListViewInterface(wrappedInterface())->state();
}

void *QAccessibleComboBoxPrivateContainerInterfaceWrapper::interface_cast(
    QAccessible::InterfaceType interfaceType)
{
    auto *wrappedComboBoxListViewInterface =
        getWrappedInterfaceOrAbort(getComboBoxListViewInterface(wrappedInterface()));

    return wrappedComboBoxListViewInterface->interface_cast(interfaceType);
}

QAccessibleInterface *
QAccessibleComboBoxPrivateContainerInterfaceWrapper::getComboBoxListViewInterface(
    QAccessibleInterface *interface) const
{
    auto *comboBoxInterface =
        QAccessible::queryAccessibleInterface(interface->object()->parent());

    return comboBoxInterface->child(qAccessibleComboBoxListViewChildIndex);
}

QAccessibleComboBoxInterfaceWrapper::QAccessibleComboBoxInterfaceWrapper(
    QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(wrappedInterface)
{
}

QAccessibleInterface *QAccessibleComboBoxInterfaceWrapper::child(int index) const
{
    if (index == qAccessibleComboBoxListViewChildIndex) {
        // NOTE: the original list view child must be called upon first to ensure that
        // both the QComboBoxPrivateContainer and the view itself are lazily created.
        auto *originalListViewInterface = wrappedInterface()->child(qAccessibleComboBoxListViewChildIndex);
        return getWrappedInterfaceOrAbort(
            QAccessible::queryAccessibleInterface(originalListViewInterface->object()->parent()));
    }

    return QAccessibleInterfaceWrapperImpl::child(index);
}

QAccessibleComboBoxListViewChildInterfaceWrapper::QAccessibleComboBoxListViewChildInterfaceWrapper(
    QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(wrappedInterface)
{
}

QAccessibleInterface *QAccessibleComboBoxListViewChildInterfaceWrapper::parent() const
{
    auto *qComboBoxPrivateContainerQObject = wrappedInterface()->parent()->object()->parent();

    return getWrappedInterfaceOrAbort(
        QAccessible::queryAccessibleInterface(qComboBoxPrivateContainerQObject));
}

QAccessibleTableWrapper::QAccessibleTableWrapper(QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(wrappedInterface)
{
}

QAccessibleInterface *QAccessibleTableWrapper::parent() const
{
    auto *parentInterface = wrappedInterface()->parent();
    if (parentInterface != nullptr) {
        return QAccessibleInterfaceWrapperImpl::parent();
    }

    auto *qObjectParent = wrappedInterface()->object()->parent();
    return getWrappedInterfaceOrAbort(
        qObjectParent != nullptr
            ? QAccessible::queryAccessibleInterface(qObjectParent)
            : QAccessible::queryAccessibleInterface(qApp));
}

QWindow *QAccessibleTableWrapper::window() const
{
    if (wrappedInterface()->window() != nullptr) {
        return wrappedInterface()->window();
    }

    for (auto *qWindow : QGuiApplication::allWindows()) {
        if (qWindow->accessibleRoot() == wrappedInterface()) {
            return qWindow;
        }
    }

    return nullptr;
}

QAccessibleQMenuInterfaceWrapper::QAccessibleQMenuInterfaceWrapper(
    QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(wrappedInterface)
{
}

QAccessibleInterface *QAccessibleQMenuInterfaceWrapper::parent() const
{
    return getWrappedInterfaceOrAbort(QAccessible::queryAccessibleInterface(qApp));
}

QAccessibleTabButtonInterfaceWrapper::QAccessibleTabButtonInterfaceWrapper(
    QAccessibleInterface *wrappedInterface)
    : QAccessibleInterfaceWrapperImpl(
        wrappedInterface,
        tryMakeTableInterfaceWrapperFromRawInterface(wrappedInterface),
        tryMakeTableCellInterfaceWrapperFromRawInterface(wrappedInterface),
        makeTabButtonActionInterfaceWrapper(wrappedInterface))
{
}

std::shared_ptr<QAccessibleActionInterface>
QAccessibleTabButtonInterfaceWrapper::makeTabButtonActionInterfaceWrapper(
    QAccessibleInterface *tabButtonInterface)
{
    auto *actionInterface = tabButtonInterface->actionInterface();
    if (actionInterface == nullptr) {
        qOhosReportFatalErrorAndAbort(
            "%s: QAccessibleTabButton %p doesn't implement QAccessibleActionInterface. This shouldn't have happened.",
            Q_FUNC_INFO, tabButtonInterface);
    }

    if (!actionInterface->actionNames().contains(QAccessibleActionInterface::pressAction())) {
        qOhosReportFatalErrorAndAbort(
            "%s: QAccessibleTabButton %p doesn't contain press action. This shouldn't have happened.",
            Q_FUNC_INFO, tabButtonInterface);
    }

    auto *tabBarInterface = tabButtonInterface->parent();
    if (tabBarInterface == nullptr || tabBarInterface->role() != QAccessible::Role::PageTabList) {
        qOhosReportFatalErrorAndAbort(
            "%s: QAccessibleTabButton %p is not a child of QAccessibleTabBar. This shouldn't have happened.",
            Q_FUNC_INFO, tabButtonInterface);
    }

    return std::make_shared<QAccessibleTabButtonActionInterfaceWrapperImpl>(
        tabBarInterface, actionInterface);
}

QAccessibleTableInterfaceWrapperImpl::QAccessibleTableInterfaceWrapperImpl(
    QAccessibleTableInterface *interface)
    : SpecializedInterfaceHolder<QAccessibleTableInterface>(interface)
{
}

QAccessibleInterface *QAccessibleTableInterfaceWrapperImpl::caption() const
{
    auto *captionInterface = interface()->caption();

    return captionInterface != nullptr
        ? getWrappedInterfaceOrAbort(captionInterface)
        : nullptr;
}

QAccessibleInterface *QAccessibleTableInterfaceWrapperImpl::summary() const
{
    auto *summaryInterface = interface()->summary();

    return summaryInterface != nullptr
        ? getWrappedInterfaceOrAbort(summaryInterface)
        : nullptr;
}

QAccessibleInterface *QAccessibleTableInterfaceWrapperImpl::cellAt(
    int row, int column) const
{
    auto *cellInterface = interface()->cellAt(row, column);

    return cellInterface != nullptr
        ? getWrappedInterfaceOrAbort(cellInterface)
        : nullptr;
}

int QAccessibleTableInterfaceWrapperImpl::selectedCellCount() const
{
    return interface()->selectedCellCount();
}

QList<QAccessibleInterface *> QAccessibleTableInterfaceWrapperImpl::selectedCells() const
{
    QList<QAccessibleInterface *> result;
    for (auto *cellInterface : interface()->selectedCells()) {
        result.push_back(getWrappedInterfaceOrAbort(cellInterface));
    }

    return result;
}

QString QAccessibleTableInterfaceWrapperImpl::columnDescription(int column) const
{
    return interface()->columnDescription(column);
}

QString QAccessibleTableInterfaceWrapperImpl::rowDescription(int row) const
{
    return interface()->rowDescription(row);
}

int QAccessibleTableInterfaceWrapperImpl::selectedColumnCount() const
{
    return interface()->selectedColumnCount();
}

int QAccessibleTableInterfaceWrapperImpl::selectedRowCount() const
{
    return interface()->selectedRowCount();
}

int QAccessibleTableInterfaceWrapperImpl::columnCount() const
{
    return interface()->columnCount();
}

int QAccessibleTableInterfaceWrapperImpl::rowCount() const
{
    return interface()->rowCount();
}

QList<int> QAccessibleTableInterfaceWrapperImpl::selectedColumns() const
{
    return interface()->selectedColumns();
}

QList<int> QAccessibleTableInterfaceWrapperImpl::selectedRows() const
{
    return interface()->selectedRows();
}

bool QAccessibleTableInterfaceWrapperImpl::isColumnSelected(int column) const
{
    return interface()->isColumnSelected(column);
}

bool QAccessibleTableInterfaceWrapperImpl::isRowSelected(int row) const
{
    return interface()->isRowSelected(row);
}

bool QAccessibleTableInterfaceWrapperImpl::selectRow(int row)
{
    return interface()->selectRow(row);
}

bool QAccessibleTableInterfaceWrapperImpl::selectColumn(int column)
{
    return interface()->selectColumn(column);
}

bool QAccessibleTableInterfaceWrapperImpl::unselectRow(int row)
{
    return interface()->unselectRow(row);
}

bool QAccessibleTableInterfaceWrapperImpl::unselectColumn(int column)
{
    return interface()->unselectColumn(column);
}

void QAccessibleTableInterfaceWrapperImpl::modelChange(QAccessibleTableModelChangeEvent *event)
{
    interface()->modelChange(event);
}

QAccessibleTableCellInterfaceWrapperImpl::QAccessibleTableCellInterfaceWrapperImpl(
    QAccessibleTableCellInterface *interface)
    : SpecializedInterfaceHolder<QAccessibleTableCellInterface>(interface)
{
}

bool QAccessibleTableCellInterfaceWrapperImpl::isSelected() const
{
    return interface()->isSelected();
}

QList<QAccessibleInterface *> QAccessibleTableCellInterfaceWrapperImpl::columnHeaderCells() const
{
    QList<QAccessibleInterface *> result;
    for (auto *cell : interface()->columnHeaderCells()) {
        result.push_back(getWrappedInterfaceOrAbort(cell));
    }

    return result;
}

QList<QAccessibleInterface *> QAccessibleTableCellInterfaceWrapperImpl::rowHeaderCells() const
{
    QList<QAccessibleInterface *> result;
    for (auto *cell : interface()->rowHeaderCells()) {
        result.push_back(getWrappedInterfaceOrAbort(cell));
    }

    return result;
}

int QAccessibleTableCellInterfaceWrapperImpl::columnIndex() const
{
    return interface()->columnIndex();
}

int QAccessibleTableCellInterfaceWrapperImpl::rowIndex() const
{
    return interface()->rowIndex();
}

int QAccessibleTableCellInterfaceWrapperImpl::columnExtent() const
{
    return interface()->columnExtent();
}

int QAccessibleTableCellInterfaceWrapperImpl::rowExtent() const
{
    return interface()->rowExtent();
}

QAccessibleInterface *QAccessibleTableCellInterfaceWrapperImpl::table() const
{
    auto *tableInterface = interface()->table();

    return tableInterface != nullptr
        ? getWrappedInterfaceOrAbort(tableInterface)
        : nullptr;
}

QAccessibleActionInterfaceWrapperImpl::QAccessibleActionInterfaceWrapperImpl(
    QAccessibleActionInterface *interface)
    : SpecializedInterfaceHolder<QAccessibleActionInterface>(interface)
{
}

QStringList QAccessibleActionInterfaceWrapperImpl::actionNames() const
{
    return interface()->actionNames();
}

QString QAccessibleActionInterfaceWrapperImpl::localizedActionName(const QString &name) const
{
    return interface()->localizedActionName(name);
}

QString QAccessibleActionInterfaceWrapperImpl::localizedActionDescription(const QString &name) const
{
    return interface()->localizedActionDescription(name);
}

void QAccessibleActionInterfaceWrapperImpl::doAction(const QString &actionName)
{
    interface()->doAction(actionName);
}

QStringList QAccessibleActionInterfaceWrapperImpl::keyBindingsForAction(const QString &actionName) const
{
    return interface()->keyBindingsForAction(actionName);
}

QAccessibleTabButtonActionInterfaceWrapperImpl::QAccessibleTabButtonActionInterfaceWrapperImpl(
    QAccessibleInterface *tabBarInterface, QAccessibleActionInterface *actionInterface)
    : QAccessibleActionInterfaceWrapperImpl(actionInterface)
    , m_tabBarInterface(tabBarInterface)
{
}

QStringList QAccessibleTabButtonActionInterfaceWrapperImpl::actionNames() const
{
    return {
        setFocusAction(),
        pressAction(),
    };
}

void QAccessibleTabButtonActionInterfaceWrapperImpl::doAction(const QString &actionName)
{
    if (actionName == setFocusAction()) {
        m_tabBarInterface->actionInterface()->doAction(setFocusAction());
    } else if (actionName == pressAction()) {
        interface()->doAction(pressAction());
    }
}

}

namespace QAccessibleWrapper {

void startSession(const std::function<void()> &task)
{
    if (getAccessibleInterfacesWrappersSessionRef().hasValue()) {
        qOhosReportFatalErrorAndAbort("%s: another wrappers session already created.", Q_FUNC_INFO);
    }

    getAccessibleInterfacesWrappersSessionRef() = makeQOhosOptional(AccessibleInterfacesWrappersSession());
    task();
    getAccessibleInterfacesWrappersSessionRef().reset();
}

QAccessible::Id uniqueId(QAccessibleInterface *interfaceWrapper)
{
    return QAccessible::uniqueId(getRawInterfaceOrFail(interfaceWrapper));
}

QAccessibleInterface *accessibleInterface(QAccessible::Id id)
{
    return getWrappedInterfaceOrAbort(QAccessible::accessibleInterface(id));
}

}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
