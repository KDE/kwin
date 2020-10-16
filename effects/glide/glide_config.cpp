/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2010 Alexandre Pereira <pereira.alex@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "glide_config.h"
// KConfigSkeleton
#include "glideconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>
#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(GlideEffectConfigFactory,
                           "glide_config.json",
                           registerPlugin<KWin::GlideEffectConfig>();)

namespace KWin
{

GlideEffectConfig::GlideEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("glide")), parent, args)
{
    ui.setupUi(this);
    GlideConfig::instance(KWIN_CONFIG);
    addConfig(GlideConfig::self(), this);
    load();
}

GlideEffectConfig::~GlideEffectConfig()
{
}

void GlideEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("glide"));
}

} // namespace KWin

#include "glide_config.moc"
