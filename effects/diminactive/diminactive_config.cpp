/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "diminactive_config.h"

// KConfigSkeleton
#include "diminactiveconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>

#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(DimInactiveEffectConfigFactory,
                           "diminactive_config.json",
                           registerPlugin<KWin::DimInactiveEffectConfig>();)

namespace KWin
{

DimInactiveEffectConfig::DimInactiveEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("diminactive")), parent, args)
{
    m_ui.setupUi(this);
    DimInactiveConfig::instance(KWIN_CONFIG);
    addConfig(DimInactiveConfig::self(), this);
    load();
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
