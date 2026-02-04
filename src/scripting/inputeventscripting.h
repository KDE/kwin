/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 KWin Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "input_event.h"
#include "input_event_spy.h"

#include <QObject>

namespace KWin
{

class InputEventScriptingSpy : public QObject, public InputEventSpy
{
    Q_OBJECT
public:
    explicit InputEventScriptingSpy(QObject *parent = nullptr);
    ~InputEventScriptingSpy() override = default;

    void pointerMotion(PointerMotionEvent *event) override;
    void pointerButton(PointerButtonEvent *event) override;
    void keyboardKey(KeyboardKeyEvent *event) override;

Q_SIGNALS:
    void mouseMoved(KWin::PointerMotionEvent event);
    void mousePressed(KWin::PointerButtonEvent event);
    void mouseReleased(KWin::PointerButtonEvent event);
    void keyPressed(KWin::KeyboardKeyEvent event);
    void keyReleased(KWin::KeyboardKeyEvent event);
};

} // namespace KWin
