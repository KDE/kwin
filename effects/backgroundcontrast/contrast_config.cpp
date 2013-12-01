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

#include "contrast_config.h"
// KConfigSkeleton
#include "contrastconfig.h"

#include <kwineffects.h>
#include <KDE/KAboutData>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

ContrastEffectConfig::ContrastEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("backgroundcontrast")), parent, args)
{
    ui.setupUi(this);
    addConfig(ContrastConfig::self(), this);

    load();
}

ContrastEffectConfig::~ContrastEffectConfig()
{
}

void ContrastEffectConfig::save()
{
    KCModule::save();

    EffectsHandler::sendReloadMessage(QStringLiteral("contrast"));
}

} // namespace KWin

#include "moc_contrast_config.cpp"
