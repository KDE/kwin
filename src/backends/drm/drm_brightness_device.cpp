/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_brightness_device.h"
#include "drm_output.h"
#include "drm_pipeline.h"

namespace KWin
{

DrmBrightnessDevice::DrmBrightnessDevice(DrmPipeline *pipeline)
    : m_pipeline(pipeline)
{
}

void DrmBrightnessDevice::DrmBrightnessDevice::setBrightness(double brightness)
{
    m_pipeline->setLuminance(std::round(brightness * brightnessSteps()) + minValue());
}

std::optional<double> DrmBrightnessDevice::observedBrightness() const
{
    return (m_pipeline->connector()->luminance.value() - minValue()) / double(brightnessSteps());
}

bool DrmBrightnessDevice::isInternal() const
{
    return m_pipeline->connector()->isInternal();
}

QByteArray DrmBrightnessDevice::edidBeginning() const
{
    return m_pipeline->connector()->edid()->raw();
}

bool DrmBrightnessDevice::usesDdcCi() const
{
    return !isInternal();
}

int DrmBrightnessDevice::brightnessSteps() const
{
    return m_pipeline->connector()->luminance.maxValue() - minValue();
}

int DrmBrightnessDevice::minValue() const
{
    return isInternal() ? 1 : 0;
}

}
