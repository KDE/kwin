/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef MOCK_LIBINPUT_H
#define MOCK_LIBINPUT_H
#include <libinput.h>

#include <QByteArray>
#include <QPointF>
#include <QSizeF>
#include <QVector>

struct libinput_device {
    bool keyboard = false;
    bool pointer = false;
    bool touch = false;
    bool tabletTool = false;
    bool gestureSupported = false;
    QByteArray name;
    QByteArray sysName;
    QByteArray outputName;
    quint32 product = 0;
    quint32 vendor = 0;
    int tapFingerCount = 0;
    QSizeF deviceSize;
    int deviceSizeReturnValue = 0;
    bool tapEnabledByDefault = false;
    bool tapToClick = false;
    bool tapAndDragEnabledByDefault = false;
    bool tapAndDrag = false;
    bool tapDragLockEnabledByDefault = false;
    bool tapDragLock = false;
    bool supportsDisableWhileTyping = false;
    bool supportsPointerAcceleration = false;
    bool supportsLeftHanded = false;
    bool supportsCalibrationMatrix = false;
    bool supportsDisableEvents = false;
    bool supportsDisableEventsOnExternalMouse = false;
    qreal pointerAcceleration = 0.0;
    int setPointerAccelerationReturnValue = 0;
    bool leftHanded = false;
    int setLeftHandedReturnValue = 0;
    Qt::MouseButtons supportedButtons;
    QVector<quint32> keys;
    bool enabled = true;
    int setEnableModeReturnValue = 0;
    int setTapToClickReturnValue = 0;
    int setTapAndDragReturnValue = 0;
    int setTapDragLockReturnValue = 0;
};

struct libinput_event {
    libinput_device *device = nullptr;
    libinput_event_type type = LIBINPUT_EVENT_NONE;
    quint32 time = 0;
};

struct libinput_event_keyboard : libinput_event {
    libinput_event_keyboard() {
        type = LIBINPUT_EVENT_KEYBOARD_KEY;
    }
    libinput_key_state state = LIBINPUT_KEY_STATE_RELEASED;
    quint32 key = 0;
};

struct libinput_event_pointer : libinput_event {
    libinput_button_state buttonState = LIBINPUT_BUTTON_STATE_RELEASED;
    quint32 button = 0;
    bool verticalAxis = false;
    bool horizontalAxis = false;
    qreal horizontalAxisValue = 0.0;
    qreal verticalAxisValue = 0.0;
    QSizeF delta;
    QPointF absolutePos;
};

struct libinput_event_touch : libinput_event {
    qint32 slot = -1;
    QPointF absolutePos;
};

struct libinput_event_gesture : libinput_event {
    int fingerCount = 0;
    bool cancelled = false;
    QSizeF delta = QSizeF(0, 0);
    qreal scale = 0.0;
    qreal angleDelta = 0.0;
};

struct libinput {
    int refCount = 1;
    QByteArray seat;
    int assignSeatRetVal = 0;
};

#endif
