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

OverviewEffectConfig::OverviewEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    ui.setupUi(widget());
    OverviewConfig::instance(KWIN_CONFIG);
    addConfig(OverviewConfig::self(), widget());

    auto actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("Overview"));
    actionCollection->setConfigGlobal(true);

    const QKeySequence defaultCycleShortcut = Qt::META | Qt::Key_Tab;
    QAction *cycleAction = actionCollection->addAction(QStringLiteral("Cycle Overview"));
    cycleAction->setText(i18nc("@action Overview and Grid View are the name of KWin effects", "Cycle through Overview and Grid View"));
    cycleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(cycleAction, {defaultCycleShortcut});
    KGlobalAccel::self()->setShortcut(cycleAction, {defaultCycleShortcut});

    const QKeySequence defaultUncycleShortcut = Qt::META | Qt::SHIFT | Qt::Key_Tab;
    QAction *reverseCycleAction = actionCollection->addAction(QStringLiteral("Cycle Overview Opposite"));
    reverseCycleAction->setText(i18nc("@action Grid View and Overview are the name of KWin effects", "Cycle through Grid View and Overview"));
    reverseCycleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(reverseCycleAction, {defaultUncycleShortcut});
    KGlobalAccel::self()->setShortcut(reverseCycleAction, {defaultUncycleShortcut});

    const QKeySequence defaultOverviewShortcut = Qt::META | Qt::Key_W;
    QAction *overviewAction = actionCollection->addAction(QStringLiteral("Overview"));
    overviewAction->setText(i18nc("@action Overview is the name of a KWin effect", "Toggle Overview"));
    overviewAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(overviewAction, {defaultOverviewShortcut});
    KGlobalAccel::self()->setShortcut(overviewAction, {defaultOverviewShortcut});

    const QKeySequence defaultGridShortcut = Qt::META | Qt::Key_G;
    QAction *gridAction = actionCollection->addAction(QStringLiteral("Grid View"));
    gridAction->setText(i18nc("@action Grid View is the name of a KWin effect", "Toggle Grid View"));
    gridAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(gridAction, {defaultGridShortcut});
    KGlobalAccel::self()->setShortcut(gridAction, {defaultGridShortcut});

    ui.shortcutsEditor->addCollection(actionCollection);
    connect(ui.shortcutsEditor, &KShortcutsEditor::keyChange, this, &KCModule::markAsChanged);
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

#include "moc_overvieweffectkcm.cpp"
