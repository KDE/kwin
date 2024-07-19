#pragma once

#include "internalinputcontext.h"

#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QObject>
#include <QRect>

void InternalInputContext::setPreeditText(const QString &text, int cursorBegin, int cursorEnd)
{
    QObject *focusObject = QGuiApplication::focusObject();
    if (!focusObject) {
        return;
    }
    QList<QInputMethodEvent::Attribute> attributes;
    {
        if (cursorBegin != -1 || cursorEnd != -1) {
            // Current supported cursor shape is just line.
            // It means, cursorEnd and cursorBegin are the same.
            QInputMethodEvent::Attribute attribute1(QInputMethodEvent::Cursor,
                                                    text.length(),
                                                    1);
            attributes.append(attribute1);
        }
        // maybe add a styling attributes
    }
    QInputMethodEvent event(text, attributes);

    QCoreApplication::sendEvent(focusObject, &event);

    // apparently we also maybe need to updateState?
}

void InternalInputContext::commitText(const QString &text)
{
    QInputMethodEvent event(QString(), {});

    QObject *focusObject = QGuiApplication::focusObject();
    event.setCommitString(text);
    if (!focusObject)
        return;
    QCoreApplication::sendEvent(focusObject, &event);
}
