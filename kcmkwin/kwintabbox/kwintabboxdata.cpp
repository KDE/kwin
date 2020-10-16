/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwintabboxdata.h"

#include "kwintabboxsettings.h"
#include "kwinswitcheffectsettings.h"
#include "kwinpluginssettings.h"

namespace KWin
{
namespace TabBox
{

KWinTabboxData::KWinTabboxData(QObject *parent, const QVariantList &args)
    : KCModuleData(parent, args)
    , m_tabBoxConfig(new TabBoxSettings(QStringLiteral("TabBox"), this))
    , m_tabBoxAlternativeConfig(new TabBoxSettings(QStringLiteral("TabBoxAlternative"), this))
    , m_coverSwitchConfig(new SwitchEffectSettings(QStringLiteral("Effect-CoverSwitch"), this))
    , m_flipSwitchConfig(new SwitchEffectSettings(QStringLiteral("Effect-FlipSwitch"), this))
    , m_pluginsConfig(new PluginsSettings(this))
{
    autoRegisterSkeletons();
}

TabBoxSettings *KWinTabboxData::tabBoxConfig() const
{
    return m_tabBoxConfig;
}

TabBoxSettings *KWinTabboxData::tabBoxAlternativeConfig() const
{
    return m_tabBoxAlternativeConfig;
}

SwitchEffectSettings *KWinTabboxData::coverSwitchConfig() const
{
    return m_coverSwitchConfig;
}

SwitchEffectSettings *KWinTabboxData::flipSwitchConfig() const
{
    return m_flipSwitchConfig;
}

PluginsSettings *KWinTabboxData::pluginsConfig() const
{
    return m_pluginsConfig;
}

}

}
