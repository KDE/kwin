/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwintabboxdata.h"

#include "kwinpluginssettings.h"
#include "kwinswitcheffectsettings.h"
#include "kwintabboxsettings.h"

namespace KWin
{
namespace TabBox
{

KWinTabboxData::KWinTabboxData(QObject *parent, const QVariantList &args)
    : KCModuleData(parent, args)
    , m_tabBoxConfig(new TabBoxSettings(QStringLiteral("TabBox"), this))
    , m_tabBoxAlternativeConfig(new TabBoxSettings(QStringLiteral("TabBoxAlternative"), this))
    , m_pluginsConfig(new PluginsSettings(this))
{
    registerSkeleton(m_tabBoxConfig);
    registerSkeleton(m_tabBoxAlternativeConfig);
}

TabBoxSettings *KWinTabboxData::tabBoxConfig() const
{
    return m_tabBoxConfig;
}

TabBoxSettings *KWinTabboxData::tabBoxAlternativeConfig() const
{
    return m_tabBoxAlternativeConfig;
}

PluginsSettings *KWinTabboxData::pluginsConfig() const
{
    return m_pluginsConfig;
}

}

}
