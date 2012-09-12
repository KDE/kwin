/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Cédric Borgese <cedric.borgese@gmail.com>
 Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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
// KConfigSkeleton
#include "wobblywindowsconfig.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kaction.h>
#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

//-----------------------------------------------------------------------------
// WARNING: This is (kinda) copied from wobblywindows.cpp

struct ParameterSet {
    int stiffness;
    int drag;
    int move_factor;
};

ParameterSet set_0 = {
    15,
    80,
    10
};

ParameterSet set_1 = {
    10,
    85,
    10
};

ParameterSet set_2 = {
    6,
    90,
    10
};

ParameterSet set_3 = {
    3,
    92,
    20
};

ParameterSet set_4 = {
    1,
    97,
    25
};

ParameterSet pset[5] = { set_0, set_1, set_2, set_3, set_4 };

//-----------------------------------------------------------------------------

WobblyWindowsEffectConfig::WobblyWindowsEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui.setupUi(this);

    addConfig(WobblyWindowsConfig::self(), this);
    connect(m_ui.kcfg_WobblynessLevel, SIGNAL(valueChanged(int)), this, SLOT(wobblinessChanged()));

    load();
}

WobblyWindowsEffectConfig::~WobblyWindowsEffectConfig()
{
}

void WobblyWindowsEffectConfig::save()
{
    KCModule::save();

    EffectsHandler::sendReloadMessage("wobblywindows");
}

void WobblyWindowsEffectConfig::wobblinessChanged()
{
    ParameterSet preset = pset[m_ui.kcfg_WobblynessLevel->value()];

    m_ui.kcfg_Stiffness->setValue(preset.stiffness);
    m_ui.kcfg_Drag->setValue(preset.drag);
    m_ui.kcfg_MoveFactor->setValue(preset.move_factor);
}

} // namespace

#include "wobblywindows_config.moc"
