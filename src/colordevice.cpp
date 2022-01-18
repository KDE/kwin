/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colordevice.h"
#include "abstract_output.h"
#include "utils/common.h"

#include "3rdparty/colortemperature.h"

#include <QTimer>

#include <lcms2.h>

namespace KWin
{

template <typename T>
struct CmsDeleter;

template <typename T>
using CmsScopedPointer = QScopedPointer<T, CmsDeleter<T>>;

template <>
struct CmsDeleter<cmsPipeline>
{
    static inline void cleanup(cmsPipeline *pipeline)
    {
        if (pipeline) {
            cmsPipelineFree(pipeline);
        }
    }
};

template <>
struct CmsDeleter<cmsStage>
{
    static inline void cleanup(cmsStage *stage)
    {
        if (stage) {
            cmsStageFree(stage);
        }
    }
};

template <>
struct CmsDeleter<cmsToneCurve>
{
    static inline void cleanup(cmsToneCurve *toneCurve)
    {
        if (toneCurve) {
            cmsFreeToneCurve(toneCurve);
        }
    }
};

class ColorDevicePrivate
{
public:
    enum DirtyToneCurveBit {
        DirtyTemperatureToneCurve = 0x1,
        DirtyBrightnessToneCurve = 0x2,
        DirtyCalibrationToneCurve = 0x4,
    };
    Q_DECLARE_FLAGS(DirtyToneCurves, DirtyToneCurveBit)

    void rebuildPipeline();
    void unlinkPipeline();

    void updateTemperatureToneCurves();
    void updateBrightnessToneCurves();
    void updateCalibrationToneCurves();

    AbstractOutput *output;
    DirtyToneCurves dirtyCurves;
    QTimer *updateTimer;
    QString profile;
    uint brightness = 100;
    uint temperature = 6500;

    CmsScopedPointer<cmsStage> temperatureStage;
    CmsScopedPointer<cmsStage> brightnessStage;
    CmsScopedPointer<cmsStage> calibrationStage;

    CmsScopedPointer<cmsPipeline> pipeline;
};

void ColorDevicePrivate::rebuildPipeline()
{
    if (!pipeline) {
        pipeline.reset(cmsPipelineAlloc(nullptr, 3, 3));
    }

    unlinkPipeline();

    if (dirtyCurves & DirtyCalibrationToneCurve) {
        updateCalibrationToneCurves();
    }
    if (dirtyCurves & DirtyBrightnessToneCurve) {
        updateBrightnessToneCurves();
    }
    if (dirtyCurves & DirtyTemperatureToneCurve) {
        updateTemperatureToneCurves();
    }

    dirtyCurves = DirtyToneCurves();

    if (calibrationStage) {
        if (!cmsPipelineInsertStage(pipeline.data(), cmsAT_END, calibrationStage.data())) {
            qCWarning(KWIN_CORE) << "Failed to insert the color calibration pipeline stage";
        }
    }
    if (temperatureStage) {
        if (!cmsPipelineInsertStage(pipeline.data(), cmsAT_END, temperatureStage.data())) {
            qCWarning(KWIN_CORE) << "Failed to insert the  color temperature pipeline stage";
        }
    }
    if (brightnessStage) {
        if (!cmsPipelineInsertStage(pipeline.data(), cmsAT_END, brightnessStage.data())) {
            qCWarning(KWIN_CORE) << "Failed to insert the color brightness pipeline stage";
        }
    }
}

void ColorDevicePrivate::unlinkPipeline()
{
    while (true) {
        cmsStage *last = nullptr;
        cmsPipelineUnlinkStage(pipeline.data(), cmsAT_END, &last);

        if (!last) {
            break;
        }
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

    const double redCurveParams[] = { 1.0, xWhitePoint, 0.0 };
    const double greenCurveParams[] = { 1.0, yWhitePoint, 0.0 };
    const double blueCurveParams[] = { 1.0, zWhitePoint, 0.0 };

    CmsScopedPointer<cmsToneCurve> redCurve(cmsBuildParametricToneCurve(nullptr, 2, redCurveParams));
    if (!redCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the temperature tone curve for the red channel";
        return;
    }
    CmsScopedPointer<cmsToneCurve> greenCurve(cmsBuildParametricToneCurve(nullptr, 2, greenCurveParams));
    if (!greenCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the temperature tone curve for the green channel";
        return;
    }
    CmsScopedPointer<cmsToneCurve> blueCurve(cmsBuildParametricToneCurve(nullptr, 2, blueCurveParams));
    if (!blueCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the temperature tone curve for the blue channel";
        return;
    }

    // The ownership of the tone curves will be moved to the pipeline stage.
    cmsToneCurve *toneCurves[] = { redCurve.take(), greenCurve.take(), blueCurve.take() };

    temperatureStage.reset(cmsStageAllocToneCurves(nullptr, 3, toneCurves));
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

    const double curveParams[] = { 1.0, brightness / 100.0, 0.0 };

    CmsScopedPointer<cmsToneCurve> redCurve(cmsBuildParametricToneCurve(nullptr, 2, curveParams));
    if (!redCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the brightness tone curve for the red channel";
        return;
    }

    CmsScopedPointer<cmsToneCurve> greenCurve(cmsBuildParametricToneCurve(nullptr, 2, curveParams));
    if (!greenCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the brightness tone curve for the green channel";
        return;
    }

    CmsScopedPointer<cmsToneCurve> blueCurve(cmsBuildParametricToneCurve(nullptr, 2, curveParams));
    if (!blueCurve) {
        qCWarning(KWIN_CORE) << "Failed to build the brightness tone curve for the blue channel";
        return;
    }

    // The ownership of the tone curves will be moved to the pipeline stage.
    cmsToneCurve *toneCurves[] = { redCurve.take(), greenCurve.take(), blueCurve.take() };

    brightnessStage.reset(cmsStageAllocToneCurves(nullptr, 3, toneCurves));
    if (!brightnessStage) {
        qCWarning(KWIN_CORE) << "Failed to create the color brightness pipeline stage";
    }
}

void ColorDevicePrivate::updateCalibrationToneCurves()
{
    calibrationStage.reset();

    if (profile.isNull()) {
        return;
    }

    cmsHPROFILE handle = cmsOpenProfileFromFile(profile.toUtf8(), "r");
    if (!handle) {
        qCWarning(KWIN_CORE) << "Failed to open color profile file:" << profile;
        return;
    }

    cmsToneCurve **vcgt = static_cast<cmsToneCurve **>(cmsReadTag(handle, cmsSigVcgtTag));
    if (!vcgt || !vcgt[0]) {
        qCWarning(KWIN_CORE) << "Profile" << profile << "has no VCGT tag";
    } else {
        // Need to duplicate the VCGT tone curves as they are owned by the profile.
        cmsToneCurve *toneCurves[] = {
            cmsDupToneCurve(vcgt[0]),
            cmsDupToneCurve(vcgt[1]),
            cmsDupToneCurve(vcgt[2]),
        };

        calibrationStage.reset(cmsStageAllocToneCurves(nullptr, 3, toneCurves));
        if (!calibrationStage) {
            qCWarning(KWIN_CORE) << "Failed to create the color calibration pipeline stage";
        }
    }

    cmsCloseProfile(handle);
}

ColorDevice::ColorDevice(AbstractOutput *output, QObject *parent)
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
    if (d->pipeline) {
        d->unlinkPipeline();
    }
}

AbstractOutput *ColorDevice::output() const
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

QString ColorDevice::profile() const
{
    return d->profile;
}

void ColorDevice::setProfile(const QString &profile)
{
    if (d->profile == profile) {
        return;
    }
    d->profile = profile;
    d->dirtyCurves |= ColorDevicePrivate::DirtyCalibrationToneCurve;
    scheduleUpdate();
    Q_EMIT profileChanged();
}

void ColorDevice::update()
{
    d->rebuildPipeline();

    GammaRamp gammaRamp(d->output->gammaRampSize());
    uint16_t *redChannel = gammaRamp.red();
    uint16_t *greenChannel = gammaRamp.green();
    uint16_t *blueChannel = gammaRamp.blue();

    for (uint32_t i = 0; i < gammaRamp.size(); ++i) {
        // ensure 64 bit calculation to prevent overflows
        const uint16_t index = (static_cast<uint64_t>(i) * 0xffff) / (gammaRamp.size() - 1);

        const uint16_t in[3] = { index, index, index };
        uint16_t out[3] = { 0 };
        cmsPipelineEval16(in, out, d->pipeline.data());

        redChannel[i] = out[0];
        greenChannel[i] = out[1];
        blueChannel[i] = out[2];
    }

    if (!d->output->setGammaRamp(gammaRamp)) {
        qCWarning(KWIN_CORE) << "Failed to update gamma ramp for output" << d->output;
    }
}

void ColorDevice::scheduleUpdate()
{
    d->updateTimer->start();
}

} // namespace KWin
