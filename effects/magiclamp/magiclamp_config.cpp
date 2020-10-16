/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "magiclamp_config.h"
// KConfigSkeleton
#include "magiclampconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>

#include <kconfiggroup.h>
#include <KAboutData>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(MagicLampEffectConfigFactory,
                           "magiclamp_config.json",
                           registerPlugin<KWin::MagicLampEffectConfig>();)

namespace KWin
{

MagicLampEffectConfigForm::MagicLampEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MagicLampEffectConfig::MagicLampEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("magiclamp")), parent, args)
{
    m_ui = new MagicLampEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    MagicLampConfig::instance(KWIN_CONFIG);
    addConfig(MagicLampConfig::self(), m_ui);

    load();
}

void MagicLampEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("magiclamp"));
}

} // namespace

#include "magiclamp_config.moc"
