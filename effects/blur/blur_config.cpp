/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blur_config.h"
// KConfigSkeleton
#include "blurconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>
#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(BlurEffectConfigFactory,
                           "blur_config.json",
                           registerPlugin<KWin::BlurEffectConfig>();)

namespace KWin
{

BlurEffectConfig::BlurEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("blur")), parent, args)
{
    ui.setupUi(this);
    BlurConfig::instance(KWIN_CONFIG);
    addConfig(BlurConfig::self(), this);

    load();
}

BlurEffectConfig::~BlurEffectConfig()
{
}

void BlurEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("blur"));
}

} // namespace KWin

#include "blur_config.moc"
