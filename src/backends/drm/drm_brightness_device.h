/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/brightnessdevice.h"

namespace KWin
{

class DrmPipeline;

class DrmBrightnessDevice : public BrightnessDevice
{
public:
    explicit DrmBrightnessDevice(DrmPipeline *pipeline);

    void setBrightness(double brightness) override;
    std::optional<double> observedBrightness() const override;
    bool isInternal() const override;
    QByteArray edidBeginning() const override;
    bool usesDdcCi() const override;
    int brightnessSteps() const override;

private:
    int minValue() const;

    DrmPipeline *const m_pipeline;
};

}
