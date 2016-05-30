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

#include <QByteArray>
#include <QSizeF>
#include <QVector>

struct libinput_device {
    bool keyboard = false;
    bool pointer = false;
    bool touch = false;
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
};

#endif
