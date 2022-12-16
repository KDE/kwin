/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input_event_spy.h"
#include <kwin_export.h>

#include <QObject>
#include <QSet>

namespace KWin
{

class KWIN_EXPORT ModifierOnlyShortcuts : public QObject, public InputEventSpy
{
    Q_OBJECT
public:
    explicit ModifierOnlyShortcuts();
    ~ModifierOnlyShortcuts() override;

    void keyEvent(KeyEvent *event) override;
    void pointerEvent(MouseEvent *event) override;
    void wheelEvent(WheelEvent *event) override;

    void reset()
    {
        m_modifier = Qt::NoModifier;
    }

private:
    Qt::KeyboardModifier m_modifier = Qt::NoModifier;
    Qt::KeyboardModifiers m_cachedMods;
    Qt::MouseButtons m_pressedButtons;
    QSet<quint32> m_pressedKeys;
};

}
