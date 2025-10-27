/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Ritchie Frodomar <ritchie@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "input.h"

class QAction;

namespace KWin
{

class ZoomEventFilter : public QObject, public InputEventFilter
{
    Q_OBJECT

public:
    explicit ZoomEventFilter();

    bool keyboardKey(KeyboardKeyEvent *event) override;

Q_SIGNALS:
    void panDirectionChanged(const QPoint &direction);

private:
    bool matchShortcut(const QAction *action, const QKeySequence &sequence);

private:
    QAction *m_moveLeftAction;
    QAction *m_moveRightAction;
    QAction *m_moveUpAction;
    QAction *m_moveDownAction;
    bool leftIsPressed;
    bool rightIsPressed;
    bool upIsPressed;
    bool downIsPressed;
};

};