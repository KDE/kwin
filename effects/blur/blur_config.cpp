/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
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

#include "blur_config.h"

#include <kwineffects.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

BlurEffectConfig::BlurEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(EffectFactory::componentData(), parent, args)
{
    ui.setupUi(this);
    connect(ui.slider, SIGNAL(valueChanged(int)), SLOT(valueChanged(int)));
    connect(ui.checkBox, SIGNAL(stateChanged(int)), SLOT(valueChanged(int)));

    load();
}

BlurEffectConfig::~BlurEffectConfig()
{
}

void BlurEffectConfig::load()
{
    KCModule::load();
    KConfigGroup cg = EffectsHandler::effectConfig("Blur");
    ui.slider->setValue(cg.readEntry("BlurRadius", 12) / 2);
    ui.checkBox->setChecked(cg.readEntry("CacheTexture", true));
    emit changed(false);
}

void BlurEffectConfig::save()
{
    KCModule::save();

    KConfigGroup cg = EffectsHandler::effectConfig("Blur");
    cg.writeEntry("BlurRadius", ui.slider->value() * 2);
    cg.writeEntry("CacheTexture", ui.checkBox->isChecked());
    cg.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("blur");
}

void BlurEffectConfig::defaults()
{
    emit changed(true);
}

void BlurEffectConfig::valueChanged(int value)
{
    Q_UNUSED(value)
    emit changed(true);
}

} // namespace KWin

#include "blur_config.moc"

