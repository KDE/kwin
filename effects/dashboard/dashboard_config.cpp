/*
 *   Copyright © 2010 Andreas Demmer <ademmer@opensuse.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "dashboard_config.h"
// KConfigSkeleton
#include "dashboardconfig.h"

#include <kwineffects_interface.h>
#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(DashboardEffectConfigFactory,
                           "dashboard_config.json",
                           registerPlugin<KWin::DashboardEffectConfig>();)

namespace KWin
{

DashboardEffectConfig::DashboardEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("dashboard")), parent, args)
{
    ui.setupUi(this);

    addConfig(DashboardConfig::self(), this);

    load();
}

DashboardEffectConfig::~DashboardEffectConfig()
{
}

void DashboardEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.kwin.Effects"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("kwin4_effect_dashboard"));
}

} // namespace KWin

#include "dashboard_config.moc"
