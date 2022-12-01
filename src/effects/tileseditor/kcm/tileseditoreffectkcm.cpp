/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tileseditoreffectkcm.h"

#include <config-kwin.h>

#include <kwineffects_interface.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>

K_PLUGIN_CLASS(KWin::TilesEditorEffectConfig)

namespace KWin
{

TilesEditorEffectConfig::TilesEditorEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    ui.setupUi(this);

    auto actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("tileseditor"));
    actionCollection->setConfigGlobal(true);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_T;
    QAction *toggleAction = actionCollection->addAction(QStringLiteral("Edit Tiles"));
    toggleAction->setText(i18n("Toggle Tiles Editor"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(toggleAction, {defaultToggleShortcut});

    ui.shortcutsEditor->addCollection(actionCollection);
    connect(ui.shortcutsEditor, &KShortcutsEditor::keyChange, this, &TilesEditorEffectConfig::markAsChanged);
}

TilesEditorEffectConfig::~TilesEditorEffectConfig()
{
    // If save() is called, undo() has no effect.
    ui.shortcutsEditor->undo();
}

void TilesEditorEffectConfig::save()
{
    KCModule::save();
    ui.shortcutsEditor->save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("tileseditor"));
}

void TilesEditorEffectConfig::defaults()
{
    ui.shortcutsEditor->allDefault();
    KCModule::defaults();
}

} // namespace KWin

#include "tileseditoreffectkcm.moc"
