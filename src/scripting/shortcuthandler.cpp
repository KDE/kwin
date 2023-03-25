/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shortcuthandler.h"
#include "utils/common.h"

#include <KGlobalAccel>

#include <QAction>

namespace KWin
{

ShortcutHandler::ShortcutHandler(QObject *parent)
    : QObject(parent)
{
}

void ShortcutHandler::classBegin()
{
}

void ShortcutHandler::componentComplete()
{
    if (m_name.isEmpty()) {
        qCWarning(KWIN_CORE) << "ShortcutHandler.name is required";
        return;
    }
    if (m_text.isEmpty()) {
        qCWarning(KWIN_CORE) << "ShortcutHandler.text is required";
        return;
    }

    QAction *action = new QAction(this);
    connect(action, &QAction::triggered, this, &ShortcutHandler::activated);
    action->setObjectName(m_name);
    action->setText(m_text);
    KGlobalAccel::self()->setShortcut(action, {m_keySequence});
}

QString ShortcutHandler::name() const
{
    return m_name;
}

void ShortcutHandler::setName(const QString &name)
{
    if (m_action) {
        qCWarning(KWIN_CORE) << "ShortcutHandler.name cannot be changed";
        return;
    }
    if (m_name != name) {
        m_name = name;
        Q_EMIT nameChanged();
    }
}

QString ShortcutHandler::text() const
{
    return m_text;
}

void ShortcutHandler::setText(const QString &text)
{
    if (m_text != text) {
        m_text = text;
        if (m_action) {
            m_action->setText(text);
        }
        Q_EMIT textChanged();
    }
}

QVariant ShortcutHandler::sequence() const
{
    return m_userSequence;
}

void ShortcutHandler::setSequence(const QVariant &sequence)
{
    if (m_action) {
        qCWarning(KWIN_CORE) << "ShortcutHandler.sequence cannot be changed";
        return;
    }
    if (m_userSequence != sequence) {
        m_userSequence = sequence;
        m_keySequence = QKeySequence::fromString(sequence.toString());
        Q_EMIT sequenceChanged();
    }
}

} // namespace KWin
