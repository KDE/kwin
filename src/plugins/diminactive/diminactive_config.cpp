/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "diminactive_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "diminactiveconfig.h"

#include <kwineffects_interface.h>

#include <KPluginFactory>

K_PLUGIN_CLASS(KWin::DimInactiveEffectConfig)

namespace KWin
{

DimInactiveEffectConfig::DimInactiveEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    DimInactiveConfig::instance(KWIN_CONFIG);
    addConfig(DimInactiveConfig::self(), widget());
}

DimInactiveEffectConfig::~DimInactiveEffectConfig()
{
}

void DimInactiveEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("diminactive"));
}

} // namespace KWin

#include "diminactive_config.moc"

#include "moc_diminactive_config.cpp"
