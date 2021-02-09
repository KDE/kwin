/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MODIFIER_ONLY_SHORTCUTS_H
#define KWIN_MODIFIER_ONLY_SHORTCUTS_H

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

    void reset() {
        m_modifier = Qt::NoModifier;
    }

private:
    Qt::KeyboardModifier m_modifier = Qt::NoModifier;
    Qt::KeyboardModifiers m_cachedMods;
    uint m_buttonPressCount = 0;
    QSet<quint32> m_pressedKeys;
};

}

#endif
