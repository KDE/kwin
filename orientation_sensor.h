/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#pragma once

#include <QObject>

#include <kwin_export.h>

class QOrientationSensor;
class KStatusNotifierItem;

namespace KWin
{

class KWIN_EXPORT OrientationSensor : public QObject
{
    Q_OBJECT
public:
    explicit OrientationSensor(QObject *parent = nullptr);
    ~OrientationSensor();

    void setEnabled(bool enabled);

    /**
     * Just like QOrientationReading::Orientation,
     * copied to not leak the QSensors API into internal API.
     **/
    enum class Orientation {
        Undefined,
        TopUp,
        TopDown,
        LeftUp,
        RightUp,
        FaceUp,
        FaceDown
    };

    Orientation orientation() const {
        return m_orientation;
    }

Q_SIGNALS:
    void orientationChanged();

private:
    void setupStatusNotifier();
    void startStopSensor();
    QOrientationSensor *m_sensor;
    bool m_enabled = false;
    bool m_userEnabled = true;
    Orientation m_orientation = Orientation::Undefined;
    KStatusNotifierItem *m_sni = nullptr;

};

}
