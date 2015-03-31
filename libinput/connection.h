/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_LIBINPUT_CONNECTION_H
#define KWIN_LIBINPUT_CONNECTION_H

#include "../input.h"
#include <kwinglobals.h>

#include <QObject>
#include <QSize>

class QSocketNotifier;

namespace KWin
{
namespace LibInput
{

class Context;

class Connection : public QObject
{
    Q_OBJECT
public:
    ~Connection();

    void setup();
    /**
     * Sets the screen @p size. This is needed for mapping absolute pointer events to
     * the screen data.
     **/
    void setScreenSize(const QSize &size);

    bool hasKeyboard() const {
        return m_keyboard > 0;
    }
    bool hasTouch() const {
        return m_touch > 0;
    }
    bool hasPointer() const {
        return m_pointer > 0;
    }

    bool isSuspended() const;

Q_SIGNALS:
    void keyChanged(uint32_t key, InputRedirection::KeyboardKeyState, uint32_t time);
    void pointerButtonChanged(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time);
    void pointerMotionAbsolute(QPointF orig, QPointF screen, uint32_t time);
    void pointerMotion(QPointF delta, uint32_t time);
    void pointerAxisChanged(InputRedirection::PointerAxis axis, qreal delta, uint32_t time);
    void touchFrame();
    void touchCanceled();
    void touchDown(qint32 id, const QPointF &absolutePos, quint32 time);
    void touchUp(qint32 id, quint32 time);
    void touchMotion(qint32 id, const QPointF &absolutePos, quint32 time);
    void hasKeyboardChanged(bool);
    void hasPointerChanged(bool);
    void hasTouchChanged(bool);

private:
    Connection(Context *input, QObject *parent = nullptr);
    void handleEvent();
    Context *m_input;
    QSocketNotifier *m_notifier;
    QSize m_size;
    int m_keyboard = 0;
    int m_pointer = 0;
    int m_touch = 0;
    bool m_keyboardBeforeSuspend = false;
    bool m_pointerBeforeSuspend = false;
    bool m_touchBeforeSuspend = false;

    KWIN_SINGLETON(Connection)
};

}
}

#endif
