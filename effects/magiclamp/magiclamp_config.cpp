/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "magiclamp_config.h"
// KConfigSkeleton
#include "magiclampconfig.h"

#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

MagicLampEffectConfigForm::MagicLampEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MagicLampEffectConfig::MagicLampEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new MagicLampEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    addConfig(MagicLampConfig::self(), m_ui);

    load();
}

void MagicLampEffectConfig::save()
{
    KCModule::save();
    EffectsHandler::sendReloadMessage("magiclamp");
}

} // namespace

#include "magiclamp_config.moc"
