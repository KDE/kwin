/*
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "orientationsensor.h"

#include <QOrientationSensor>

namespace KWin
{

OrientationSensor::OrientationSensor()
    : m_sensor(std::make_unique<QOrientationSensor>())
    , m_reading(std::make_unique<QOrientationReading>())
{
    m_reading->setOrientation(QOrientationReading::Orientation::Undefined);
    connect(m_sensor.get(), &QOrientationSensor::availableSensorsChanged, this, &OrientationSensor::availableChanged);
}

OrientationSensor::~OrientationSensor() = default;

bool OrientationSensor::isAvailable() const
{
    return !m_sensor->sensorsForType(m_sensor->type()).isEmpty();
}

void OrientationSensor::setEnabled(bool enable)
{
    if (enable) {
        connect(m_sensor.get(), &QOrientationSensor::readingChanged, this, &OrientationSensor::update, Qt::UniqueConnection);
        m_sensor->start();
        // after we enable the sensor, pick up current reading as device might have rotated meanwhile
        update();
    } else {
        disconnect(m_sensor.get(), &QOrientationSensor::readingChanged, this, &OrientationSensor::update);
        m_sensor->stop();
        m_reading->setOrientation(QOrientationReading::Undefined);
    }
}

QOrientationReading *OrientationSensor::reading() const
{
    return m_reading.get();
}

void OrientationSensor::update()
{
    if (auto reading = m_sensor->reading()) {
        if (m_reading->orientation() != reading->orientation()) {
            m_reading->setOrientation(reading->orientation());
            Q_EMIT orientationChanged();
        }
    } else if (m_reading->orientation() != QOrientationReading::Orientation::Undefined) {
        m_reading->setOrientation(QOrientationReading::Orientation::Undefined);
        Q_EMIT orientationChanged();
    }
}

}

#include "moc_orientationsensor.cpp"
