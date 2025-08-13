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

class LogicalOutput;
class BackendOutput;
class ColorDevice;
class ColorManagerPrivate;

/**
 * The ColorManager class is the entry point into color management facilities.
 */
class KWIN_EXPORT ColorManager : public QObject
{
    Q_OBJECT

public:
    ColorManager();
    ~ColorManager() override;

    /**
     * Returns the color device for the specified @a output, or @c null if there is no
     * any device.
     */
    ColorDevice *findDevice(BackendOutput *output) const;

    /**
     * Returns the list of all available color devices.
     */
    QList<ColorDevice *> devices() const;

Q_SIGNALS:
    /**
     * This signal is emitted when a new color device @a device has been added.
     */
    void deviceAdded(ColorDevice *device);
    /**
     * This signal is emitted when a color device has been removed. @a device indicates
     * what color device was removed.
     */
    void deviceRemoved(ColorDevice *device);

private Q_SLOTS:
    void handleOutputAdded(LogicalOutput *output);
    void handleOutputRemoved(LogicalOutput *output);
    void handleSessionActiveChanged(bool active);

private:
    std::unique_ptr<ColorManagerPrivate> d;
};

} // namespace KWin
