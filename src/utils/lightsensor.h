/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <memory>

class QLightSensor;

namespace KWin
{

class LightSensor : public QObject
{
    Q_OBJECT
public:
    explicit LightSensor();
    ~LightSensor();

    void setEnabled(bool enable);
    double lux() const;

    bool isAvailable() const;

Q_SIGNALS:
    void brightnessChanged();
    void availableChanged();

private:
    void update();

    const std::unique_ptr<QLightSensor> m_sensor;
    double m_lux = 0.0;
    bool m_isAvailable = false;
    bool m_enabled = false;
};

}
