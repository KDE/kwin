/*
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <memory>

class QOrientationSensor;
class QOrientationReading;

namespace KWin
{

class OrientationSensor : public QObject
{
    Q_OBJECT
public:
    explicit OrientationSensor();
    ~OrientationSensor();

    void setEnabled(bool enable);
    QOrientationReading *reading() const;

Q_SIGNALS:
    void orientationChanged();

private:
    void update();

    const std::unique_ptr<QOrientationSensor> m_sensor;
    const std::unique_ptr<QOrientationReading> m_reading;
};

}
