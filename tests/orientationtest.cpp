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
#include "../orientation_sensor.h"
#include <QApplication>
#include <QDebug>

using KWin::OrientationSensor;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    OrientationSensor sensor;
    QObject::connect(&sensor, &OrientationSensor::orientationChanged,
        [&sensor] {
            const auto orientation = sensor.orientation();
            switch (orientation) {
            case OrientationSensor::Orientation::Undefined:
                qDebug() << "Undefined";
                break;
            case OrientationSensor::Orientation::TopUp:
                qDebug() << "TopUp";
                break;
            case OrientationSensor::Orientation::TopDown:
                qDebug() << "TopDown";
                break;
            case OrientationSensor::Orientation::LeftUp:
                qDebug() << "LeftUp";
                break;
            case OrientationSensor::Orientation::RightUp:
                qDebug() << "RightUp";
                break;
            case OrientationSensor::Orientation::FaceUp:
                qDebug() << "FaceUp";
                break;
            case OrientationSensor::Orientation::FaceDown:
                qDebug() << "FaceDown";
                break;
            }
        }
    );
    sensor.setEnabled(true);

    return app.exec();
}
