/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lightsensor.h"

#include <QLightSensor>

namespace KWin
{

LightSensor::LightSensor()
    : m_sensor(std::make_unique<QLightSensor>())
{
    connect(m_sensor.get(), &QLightSensor::readingChanged, this, [this]() {
        m_isAvailable = true;
        Q_EMIT availableChanged();
        setEnabled(m_enabled);
    }, Qt::SingleShotConnection);
    m_sensor->start();
}

LightSensor::~LightSensor()
{
}

void LightSensor::setEnabled(bool enable)
{
    m_enabled = enable;
    if (!m_isAvailable) {
        // haven't received the first reading, this won't do anything
        return;
    }
    if (enable) {
        connect(m_sensor.get(), &QLightSensor::readingChanged, this, &LightSensor::update, Qt::UniqueConnection);
        m_sensor->start();
        // after we enable the sensor, pick up current reading as device might have rotated meanwhile
        update();
    } else {
        disconnect(m_sensor.get(), &QLightSensor::readingChanged, this, &LightSensor::update);
        m_sensor->stop();
    }
}

double LightSensor::lux() const
{
    return m_lux;
}

bool LightSensor::isAvailable() const
{
    return m_isAvailable;
}

void LightSensor::update()
{
    auto reading = m_sensor->reading();
    if (!reading) {
        return;
    }
    if (m_lux != reading->lux()) {
        m_lux = reading->lux();
        Q_EMIT brightnessChanged();
    }
}

}

#include "moc_lightsensor.cpp"
