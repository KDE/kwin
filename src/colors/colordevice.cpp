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

struct CmsDeleter
{
    void operator()(cmsToneCurve *toneCurve)
    {
        if (toneCurve) {
            cmsFreeToneCurve(toneCurve);
        }
    }
};
using UniqueToneCurvePtr = std::unique_ptr<cmsToneCurve, CmsDeleter>;

class ColorDevicePrivate
{
public:
    enum DirtyToneCurveBit {
        DirtyTemperatureToneCurve = 0x1,
        DirtyBrightnessToneCurve = 0x2,
    };
    Q_DECLARE_FLAGS(DirtyToneCurves, DirtyToneCurveBit)

    void rebuildPipeline();

    void updateTemperatureToneCurves();
    void updateBrightnessToneCurves();

    Output *output;
    DirtyToneCurves dirtyCurves;
    QTimer *updateTimer;
    uint brightness = 100;
    uint temperature = 6500;

    std::unique_ptr<ColorPipelineStage> temperatureStage;
    QVector3D temperatureFactors = QVector3D(1, 1, 1);
    std::unique_ptr<ColorPipelineStage> brightnessStage;
    QVector3D brightnessFactors = QVector3D(1, 1, 1);

    std::shared_ptr<ColorTransformation> transformation;
    // used if only limited per-channel multiplication is available
    QVector3D simpleTransformation = QVector3D(1, 1, 1);
};

void ColorDevicePrivate::rebuildPipeline()
{
    if (dirtyCurves & DirtyBrightnessToneCurve) {
        updateBrightnessToneCurves();
    }
    if (dirtyCurves & DirtyTemperatureToneCurve) {
        updateTemperatureToneCurves();
    }
    dirtyCurves = DirtyToneCurves();

    std::vector<std::unique_ptr<ColorPipelineStage>> stages;
    if (brightnessStage) {
        if (auto s = brightnessStage->dup()) {
            stages.push_back(std::move(s));
        } else {
            return;
        }
    }
    if (temperatureStage) {
        if (auto s = temperatureStage->dup()) {
            stages.push_back(std::move(s));
        } else {
            return;
        }
    }

    const auto tmp = std::make_shared<ColorTransformation>(std::move(stages));
    if (tmp->valid()) {
        transformation = tmp;
        simpleTransformation = brightnessFactors * temperatureFactors;
    }
}

static qreal interpolate(qreal a, qreal b, qreal blendFactor)
{
    return (1 - blendFactor) * a + blendFactor * b;
}

void ColorDevicePrivate::updateTemperatureToneCurves()
{
    temperatureStage.reset();

    if (temperature == 6500) {
        return;
    }

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

    temperatureFactors = QVector3D(xWhitePoint, yWhitePoint, zWhitePoint);

    const double redCurveParams[] = {1.0, xWhitePoint, 0.0};
    const double greenCurveParams[] = {1.0, yWhitePoint, 0.0};
    const double blueCurveParams[] = {1.0, zWhitePoint, 0.0};

    UniqueToneCurvePtr redCurve(cmsBuildParametricToneCurve(nullptr, 2, redCurveParams));
    if (!redCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the temperature tone curve for the red channel";
        return;
    }
    UniqueToneCurvePtr greenCurve(cmsBuildParametricToneCurve(nullptr, 2, greenCurveParams));
    if (!greenCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the temperature tone curve for the green channel";
        return;
    }
    UniqueToneCurvePtr blueCurve(cmsBuildParametricToneCurve(nullptr, 2, blueCurveParams));
    if (!blueCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the temperature tone curve for the blue channel";
        return;
    }

    // The ownership of the tone curves will be moved to the pipeline stage.
    cmsToneCurve *toneCurves[] = {redCurve.release(), greenCurve.release(), blueCurve.release()};

    temperatureStage = std::make_unique<ColorPipelineStage>(cmsStageAllocToneCurves(nullptr, 3, toneCurves));
    if (!temperatureStage) {
        qCWarning(KWIN_CORE) << "Failed to create the color temperature pipeline stage";
    }
}

void ColorDevicePrivate::updateBrightnessToneCurves()
{
    brightnessStage.reset();

    if (brightness == 100) {
        return;
    }

    const double curveParams[] = {1.0, brightness / 100.0, 0.0};
    brightnessFactors = QVector3D(brightness / 100.0, brightness / 100.0, brightness / 100.0);

    UniqueToneCurvePtr redCurve(cmsBuildParametricToneCurve(nullptr, 2, curveParams));
    if (!redCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the brightness tone curve for the red channel";
        return;
    }

    UniqueToneCurvePtr greenCurve(cmsBuildParametricToneCurve(nullptr, 2, curveParams));
    if (!greenCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the brightness tone curve for the green channel";
        return;
    }

    UniqueToneCurvePtr blueCurve(cmsBuildParametricToneCurve(nullptr, 2, curveParams));
    if (!blueCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the brightness tone curve for the blue channel";
        return;
    }

    // The ownership of the tone curves will be moved to the pipeline stage.
    cmsToneCurve *toneCurves[] = {redCurve.release(), greenCurve.release(), blueCurve.release()};

    brightnessStage = std::make_unique<ColorPipelineStage>(cmsStageAllocToneCurves(nullptr, 3, toneCurves));
    if (!brightnessStage) {
        qCWarning(KWIN_CORE) << "Failed to create the color brightness pipeline stage";
    }
}

ColorDevice::ColorDevice(Output *output, QObject *parent)
    : QObject(parent)
    , d(new ColorDevicePrivate)
{
    d->updateTimer = new QTimer(this);
    d->updateTimer->setSingleShot(true);
    connect(d->updateTimer, &QTimer::timeout, this, &ColorDevice::update);
    connect(output, &Output::dpmsModeChanged, this, [this, output]() {
        if (output->dpmsMode() == Output::DpmsMode::On) {
            update();
        }
    });

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

uint ColorDevice::brightness() const
{
    return d->brightness;
}

void ColorDevice::setBrightness(uint brightness)
{
    if (brightness > 100) {
        qCWarning(KWIN_CORE) << "Got invalid brightness value:" << brightness;
        brightness = 100;
    }
    if (d->brightness == brightness) {
        return;
    }
    d->brightness = brightness;
    d->dirtyCurves |= ColorDevicePrivate::DirtyBrightnessToneCurve;
    scheduleUpdate();
    Q_EMIT brightnessChanged();
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
    d->dirtyCurves |= ColorDevicePrivate::DirtyTemperatureToneCurve;
    scheduleUpdate();
    Q_EMIT temperatureChanged();
}

void ColorDevice::update()
{
    d->rebuildPipeline();
    if (!d->output->setGammaRamp(d->transformation)) {
        d->output->setChannelFactors(d->simpleTransformation);
    }
}

void ColorDevice::scheduleUpdate()
{
    d->updateTimer->start();
}

} // namespace KWin

#include "moc_colordevice.cpp"
