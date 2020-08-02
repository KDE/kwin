/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017, 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slide_config.h"
// KConfigSkeleton
#include "slideconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>

#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(SlideEffectConfigFactory,
                           "slide_config.json",
                           registerPlugin<KWin::SlideEffectConfig>();)

namespace KWin
{

SlideEffectConfig::SlideEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("slide")), parent, args)
{
    m_ui.setupUi(this);
    SlideConfig::instance(KWIN_CONFIG);
    addConfig(SlideConfig::self(), this);
    load();
}

SlideEffectConfig::~SlideEffectConfig()
{
}

void SlideEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("slide"));
}

} // namespace KWin

#include "slide_config.moc"
