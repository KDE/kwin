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
// KConfigSkeleton
#include "glideconfig.h"

#include <kwineffects.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

GlideEffectConfig::GlideEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(EffectFactory::componentData(), parent, args)
{
    ui.setupUi(this);
    addConfig(GlideConfig::self(), this);
    load();
}

GlideEffectConfig::~GlideEffectConfig()
{
}

void GlideEffectConfig::save()
{
    KCModule::save();
    EffectsHandler::sendReloadMessage("glide");
}
} // namespace KWin
#include "glide_config.moc"
