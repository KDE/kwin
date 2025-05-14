/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "internalinputmethodcontext.h"

#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QObject>
#include <QRect>
#include <QTextCharFormat>

namespace KWin {

InternalInputMethodContext::InternalInputMethodContext(QObject *parent)
    : QPlatformInputContext()
{
    setParent(parent);
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
    if (!QGuiApplication::focusObject()) {
        return;
    }
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
    if (!inputMethodAccepted()) {
        return;
    }
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

    if (cursorBegin != -1 || cursorEnd != -1) {
        // Current supported cursor shape is just line.
        // It means, cursorEnd and cursorBegin are the same.
        QInputMethodEvent::Attribute attribute1(QInputMethodEvent::Cursor,
                                                text.length(),
                                                1);
        attributes.append(attribute1);
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
    if (!focusObject) {
        return;
    }

    QInputMethodEvent event(QString(), {});
    event.setCommitString(text);
    QCoreApplication::sendEvent(focusObject, &event);
}

void InternalInputMethodContext::handleDeleteSurroundingText(int deleteBefore, uint deleteAfter)
{
    QObject *focusObject = QGuiApplication::focusObject();
    if (!focusObject) {
        return;
    }
    // insprired by QtWayland client code
    const QString &surrounding = QInputMethod::queryFocusObject(Qt::ImSurroundingText, QVariant()).toString();
    const int cursor = QInputMethod::queryFocusObject(Qt::ImCursorPosition, QVariant()).toInt();
    const int anchor = QInputMethod::queryFocusObject(Qt::ImAnchorPosition, QVariant()).toInt();

    const int selectionStart = qMin(cursor, anchor);
    const int selectionEnd = qMax(cursor, anchor);

    const int deleteBeforeTrasnslated = selectionStart - indexFromWayland(surrounding, -deleteBefore, selectionStart);
    const int deleteAfterTranslated = indexFromWayland(surrounding, deleteAfter, selectionEnd) - selectionEnd;

    QInputMethodEvent event(QString(), {});
    event.setCommitString(QString(), deleteBeforeTrasnslated, deleteAfterTranslated);
    QCoreApplication::sendEvent(focusObject, &event);
}

int InternalInputMethodContext::indexFromWayland(const QString &text, int length, int base)
{
    if (length == 0) {
        return base;
    }
    if (length < 0) {
        const QByteArray &utf8 = QStringView{text}.left(base).toUtf8();
        return QString::fromUtf8(utf8.first(qMax(utf8.size() + length, 0))).size();
    } else {
        const QByteArray &utf8 = QStringView{text}.mid(base).toUtf8();
        return QString::fromUtf8(utf8.first(qMin(length, utf8.size()))).size() + base;
    }
}

} //namespace
