/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wobblywindows_config.h"
// KConfigSkeleton
#include "wobblywindowsconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <KLocalizedString>
#include <KAboutData>
#include <kconfiggroup.h>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(WobblyWindowsEffectConfigFactory,
                           "wobblywindows_config.json",
                           registerPlugin<KWin::WobblyWindowsEffectConfig>();)

namespace KWin
{

//-----------------------------------------------------------------------------
// WARNING: This is (kinda) copied from wobblywindows.cpp

struct ParameterSet {
    int stiffness;
    int drag;
    int move_factor;
};

static const ParameterSet set_0 = {
    15,
    80,
    10
};

static const ParameterSet set_1 = {
    10,
    85,
    10
};

static const ParameterSet set_2 = {
    6,
    90,
    10
};

static const ParameterSet set_3 = {
    3,
    92,
    20
};

static const ParameterSet set_4 = {
    1,
    97,
    25
};

ParameterSet pset[5] = { set_0, set_1, set_2, set_3, set_4 };

//-----------------------------------------------------------------------------

WobblyWindowsEffectConfig::WobblyWindowsEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("wobblywindows")), parent, args)
{
    WobblyWindowsConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(this);

    addConfig(WobblyWindowsConfig::self(), this);
    connect(m_ui.kcfg_WobblynessLevel, &QSlider::valueChanged, this, &WobblyWindowsEffectConfig::wobblinessChanged);

    load();
}

WobblyWindowsEffectConfig::~WobblyWindowsEffectConfig()
{
}

void WobblyWindowsEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("wobblywindows"));
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
