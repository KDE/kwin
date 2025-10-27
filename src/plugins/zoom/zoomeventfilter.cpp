/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Ritchie Frodomar <ritchie@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "zoomeventfilter.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QtCore>
#include <input_event.h>

KWin::ZoomEventFilter::ZoomEventFilter()
    : InputEventFilter(InputFilterOrder::Order::GlobalShortcut)
{
    m_moveLeftAction = new QAction(this);
    m_moveLeftAction->setObjectName(QStringLiteral("MoveZoomLeft"));
    m_moveLeftAction->setText(i18n("Move Zoomed Area to Left"));
    KGlobalAccel::self()->setDefaultShortcut(m_moveLeftAction, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(m_moveLeftAction, QList<QKeySequence>());

    m_moveRightAction = new QAction(this);
    m_moveRightAction->setObjectName(QStringLiteral("MoveZoomRight"));
    m_moveRightAction->setText(i18n("Move Zoomed Area to Right"));
    KGlobalAccel::self()->setDefaultShortcut(m_moveRightAction, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(m_moveRightAction, QList<QKeySequence>());

    m_moveUpAction = new QAction(this);
    m_moveUpAction->setObjectName(QStringLiteral("MoveZoomUp"));
    m_moveUpAction->setText(i18n("Move Zoomed Area Upwards"));
    KGlobalAccel::self()->setDefaultShortcut(m_moveUpAction, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(m_moveUpAction, QList<QKeySequence>());

    m_moveDownAction = new QAction(this);
    m_moveDownAction->setObjectName(QStringLiteral("MoveZoomDown"));
    m_moveDownAction->setText(i18n("Move Zoomed Area Downwards"));
    KGlobalAccel::self()->setDefaultShortcut(m_moveDownAction, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(m_moveDownAction, QList<QKeySequence>());
}

bool KWin::ZoomEventFilter::keyboardKey(KeyboardKeyEvent *event)
{
    QKeyCombination combination = QKeyCombination(event->modifiersRelevantForGlobalShortcuts, event->key);
    QKeySequence sequence = QKeySequence(combination);

    bool isPressedOrRepeated = event->state != KeyboardKeyState::Released;

    bool leftInput = leftIsPressed;
    bool rightInput = rightIsPressed;
    bool upInput = upIsPressed;
    bool downInput = downIsPressed;

    if (this->matchShortcut(m_moveLeftAction, sequence)) {
        leftInput = isPressedOrRepeated;
    }

    if (this->matchShortcut(m_moveRightAction, sequence)) {
        rightInput = isPressedOrRepeated;
    }

    if (this->matchShortcut(m_moveUpAction, sequence)) {
        upInput = isPressedOrRepeated;
    }

    if (this->matchShortcut(m_moveDownAction, sequence)) {
        downInput = isPressedOrRepeated;
    }

    bool hasDirectionChanged = (leftInput != leftIsPressed) || (rightInput != rightIsPressed) || (upInput != upIsPressed) || (downInput != downIsPressed);
    if (!hasDirectionChanged) {
        return InputEventFilter::keyboardKey(event);
    }

    leftIsPressed = leftInput;
    rightIsPressed = rightInput;
    upIsPressed = upInput;
    downIsPressed = downInput;

    int xAxis = 0;
    int yAxis = 0;

    if (rightIsPressed) {
        xAxis += 1;
    }
    if (leftIsPressed) {
        xAxis -= 1;
    }
    if (downIsPressed) {
        yAxis += 1;
    }
    if (upIsPressed) {
        yAxis -= 1;
    }

    Q_EMIT panDirectionChanged(QPoint(xAxis, yAxis));
    return true;
}

bool KWin::ZoomEventFilter::matchShortcut(const QAction *action, const QKeySequence &sequence)
{
    QList<QKeySequence> shortcutSequences = KGlobalAccel::self()->shortcut(action);
    for (const QKeySequence &shortcutSequence : shortcutSequences) {
        if (shortcutSequence.matches(sequence)) {
            return true;
        }
    }
    return false;
}

#include "moc_zoomeventfilter.cpp"
