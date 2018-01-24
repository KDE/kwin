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
#include "../keyboard_input.h"
#include <kwinglobals.h>

#include <QObject>
#include <QPointer>
#include <QSize>
#include <QMutex>
#include <QVector>
#include <QStringList>

class QSocketNotifier;
class QThread;

namespace KWin
{
namespace LibInput
{

class Event;
class Device;
class Context;

class Connection : public QObject
{
    Q_OBJECT

public:
    ~Connection();

    void setInputConfig(const KSharedConfigPtr &config) {
        m_config = config;
    }

    void setup();
    /**
     * Sets the screen @p size. This is needed for mapping absolute pointer events to
     * the screen data.
     **/
    void setScreenSize(const QSize &size);

    void updateScreens();

    bool hasKeyboard() const {
        return m_keyboard > 0;
    }
    bool hasAlphaNumericKeyboard() const {
        return m_alphaNumericKeyboard > 0;
    }
    bool hasTouch() const {
        return m_touch > 0;
    }
    bool hasPointer() const {
        return m_pointer > 0;
    }
    bool hasTabletModeSwitch() const {
        return m_tabletModeSwitch > 0;
    }

    bool isSuspended() const;

    void deactivate();

    void processEvents();

    void toggleTouchpads();
    void enableTouchpads();
    void disableTouchpads();

    QVector<Device*> devices() const {
        return m_devices;
    }

    QStringList devicesSysNames() const;

    void updateLEDs(KWin::Xkb::LEDs leds);

    static void createThread();

Q_SIGNALS:
    void keyChanged(quint32 key, KWin::InputRedirection::KeyboardKeyState, quint32 time, KWin::LibInput::Device *device);
    void pointerButtonChanged(quint32 button, KWin::InputRedirection::PointerButtonState state, quint32 time, KWin::LibInput::Device *device);
    void pointerMotionAbsolute(QPointF orig, QPointF screen, quint32 time, KWin::LibInput::Device *device);
    void pointerMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint32 time, quint64 timeMicroseconds, KWin::LibInput::Device *device);
    void pointerAxisChanged(KWin::InputRedirection::PointerAxis axis, qreal delta, quint32 time, KWin::LibInput::Device *device);
    void touchFrame(KWin::LibInput::Device *device);
    void touchCanceled(KWin::LibInput::Device *device);
    void touchDown(qint32 id, const QPointF &absolutePos, quint32 time, KWin::LibInput::Device *device);
    void touchUp(qint32 id, quint32 time, KWin::LibInput::Device *device);
    void touchMotion(qint32 id, const QPointF &absolutePos, quint32 time, KWin::LibInput::Device *device);
    void hasKeyboardChanged(bool);
    void hasAlphaNumericKeyboardChanged(bool);
    void hasPointerChanged(bool);
    void hasTouchChanged(bool);
    void hasTabletModeSwitchChanged(bool);
    void deviceAdded(KWin::LibInput::Device *);
    void deviceRemoved(KWin::LibInput::Device *);
    void deviceAddedSysName(QString);
    void deviceRemovedSysName(QString);
    void swipeGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device);
    void swipeGestureUpdate(const QSizeF &delta, quint32 time, KWin::LibInput::Device *device);
    void swipeGestureEnd(quint32 time, KWin::LibInput::Device *device);
    void swipeGestureCancelled(quint32 time, KWin::LibInput::Device *device);
    void pinchGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device);
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time, KWin::LibInput::Device *device);
    void pinchGestureEnd(quint32 time, KWin::LibInput::Device *device);
    void pinchGestureCancelled(quint32 time, KWin::LibInput::Device *device);
    void switchToggledOn(quint32 time, quint64 timeMicroseconds, KWin::LibInput::Device *device);
    void switchToggledOff(quint32 time, quint64 timeMicroseconds, KWin::LibInput::Device *device);

    void eventsRead();

private Q_SLOTS:
    void doSetup();
    void slotKGlobalSettingsNotifyChange(int type, int arg);

private:
    Connection(Context *input, QObject *parent = nullptr);
    void handleEvent();
    void applyDeviceConfig(Device *device);
    void applyScreenToDevice(Device *device);
    Context *m_input;
    QSocketNotifier *m_notifier;
    QSize m_size;
    int m_keyboard = 0;
    int m_alphaNumericKeyboard = 0;
    int m_pointer = 0;
    int m_touch = 0;
    int m_tabletModeSwitch = 0;
    bool m_keyboardBeforeSuspend = false;
    bool m_alphaNumericKeyboardBeforeSuspend = false;
    bool m_pointerBeforeSuspend = false;
    bool m_touchBeforeSuspend = false;
    bool m_tabletModeSwitchBeforeSuspend = false;
    QMutex m_mutex;
    QVector<Event*> m_eventQueue;
    bool wasSuspended = false;
    QVector<Device*> m_devices;
    KSharedConfigPtr m_config;
    bool m_touchpadsEnabled = true;
    Xkb::LEDs m_leds;

    KWIN_SINGLETON(Connection)
    static QPointer<QThread> s_thread;
};

}
}

#endif
