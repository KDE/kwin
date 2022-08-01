/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglobals.h>

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
     * Returns the current color brightness on this device, in percent.
     */
    uint brightness() const;

    /**
     * Sets the color brightness on this device to @a brightness, in percent.
     */
    void setBrightness(uint brightness);

    /**
     * Returns the current color temperature on this device, in Kelvins.
     */
    uint temperature() const;

    /**
     * Sets the color temperature on this device to @a temperature, in Kelvins.
     */
    void setTemperature(uint temperature);

    /**
     * Returns the color profile for this device.
     */
    QString profile() const;

    /**
     * Sets the color profile for this device to @a profile.
     */
    void setProfile(const QString &profile);

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
    /**
     * This signal is emitted when the color profile of this device has changed.
     */
    void profileChanged();

private:
    std::unique_ptr<ColorDevicePrivate> d;
};

} // namespace KWin
