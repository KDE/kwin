/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
