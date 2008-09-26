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

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kaction.h>
#include <kconfiggroup.h>

#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

//-----------------------------------------------------------------------------
// WARNING: This is (kinda) copied from wobblywindows.cpp

struct ParameterSet
{
    int stiffness;
    int drag;
    int move_factor;
};

ParameterSet set_0 =
{
    15,
    80,
    10
};

ParameterSet set_1 =
{
    10,
    85,
    10
};

ParameterSet set_2 =
{
    6,
    90,
    10
};

ParameterSet set_3 =
{
    3,
    92,
    20
};

ParameterSet set_4 =
{
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

    connect(m_ui.wobblinessSlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui.wobblinessSlider, SIGNAL(valueChanged(int)), this, SLOT(wobblinessChanged()));
    connect(m_ui.moveBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui.resizeBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui.advancedBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui.advancedBox, SIGNAL(stateChanged(int)), this, SLOT(advancedChanged()));
    connect(m_ui.stiffnessSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui.dragSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui.moveFactorSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));

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

    unsigned int wobblynessLevel = 0;
    QString settingsMode = conf.readEntry("Settings", "Auto");
    if (settingsMode != "Custom")
    {
        wobblynessLevel = conf.readEntry("WobblynessLevel", 0);
        change = false;
    }
    if (wobblynessLevel > 4)
    {
        wobblynessLevel = 4;
        change = true;
    }
    m_ui.wobblinessSlider->setSliderPosition(wobblynessLevel);

    m_ui.moveBox->setChecked(conf.readEntry("MoveWobble", true));
    m_ui.resizeBox->setChecked(conf.readEntry("ResizeWobble", true));
    m_ui.advancedBox->setChecked(conf.readEntry("AdvancedMode", false));

    m_ui.stiffnessSlider->setValue(conf.readEntry("Stiffness", pset[0].stiffness));
    m_ui.dragSlider->setValue(conf.readEntry("Drag", pset[0].drag));
    m_ui.moveFactorSlider->setValue(conf.readEntry("MoveFactor", pset[0].move_factor));

    advancedChanged();

    emit changed(change);
}

void WobblyWindowsEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("Wobbly");
    conf.writeEntry("Settings", "Auto");

    conf.writeEntry("WobblynessLevel", m_ui.wobblinessSlider->value());

    conf.writeEntry("MoveWobble", m_ui.moveBox->isChecked());
    conf.writeEntry("ResizeWobble", m_ui.resizeBox->isChecked());
    conf.writeEntry("AdvancedMode", m_ui.advancedBox->isChecked());

    conf.writeEntry("Stiffness", m_ui.stiffnessSpin->value());
    conf.writeEntry("Drag", m_ui.dragSpin->value());
    conf.writeEntry("MoveFactor", m_ui.moveFactorSpin->value());

    emit changed(false);
    EffectsHandler::sendReloadMessage("wobblywindows");
}

void WobblyWindowsEffectConfig::defaults()
{
    m_ui.wobblinessSlider->setSliderPosition(0);

    m_ui.moveBox->setChecked(true);
    m_ui.resizeBox->setChecked(true);
    m_ui.advancedBox->setChecked(false);

    m_ui.stiffnessSlider->setValue(pset[0].stiffness);
    m_ui.dragSlider->setValue(pset[0].drag);
    m_ui.moveFactorSlider->setValue(pset[0].move_factor);

    emit changed(true);
}

void WobblyWindowsEffectConfig::advancedChanged()
    {
    if(m_ui.advancedBox->isChecked())
        m_ui.advancedGroup->setEnabled(true);
    else
        m_ui.advancedGroup->setEnabled(false);
    }

void WobblyWindowsEffectConfig::wobblinessChanged()
    {
    ParameterSet preset = pset[m_ui.wobblinessSlider->value()];

    m_ui.stiffnessSlider->setValue(preset.stiffness);
    m_ui.dragSlider->setValue(preset.drag);
    m_ui.moveFactorSlider->setValue(preset.move_factor);
    }

} // namespace

#include "wobblywindows_config.moc"
