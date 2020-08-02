/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cubeslide_config.h"
// KConfigSkeleton
#include "cubeslideconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <kconfiggroup.h>
#include <KAboutData>
#include <KPluginFactory>
#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(CubeSlideEffectConfigFactory,
                           "cubeslide_config.json",
                           registerPlugin<KWin::CubeSlideEffectConfig>();)

namespace KWin
{

CubeSlideEffectConfigForm::CubeSlideEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

CubeSlideEffectConfig::CubeSlideEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("cubeslide")), parent, args)
{
    m_ui = new CubeSlideEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    CubeSlideConfig::instance(KWIN_CONFIG);
    addConfig(CubeSlideConfig::self(), m_ui);

    load();
}

void CubeSlideEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("cubeslide"));
}

} // namespace

#include "cubeslide_config.moc"
