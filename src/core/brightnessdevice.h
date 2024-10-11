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
    explicit BrightnessDevice();
    virtual ~BrightnessDevice();

    void setOutput(Output *output);
    virtual void setBrightness(double brightness) = 0;

    Output *output() const;
    virtual std::optional<double> observedBrightness() const = 0;
    virtual bool isInternal() const = 0;
    virtual QByteArray edidBeginning() const = 0;
    virtual int brightnessSteps() const = 0;

private:
    Output *m_output = nullptr;
};

}
