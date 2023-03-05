/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputconfigurationstore.h"
#include "core/inputdevice.h"
#include "core/output.h"
#include "core/outputconfiguration.h"
#include "input.h"
#include "kscreenintegration.h"

namespace KWin
{

std::pair<OutputConfiguration, QVector<Output *>> OutputConfigurationStore::queryConfig(const QVector<Output *> &outputs)
{
    const auto kscreenConfig = KScreenIntegration::readOutputConfig(outputs, KScreenIntegration::connectedOutputsHash(outputs));
    if (kscreenConfig) {
        return kscreenConfig.value();
    } else {
        // no config file atm -> generate a new one
        return generateConfig(outputs);
    }
}

std::pair<OutputConfiguration, QVector<Output *>> OutputConfigurationStore::generateConfig(const QVector<Output *> &outputs) const
{
    OutputConfiguration ret;

    QPoint pos(0, 0);
    for (const auto output : outputs) {
        const auto mode = chooseMode(output);
        const double scale = chooseScale(output, mode.get());
        *ret.changeSet(output) = {
            .mode = mode,
            .enabled = true,
            .pos = pos,
            .scale = scale,
            .transform = output->panelOrientation(),
            .overscan = 0,
            .rgbRange = Output::RgbRange::Automatic,
            .vrrPolicy = RenderLoop::VrrPolicy::Automatic,
        };
        pos.setX(pos.x() + mode->size().width() / scale);
    }
    return std::make_pair(ret, outputs);
}

std::shared_ptr<OutputMode> OutputConfigurationStore::chooseMode(Output *output) const
{
    const auto modes = output->modes();

    // some displays advertise bigger modes than their native resolution
    // to avoid that, take the preferred mode into account, which is usually the native one
    const auto preferred = std::find_if(modes.begin(), modes.end(), [](const auto &mode) {
        return mode->flags() & OutputMode::Flag::Preferred;
    });
    if (preferred != modes.end()) {
        // some high refresh rate displays advertise a 60Hz mode as preferred for compatibility reasons
        // ignore that and choose the highest possible refresh rate by default instead
        std::shared_ptr<OutputMode> highestRefresh = *preferred;
        for (const auto &mode : modes) {
            if (mode->size() == highestRefresh->size() && mode->refreshRate() > highestRefresh->refreshRate()) {
                highestRefresh = mode;
            }
        }
        // if the preferred mode size has a refresh rate that's too low for PCs,
        // allow falling back to a mode with lower resolution and a more usable refresh rate
        if (highestRefresh->refreshRate() >= 50000) {
            return highestRefresh;
        }
    }

    std::shared_ptr<OutputMode> ret;
    for (auto mode : modes) {
        if (!ret) {
            ret = mode;
            continue;
        }
        const bool retUsableRefreshRate = ret->refreshRate() >= 50000;
        const bool usableRefreshRate = mode->refreshRate() >= 50000;
        if (retUsableRefreshRate && !usableRefreshRate) {
            ret = mode;
            continue;
        }
        if ((usableRefreshRate && !retUsableRefreshRate)
            || mode->size().width() > ret->size().width()
            || mode->size().height() > ret->size().height()
            || (mode->size() == ret->size() && mode->refreshRate() > ret->refreshRate())) {
            ret = mode;
        }
    }
    return ret;
}

double OutputConfigurationStore::chooseScale(Output *output, OutputMode *mode) const
{
    if (output->physicalSize().height() <= 0) {
        // invalid size, can't do anything with this
        return 1.0;
    }
    const double outputDpi = mode->size().height() / (output->physicalSize().height() / 25.4);
    const double desiredScale = outputDpi / targetDpi(output);
    // round to 25% steps
    return std::round(100.0 * desiredScale / 25.0) * 100.0 / 25.0;
}

double OutputConfigurationStore::targetDpi(Output *output) const
{
    const auto devices = input()->devices();
    const bool hasLaptopLid = std::any_of(devices.begin(), devices.end(), [](const auto &device) {
        return device->isLidSwitch();
    });
    if (output->isInternal()) {
        if (hasLaptopLid) {
            // laptop screens: usually closer to the face than desktop monitors
            return 125;
        } else {
            // phone screens: even closer than laptops
            return 136;
        }
    } else {
        // "normal" 1x scale desktop monitor dpi
        return 96;
    }
}

}
