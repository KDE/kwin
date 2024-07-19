/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "internalinputmethodcontext.h"

#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QObject>
#include <QRect>
#include <QTextCharFormat>

using namespace KWin;

InternalInputMethodContext::InternalInputMethodContext(QObject *parent)
    : QPlatformInputContext()
{
}

InternalInputMethodContext::~InternalInputMethodContext()
{
}

bool InternalInputMethodContext::isEnabled() const
{
    return QGuiApplication::focusObject() != nullptr;
}

QString InternalInputMethodContext::surroundingText() const
{
    return m_surroundingText;
}

int InternalInputMethodContext::cursorPosition() const
{
    return m_cursorPos;
}

int InternalInputMethodContext::anchorPosition() const
{
    return m_anchorPos;
}

bool InternalInputMethodContext::isValid() const
{
    return true;
}

void InternalInputMethodContext::update(Qt::InputMethodQueries queries)
{
    if (!QGuiApplication::focusObject())
        return;
    QInputMethodQueryEvent event(queries);
    QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);

    if ((queries & Qt::ImSurroundingText) || (queries & Qt::ImCursorPosition) || (queries & Qt::ImAnchorPosition)) {
        m_surroundingText = event.value(Qt::ImSurroundingText).toString();
        m_cursorPos = event.value(Qt::ImCursorPosition).toInt();
        m_anchorPos = event.value(Qt::ImAnchorPosition).toInt();
        Q_EMIT surroundingTextChanged();
    }

    if (queries & Qt::ImHints) {
        // When kwin gets some text input with numbers and passwords this needs pouplating
    }
}

void InternalInputMethodContext::showInputPanel()
{
    Q_EMIT showInputPanelRequested();
}

void InternalInputMethodContext::hideInputPanel()
{
    Q_EMIT hideInputPanelRequested();
}

bool InternalInputMethodContext::isInputPanelVisible() const
{
    return true;
}

QRectF InternalInputMethodContext::keyboardRect() const
{
    return QRectF();
}

QLocale InternalInputMethodContext::locale() const
{
    return QLocale();
}

Qt::LayoutDirection InternalInputMethodContext::inputDirection() const
{
    return Qt::LayoutDirection();
}

void InternalInputMethodContext::setFocusObject(QObject *object)
{
    Q_EMIT enabledChanged();
    update(Qt::ImQueryAll);
}

// From the InputMethod to our internal window

void InternalInputMethodContext::handlePreeditText(const QString &text, int cursorBegin, int cursorEnd)
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
    }

    // only use single underline style for now
    QTextCharFormat format;
    format.setFontUnderline(true);
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    QInputMethodEvent::Attribute attribute2(QInputMethodEvent::TextFormat,
                                            0,
                                            text.length(), format);
    attributes.append(attribute2);

    QInputMethodEvent event(text, attributes);

    QCoreApplication::sendEvent(focusObject, &event);
}

void InternalInputMethodContext::handleCommitString(const QString &text)
{
    QObject *focusObject = QGuiApplication::focusObject();
    if (!focusObject)
        return;

    QInputMethodEvent event(QString(), {});
    event.setCommitString(text);
    QCoreApplication::sendEvent(focusObject, &event);
}

void InternalInputMethodContext::handleDeleteSurroundingText(int index, uint length)
{
    QObject *focusObject = QGuiApplication::focusObject();
    if (!focusObject)
        return;

    QInputMethodEvent event(QString(), {});
    event.setCommitString(QString(), index, length);
    QCoreApplication::sendEvent(focusObject, &event);
}
