/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008, 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "flipswitch_config.h"
// KConfigSkeleton
#include "flipswitchconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <kconfiggroup.h>
#include <KActionCollection>
#include <KAboutData>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(FlipSwitchEffectConfigFactory,
                           "flipswitch_config.json",
                           registerPlugin<KWin::FlipSwitchEffectConfig>();)

namespace KWin
{

FlipSwitchEffectConfigForm::FlipSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

FlipSwitchEffectConfig::FlipSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("flipswitch")), parent, args)
{
    m_ui = new FlipSwitchEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    QAction* a = m_actionCollection->addAction(QStringLiteral("FlipSwitchCurrent"));
    a->setText(i18n("Toggle Flip Switch (Current desktop)"));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    QAction* b = m_actionCollection->addAction(QStringLiteral("FlipSwitchAll"));
    b->setText(i18n("Toggle Flip Switch (All desktops)"));
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>());

    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("FlipSwitch"));
    m_actionCollection->setConfigGlobal(true);

    m_ui->shortcutEditor->addCollection(m_actionCollection);

    FlipSwitchConfig::instance(KWIN_CONFIG);
    addConfig(FlipSwitchConfig::self(), m_ui);

    load();
}

FlipSwitchEffectConfig::~FlipSwitchEffectConfig()
{
}

void FlipSwitchEffectConfig::save()
{
    KCModule::save();
    m_ui->shortcutEditor->save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("flipswitch"));
}


} // namespace

#include "flipswitch_config.moc"
