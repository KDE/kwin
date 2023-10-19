/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colormanager.h"
#include "colordevice.h"
#include "core/output.h"
#include "core/session.h"
#include "main.h"
#include "utils/common.h"
#include "workspace.h"

namespace KWin
{

class ColorManagerPrivate
{
public:
    QList<ColorDevice *> devices;
};

ColorManager::ColorManager()
    : d(std::make_unique<ColorManagerPrivate>())
{
    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        handleOutputAdded(output);
    }

    connect(workspace(), &Workspace::outputAdded, this, &ColorManager::handleOutputAdded);
    connect(workspace(), &Workspace::outputRemoved, this, &ColorManager::handleOutputRemoved);
    connect(kwinApp()->session(), &Session::activeChanged, this, &ColorManager::handleSessionActiveChanged);
}

ColorManager::~ColorManager() = default;

QList<ColorDevice *> ColorManager::devices() const
{
    return d->devices;
}

ColorDevice *ColorManager::findDevice(Output *output) const
{
    auto it = std::find_if(d->devices.begin(), d->devices.end(), [&output](ColorDevice *device) {
        return device->output() == output;
    });
    if (it != d->devices.end()) {
        return *it;
    }
    return nullptr;
}

void ColorManager::handleOutputAdded(Output *output)
{
    ColorDevice *device = new ColorDevice(output, this);
    d->devices.append(device);
    Q_EMIT deviceAdded(device);
}

void ColorManager::handleOutputRemoved(Output *output)
{
    auto it = std::find_if(d->devices.begin(), d->devices.end(), [&output](ColorDevice *device) {
        return device->output() == output;
    });
    if (it == d->devices.end()) {
        qCWarning(KWIN_CORE) << "Could not find any color device for output" << output;
        return;
    }
    ColorDevice *device = *it;
    d->devices.erase(it);
    Q_EMIT deviceRemoved(device);
    delete device;
}

void ColorManager::handleSessionActiveChanged(bool active)
{
    if (!active) {
        return;
    }
    for (ColorDevice *device : std::as_const(d->devices)) {
        device->scheduleUpdate();
    }
}

} // namespace KWin

#include "moc_colormanager.cpp"
