/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colormanager.h"
#include "abstract_output.h"
#include "colordevice.h"
#include "logind.h"
#include "main.h"
#include "platform.h"
#include "utils.h"

namespace KWin
{

KWIN_SINGLETON_FACTORY(ColorManager)

class ColorManagerPrivate
{
public:
    QVector<ColorDevice *> devices;
};

ColorManager::ColorManager(QObject *parent)
    : QObject(parent)
    , d(new ColorManagerPrivate)
{
    const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
    for (AbstractOutput *output : outputs) {
        handleOutputEnabled(output);
    }

    connect(kwinApp()->platform(), &Platform::outputEnabled,
            this, &ColorManager::handleOutputEnabled);
    connect(kwinApp()->platform(), &Platform::outputDisabled,
            this, &ColorManager::handleOutputDisabled);
    connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged,
            this, &ColorManager::handleSessionActiveChanged);
}

ColorManager::~ColorManager()
{
    s_self = nullptr;
}

QVector<ColorDevice *> ColorManager::devices() const
{
    return d->devices;
}

ColorDevice *ColorManager::findDevice(AbstractOutput *output) const
{
    auto it = std::find_if(d->devices.begin(), d->devices.end(), [&output](ColorDevice *device) {
        return device->output() == output;
    });
    if (it != d->devices.end()) {
        return *it;
    }
    return nullptr;
}

void ColorManager::handleOutputEnabled(AbstractOutput *output)
{
    ColorDevice *device = new ColorDevice(output, this);
    d->devices.append(device);
    emit deviceAdded(device);
}

void ColorManager::handleOutputDisabled(AbstractOutput *output)
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
    emit deviceRemoved(device);
    delete device;
}

void ColorManager::handleSessionActiveChanged(bool active)
{
    if (!active) {
        return;
    }
    for (ColorDevice *device : qAsConst(d->devices)) {
        device->scheduleUpdate();
    }
}

} // namespace KWin
