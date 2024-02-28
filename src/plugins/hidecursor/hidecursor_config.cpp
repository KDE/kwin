/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "hidecursor_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "hidecursorconfig.h"

#include <KPluginFactory>
#include <kwineffects_interface.h>

K_PLUGIN_CLASS(KWin::HideCursorEffectConfig)

namespace KWin
{

HideCursorEffectConfig::HideCursorEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    HideCursorConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(widget());

    addConfig(HideCursorConfig::self(), widget());
}

void HideCursorEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("hidecursor"));
}

} // namespace

#include "hidecursor_config.moc"

#include "moc_hidecursor_config.cpp"
