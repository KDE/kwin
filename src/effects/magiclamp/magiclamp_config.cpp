/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "magiclamp_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "magiclampconfig.h"

#include <kwineffects_interface.h>

#include <KPluginFactory>
#include <kconfiggroup.h>

#include <QVBoxLayout>

K_PLUGIN_CLASS(KWin::MagicLampEffectConfig)

namespace KWin
{

MagicLampEffectConfigForm::MagicLampEffectConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

MagicLampEffectConfig::MagicLampEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_ui(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_ui);

    MagicLampConfig::instance(KWIN_CONFIG);
    addConfig(MagicLampConfig::self(), &m_ui);
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
