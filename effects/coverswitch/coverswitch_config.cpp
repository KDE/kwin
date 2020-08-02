/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "coverswitch_config.h"
// KConfigSkeleton
#include "coverswitchconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>
#include <KAboutData>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(CoverSwitchEffectConfigFactory,
                           "coverswitch_config.json",
                           registerPlugin<KWin::CoverSwitchEffectConfig>();)

namespace KWin
{

CoverSwitchEffectConfigForm::CoverSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

CoverSwitchEffectConfig::CoverSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("coverswitch")), parent, args)
{
    m_ui = new CoverSwitchEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    CoverSwitchConfig::instance(KWIN_CONFIG);
    addConfig(CoverSwitchConfig::self(), m_ui);
}

void CoverSwitchEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("coverswitch"));
}

} // namespace

#include "coverswitch_config.moc"
