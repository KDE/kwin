/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <optional>

namespace KWin
{

class Output;

class BrightnessDevice
{
public:
    virtual ~BrightnessDevice() = default;

    virtual void setBrightness(double brightness) = 0;

    virtual std::optional<double> observedBrightness() const = 0;
    virtual bool isInternal() const = 0;
    virtual QByteArray edidBeginning() const = 0;
    virtual int brightnessSteps() const = 0;
};

}
