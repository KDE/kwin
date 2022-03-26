/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "blur_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "blurconfig.h"

#include <KPluginFactory>
#include <kwineffects_interface.h>

K_PLUGIN_CLASS(KWin::BlurEffectConfig)

namespace KWin
{

BlurEffectConfig::BlurEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    ui.setupUi(this);
    BlurConfig::instance(KWIN_CONFIG);
    addConfig(BlurConfig::self(), this);
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
