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
#ifndef KWIN_KEYBOARD_REPEAT
#define KWIN_KEYBOARD_REPEAT

#include "input_event_spy.h"

#include <QObject>

class QTimer;

namespace KWin
{
class Xkb;

class KeyboardRepeat : public QObject, public InputEventSpy
{
    Q_OBJECT
public:
    explicit KeyboardRepeat(Xkb *xkb);
    ~KeyboardRepeat() override;

    void keyEvent(KeyEvent *event) override;

Q_SIGNALS:
    void keyRepeat(quint32 key, quint32 time);

private:
    void handleKeyRepeat();
    QTimer *m_timer;
    Xkb *m_xkb;
    quint32 m_time;
    quint32 m_key;
};


}

#endif
