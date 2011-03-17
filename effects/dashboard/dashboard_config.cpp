/*
 *   Copyright © 2010 Andreas Demmer <ademmer@opensuse.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "dashboard_config.h"
#include <kwineffects.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

DashboardEffectConfig::DashboardEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(EffectFactory::componentData(), parent, args)
{
    ui.setupUi(this);

    connect(ui.brightness, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(ui.saturation, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(ui.duration, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(ui.blur, SIGNAL(stateChanged(int)), SLOT(changed()));

    load();
}

DashboardEffectConfig::~DashboardEffectConfig()
{
}

void DashboardEffectConfig::load()
{
    KCModule::load();
    KConfigGroup config = EffectsHandler::effectConfig("Dashboard");

    const int brightness = config.readEntry<int>("Brightness", 50);
    ui.brightness->setValue(brightness);

    const int saturation = config.readEntry<int>("Saturation", 50);
    ui.saturation->setValue(saturation);

    const int duration = config.readEntry("Duration", 500);
    ui.duration->setValue(duration);

    bool blur = config.readEntry("Blur", false);
    ui.blur->setChecked(blur);

    emit changed(false);
}

void DashboardEffectConfig::save()
{
    KCModule::save();

    KConfigGroup config = EffectsHandler::effectConfig("Dashboard");

    config.writeEntry("Brightness", ui.brightness->value());
    config.writeEntry("Saturation", ui.saturation->value());
    config.writeEntry("Duration", ui.duration->value());
    config.writeEntry("Blur", ui.blur->isChecked());

    config.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("dashboard");
}

void DashboardEffectConfig::defaults()
{
    ui.brightness->setValue(50);
    ui.saturation->setValue(50);
    ui.duration->setValue(500);
    ui.blur->setChecked(false);
    emit changed(true);
}

} // namespace KWin

