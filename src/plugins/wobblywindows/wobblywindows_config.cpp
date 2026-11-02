/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wobblywindows_config.h"

#include "config-kwin.h"

// KConfigSkeleton
#include "wobblywindowsconfig.h"
#include <kwineffects_interface.h>

#include <KLocalizedString>
#include <KPluginFactory>
#include <kconfiggroup.h>

K_PLUGIN_CLASS(KWin::WobblyWindowsEffectConfig)

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

static const ParameterSet cloth = {
    16,
    50,
    25};

static const ParameterSet gelatin = {
    20,
    90,
    25};

static const ParameterSet water = {
    2,
    89,
    18};

static const ParameterSet pudding = {
    10,
    98,
    8};

static const ParameterSet molasses = {
    4,
    71,
    13};

static const ParameterSet nitroglycerin = {
    44,
    96,
    18};

std::array pset = {cloth, gelatin, water, pudding, molasses, nitroglycerin};

//-----------------------------------------------------------------------------

WobblyWindowsEffectConfig::WobblyWindowsEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    WobblyWindowsConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(widget());

    addConfig(WobblyWindowsConfig::self(), widget());
    connect(m_ui.kcfg_WobblynessLevel, &QComboBox::currentIndexChanged, this, &WobblyWindowsEffectConfig::wobblinessChanged);
    connect(m_ui.kcfg_Stiffness, &QSlider::valueChanged, this, [this]() {
        m_ui.kcfg_WobblynessLevel->setCurrentIndex(pset.size());
    });
    connect(m_ui.kcfg_Drag, &QSlider::valueChanged, this, [this]() {
        m_ui.kcfg_WobblynessLevel->setCurrentIndex(pset.size());
    });
    connect(m_ui.kcfg_MoveFactor, &QSlider::valueChanged, this, [this]() {
        m_ui.kcfg_WobblynessLevel->setCurrentIndex(pset.size());
    });
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
    const int index = m_ui.kcfg_WobblynessLevel->currentIndex();
    if (index < 0 || index >= static_cast<int>(pset.size())) {
        return;
    }
    ParameterSet preset = pset[index];

    m_ui.kcfg_Stiffness->setValue(preset.stiffness);
    m_ui.kcfg_Drag->setValue(preset.drag);
    m_ui.kcfg_MoveFactor->setValue(preset.move_factor);

    // Changing the values causes the selection to go to Custom. Re-setting it here to what was selected.
    m_ui.kcfg_WobblynessLevel->setCurrentIndex(index);
}

} // namespace

#include "wobblywindows_config.moc"

#include "moc_wobblywindows_config.cpp"
