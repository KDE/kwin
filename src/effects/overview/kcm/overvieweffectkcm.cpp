/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffectkcm.h"
#include "overviewconfig.h"

#include <config-kwin.h>
#include <kwineffects_interface.h>
#include <QAction>

#include <kconfiggroup.h>
#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(OverviewEffectConfigFactory,
                           "overvieweffectkcm.json",
                           registerPlugin<KWin::OverviewEffectConfig>();)

namespace KWin
{

OverviewEffectConfig::OverviewEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    ui.setupUi(this);
    OverviewConfig::instance(KWIN_CONFIG);
    addConfig(OverviewConfig::self(), this);
    load();
}

void OverviewEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("overview"));
}

} // namespace KWin

#include "overvieweffectkcm.moc"
