/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *   Copyright (C) 2010 Alexandre Pereira <pereira.alex@gmail.com>
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

#include "glide_config.h"

#include <kwineffects.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

GlideEffectConfig::GlideEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(EffectFactory::componentData(), parent, args)
{
    ui.setupUi(this);
    connect(ui.slider, SIGNAL(valueChanged(int)), SLOT(valueChanged(int)));
    connect(ui.slider2, SIGNAL(valueChanged(int)), SLOT(valueChanged(int)));
    load();
}

GlideEffectConfig::~GlideEffectConfig()
{
}

void GlideEffectConfig::load()
{
    KCModule::load();
    KConfigGroup cg = EffectsHandler::effectConfig("Glide");
    ui.slider->setValue(cg.readEntry("GlideEffect", 0));
    ui.slider2->setValue(cg.readEntry("GlideAngle", -90));
    emit changed(false);
}

void GlideEffectConfig::save()
{
    KCModule::save();
    KConfigGroup cg = EffectsHandler::effectConfig("Glide");
    cg.writeEntry("GlideEffect", ui.slider->value());
    cg.writeEntry("GlideAngle", ui.slider2->value());
    cg.sync();
    emit changed(false);
    EffectsHandler::sendReloadMessage("glide");
}

void GlideEffectConfig::defaults()
{
    ui.slider->setValue(0);
    ui.slider2->setValue(-90);
    emit changed(true);
}

void GlideEffectConfig::valueChanged(int value)
{
    Q_UNUSED(value)
    emit changed(true);
}
} // namespace KWin
#include "glide_config.moc"
