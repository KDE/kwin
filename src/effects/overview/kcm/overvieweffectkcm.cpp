/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "overvieweffectkcm.h"

#include <config-kwin.h>

#include "overviewconfig.h"

#include <kwineffects_interface.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>

K_PLUGIN_CLASS(KWin::OverviewEffectConfig)

namespace KWin
{

OverviewEffectConfig::OverviewEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    ui.setupUi(this);
    OverviewConfig::instance(KWIN_CONFIG);
    addConfig(OverviewConfig::self(), this);

    auto actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("Overview"));
    actionCollection->setConfigGlobal(true);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_W;
    QAction *toggleAction = actionCollection->addAction(QStringLiteral("Overview"));
    toggleAction->setText(i18n("Toggle Overview"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(toggleAction, {defaultToggleShortcut});

    ui.shortcutsEditor->addCollection(actionCollection);
    connect(ui.shortcutsEditor, &KShortcutsEditor::keyChange, this, &OverviewEffectConfig::markAsChanged);
}

OverviewEffectConfig::~OverviewEffectConfig()
{
    // If save() is called, undo() has no effect.
    ui.shortcutsEditor->undo();
}

void OverviewEffectConfig::save()
{
    KCModule::save();
    ui.shortcutsEditor->save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("overview"));
}

void OverviewEffectConfig::defaults()
{
    ui.shortcutsEditor->allDefault();
    KCModule::defaults();
}

} // namespace KWin

#include "overvieweffectkcm.moc"
