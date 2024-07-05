/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colordevice.h"
#include "core/colorpipelinestage.h"
#include "core/colortransformation.h"
#include "core/output.h"
#include "utils/common.h"

#include "3rdparty/colortemperature.h"

#include <QTimer>

#include <lcms2.h>

namespace KWin
{

class ColorDevicePrivate
{
public:
    void recalculateFactors();

    Output *output;
    QTimer *updateTimer;
    uint temperature = 6500;

    QVector3D temperatureFactors = QVector3D(1, 1, 1);
};

static qreal interpolate(qreal a, qreal b, qreal blendFactor)
{
    return (1 - blendFactor) * a + blendFactor * b;
}

void ColorDevicePrivate::recalculateFactors()
{
    if (temperature == 6500) {
        temperatureFactors = QVector3D(1, 1, 1);
    } else {
        // Note that cmsWhitePointFromTemp() returns a slightly green-ish white point.
        const int blackBodyColorIndex = ((temperature - 1000) / 100) * 3;
        const qreal blendFactor = (temperature % 100) / 100.0;

        const qreal xWhitePoint = interpolate(blackbodyColor[blackBodyColorIndex + 0],
                                              blackbodyColor[blackBodyColorIndex + 3],
                                              blendFactor);
        const qreal yWhitePoint = interpolate(blackbodyColor[blackBodyColorIndex + 1],
                                              blackbodyColor[blackBodyColorIndex + 4],
                                              blendFactor);
        const qreal zWhitePoint = interpolate(blackbodyColor[blackBodyColorIndex + 2],
                                              blackbodyColor[blackBodyColorIndex + 5],
                                              blendFactor);
        // the values in the blackbodyColor array are "gamma corrected", but we need a linear value
        temperatureFactors = TransferFunction(TransferFunction::gamma22, 0, 1).encodedToNits(QVector3D(xWhitePoint, yWhitePoint, zWhitePoint));
    }
}

ColorDevice::ColorDevice(Output *output, QObject *parent)
    : QObject(parent)
    , d(new ColorDevicePrivate)
{
    d->updateTimer = new QTimer(this);
    d->updateTimer->setSingleShot(true);
    connect(d->updateTimer, &QTimer::timeout, this, &ColorDevice::update);

    d->output = output;
    scheduleUpdate();
}

ColorDevice::~ColorDevice()
{
}

Output *ColorDevice::output() const
{
    return d->output;
}

uint ColorDevice::temperature() const
{
    return d->temperature;
}

void ColorDevice::setTemperature(uint temperature)
{
    if (temperature > 6500) {
        qCWarning(KWIN_CORE) << "Got invalid temperature value:" << temperature;
        temperature = 6500;
    }
    if (d->temperature == temperature) {
        return;
    }
    d->temperature = temperature;
    scheduleUpdate();
    Q_EMIT temperatureChanged();
}

void ColorDevice::update()
{
    d->recalculateFactors();
    d->output->setChannelFactors(d->temperatureFactors);
}

void ColorDevice::scheduleUpdate()
{
    d->updateTimer->start();
}

} // namespace KWin

#include "moc_colordevice.cpp"
