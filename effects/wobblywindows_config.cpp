/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Cédric Borgese <cedric.borgese@gmail.com>

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

#include "wobblywindows_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KGlobalAccel>
#include <kconfiggroup.h>

#include <QGridLayout>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

WobblyWindowsEffectConfig::WobblyWindowsEffectConfig(QWidget* parent, const QVariantList& args) :
KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui.setupUi(this);

    connect(m_ui.slWobblyness, SIGNAL(valueChanged(int)), this, SLOT(slotSlWobblyness(int)));

    load();
}

WobblyWindowsEffectConfig::~WobblyWindowsEffectConfig()
{
}

void WobblyWindowsEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Wobbly");
    bool change = true;

    unsigned int wobblynessLevel = 1;
    QString settingsMode = conf.readEntry("Settings", "Auto");
    if (settingsMode != "Custom")
    {
        wobblynessLevel = conf.readEntry("WobblynessLevel", 1);
        change = false;
    }

    if (wobblynessLevel > 4)
    {
        wobblynessLevel = 4;
        change = true;
    }

    m_ui.slWobblyness->setSliderPosition(wobblynessLevel);

    emit changed(change);
}

void WobblyWindowsEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("Wobbly");

    conf.writeEntry("Settings", "Auto");
    conf.writeEntry("WobblynessLevel", m_ui.slWobblyness->value());

    emit changed(false);
    EffectsHandler::sendReloadMessage("wobblywindows");
}

void WobblyWindowsEffectConfig::defaults()
{
    m_ui.slWobblyness->setSliderPosition(2);

    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlWobblyness(int)
{
    emit changed(true);
}

} // namespace

#include "wobblywindows_config.moc"
