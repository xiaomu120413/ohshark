// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QT_NO_ACCESSIBILITY

#include <QtCore/qpointer.h>
#include <accessibility/qohosaccessibilityevents.h>
#include <accessibility/qohosaccessibilitysafeeventcopysupplier.h>
#include <qohosutils.h>

QT_BEGIN_NAMESPACE

namespace QOhos {

namespace {

template<typename AccessibleEvent>
struct AccessibleEventInfo
{
};

template<>
struct AccessibleEventInfo<QAccessibleEvent>
{
    QAccessible::Event type;
};

template<>
struct AccessibleEventInfo<QAccessibleValueChangeEvent>
{
    QVariant value;
};

template<>
struct AccessibleEventInfo<QAccessibleStateChangeEvent>
{
    QAccessible::State changedStates;
};

template<>
struct AccessibleEventInfo<QAccessibleTextCursorEvent>
{
    int cursorPosition;
};

template<>
struct AccessibleEventInfo<QAccessibleTextSelectionEvent>
{
    int selectionStart;
    int selectionEnd;
};

template<>
struct AccessibleEventInfo<QAccessibleTextInsertEvent>
{
    int cursorPosition;
    QString textInserted;
};

template<>
struct AccessibleEventInfo<QAccessibleTextRemoveEvent>
{
    int cursorPosition;
    QString textRemoved;
};

template<>
struct AccessibleEventInfo<QAccessibleTextUpdateEvent>
{
    int cursorPosition;
    QString textRemoved;
    QString textInserted;
};

template<>
struct AccessibleEventInfo<QAccessibleTableModelChangeEvent>
{
    QAccessibleTableModelChangeEvent::ModelChangeType modelChangeType;
    int firstRow;
    int lastRow;
    int firstColumn;
    int lastColumn;
};

template<typename BaseObject>
std::shared_ptr<QAccessibleValueChangeEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleValueChangeEvent> &eventInfo)
{
    return std::make_shared<QAccessibleValueChangeEvent>(baseObject, eventInfo.value);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleStateChangeEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleStateChangeEvent> &eventInfo)
{
    return std::make_shared<QAccessibleStateChangeEvent>(baseObject, eventInfo.changedStates);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleTextCursorEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleTextCursorEvent> &eventInfo)
{
    return std::make_shared<QAccessibleTextCursorEvent>(baseObject, eventInfo.cursorPosition);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleTextSelectionEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleTextSelectionEvent> &eventInfo)
{
    return std::make_shared<QAccessibleTextSelectionEvent>(
        baseObject, eventInfo.selectionStart, eventInfo.selectionEnd);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleTextInsertEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleTextInsertEvent> &eventInfo)
{
    return std::make_shared<QAccessibleTextInsertEvent>(
        baseObject, eventInfo.cursorPosition, eventInfo.textInserted);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleTextRemoveEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleTextRemoveEvent> &eventInfo)
{
    return std::make_shared<QAccessibleTextRemoveEvent>(
        baseObject, eventInfo.cursorPosition, eventInfo.textRemoved);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleTextUpdateEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleTextUpdateEvent> &eventInfo)
{
    return std::make_shared<QAccessibleTextUpdateEvent>(
        baseObject, eventInfo.cursorPosition, eventInfo.textRemoved, eventInfo.textInserted);
}

template<typename BaseObject>
std::shared_ptr<QAccessibleTableModelChangeEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleTableModelChangeEvent> &eventInfo)
{
    auto event = std::make_shared<QAccessibleTableModelChangeEvent>(
        baseObject, eventInfo.modelChangeType);

    event->setFirstRow(eventInfo.firstRow);
    event->setLastRow(eventInfo.lastRow);
    event->setFirstColumn(eventInfo.firstColumn);
    event->setLastColumn(eventInfo.lastColumn);

    return event;
}

template<typename BaseObject>
std::shared_ptr<QAccessibleEvent> makeQAccessibleEvent(
    BaseObject *baseObject, const AccessibleEventInfo<QAccessibleEvent> &eventInfo)
{
    return std::make_shared<QAccessibleEvent>(baseObject, eventInfo.type);
}

template<typename Event>
std::function<std::shared_ptr<Event>()> makeSafeQAccessibleEventCopySupplierImpl(
    const Event &event, const AccessibleEventInfo<Event> &eventInfo)
{
    if (event.object() != nullptr) {
        QPointer<QObject> objectPtr(event.object());

        return [child = event.child(), objectPtr, eventInfo]() -> std::shared_ptr<Event> {
            if (objectPtr.isNull()) {
                return nullptr;
            }

            auto eventCopy = makeQAccessibleEvent(objectPtr.data(), eventInfo);
            eventCopy->setChild(child);

            return eventCopy;
        };
    }

    return [uniqueId = event.uniqueId(), eventInfo]() {
        auto *interface = QAccessible::accessibleInterface(uniqueId);
        return interface != nullptr
            ? makeQAccessibleEvent(interface, eventInfo)
            : nullptr;
    };
}

std::function<std::shared_ptr<QOhosAccessiblePlatformEvent>()>
makeSafeQAccessibleCustomEventCopySupplier(const QOhosAccessiblePlatformEvent &platformEvent)
{
    QPointer<QObject> widgetObjectPtr = platformEvent.object();
    auto platformEventType = platformEvent.platformEventType();

    return [widgetObjectPtr, platformEventType]() {
        return !widgetObjectPtr.isNull()
            ? std::make_shared<QOhosAccessiblePlatformEvent>(widgetObjectPtr, platformEventType)
            : nullptr;
    };
}

}

QOhosSupplier<std::shared_ptr<QAccessibleEvent>> makeSafeQAccessibleEventCopySupplier(
    const QAccessibleEvent &event)
{
    switch (event.type()) {
    case QAccessible::Event::ValueChanged: {
        const auto *valueChangedEvent = static_cast<const QAccessibleValueChangeEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleValueChangeEvent>(
            *valueChangedEvent,
            {
                .value = valueChangedEvent->value(),
            });
    }
    case QAccessible::Event::StateChanged: {
        const auto *stateChangedEvent = static_cast<const QAccessibleStateChangeEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleStateChangeEvent>(
            *stateChangedEvent,
            {
                .changedStates = stateChangedEvent->changedStates(),
            });
    }
    case QAccessible::Event::TextCaretMoved: {
        const auto *textCaretMovedEvent = static_cast<const QAccessibleTextCursorEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleTextCursorEvent>(
            *textCaretMovedEvent,
            {
                .cursorPosition = textCaretMovedEvent->cursorPosition(),
            });
    }
    case QAccessible::Event::TextSelectionChanged: {
        const auto *textSelectionEvent = static_cast<const QAccessibleTextSelectionEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleTextSelectionEvent>(
            *textSelectionEvent,
            {
                .selectionStart = textSelectionEvent->selectionStart(),
                .selectionEnd = textSelectionEvent->selectionEnd(),
            });
    }
    case QAccessible::Event::TextInserted: {
        const auto *textInsertEvent = static_cast<const QAccessibleTextInsertEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleTextInsertEvent>(
            *textInsertEvent,
            {
                .cursorPosition = textInsertEvent->cursorPosition(),
                .textInserted = textInsertEvent->textInserted(),
            });
    }
    case QAccessible::Event::TextRemoved: {
        const auto *textRemovedEvent = static_cast<const QAccessibleTextRemoveEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleTextRemoveEvent>(
            *textRemovedEvent,
            {
                .cursorPosition = textRemovedEvent->cursorPosition(),
                .textRemoved = textRemovedEvent->textRemoved(),
            });
    }
    case QAccessible::Event::TextUpdated: {
        const auto *textUpdatedEvent = static_cast<const QAccessibleTextUpdateEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleTextUpdateEvent>(
            *textUpdatedEvent,
            {
                .cursorPosition = textUpdatedEvent->cursorPosition(),
                .textRemoved = textUpdatedEvent->textRemoved(),
                .textInserted = textUpdatedEvent->textInserted(),
            });
    }
    case QAccessible::Event::TableModelChanged: {
        const auto *tableModelChangedEvent = static_cast<const QAccessibleTableModelChangeEvent *>(&event);
        return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleTableModelChangeEvent>(
            *tableModelChangedEvent,
            {
                .modelChangeType = tableModelChangedEvent->modelChangeType(),
                .firstRow = tableModelChangedEvent->firstRow(),
                .lastRow = tableModelChangedEvent->lastRow(),
                .firstColumn = tableModelChangedEvent->firstColumn(),
                .lastColumn = tableModelChangedEvent->lastColumn(),
            });
    }
    case QAccessible::Event::OhosPlatformEvent: {
        return makeSafeQAccessibleCustomEventCopySupplier(
            static_cast<const QOhosAccessiblePlatformEvent &>(event));
    }
    default:
        break;
    }

    return makeSafeQAccessibleEventCopySupplierImpl<QAccessibleEvent>(
        event,
        {
            event.type(),
        });
}

}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
