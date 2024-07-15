/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"

#include <QObject>
#include <memory>

namespace KWin
{

class Output;
class ColorDevicePrivate;

/**
 * The ColorDevice class represents a color managed device.
 */
class KWIN_EXPORT ColorDevice : public QObject
{
    Q_OBJECT

public:
    explicit ColorDevice(Output *output, QObject *parent = nullptr);
    ~ColorDevice() override;

    /**
     * Returns the underlying output for this color device.
     */
    Output *output() const;

    /**
     * Returns the current color temperature on this device, in Kelvins.
     */
    uint temperature() const;

    /**
     * Sets the color temperature on this device to @a temperature, in Kelvins.
     */
    void setTemperature(uint temperature);

public Q_SLOTS:
    void update();
    void scheduleUpdate();

Q_SIGNALS:
    /**
     * This signal is emitted when the brightness of this device has changed.
     */
    void brightnessChanged();
    /**
     * This signal is emitted when the color temperature of this device has changed.
     */
    void temperatureChanged();

private:
    std::unique_ptr<ColorDevicePrivate> d;
};

} // namespace KWin
