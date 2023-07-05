/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "invert_config.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KShortcutsEditor>

#include <QVBoxLayout>

K_PLUGIN_CLASS(KWin::InvertEffectConfig)

namespace KWin
{

InvertEffectConfig::InvertEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    QVBoxLayout *layout = new QVBoxLayout(widget());

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(widget(), QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction *a = actionCollection->addAction(QStringLiteral("Invert"));
    a->setText(i18n("Toggle Invert Effect"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_I));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_I));

    QAction *b = actionCollection->addAction(QStringLiteral("InvertWindow"));
    b->setText(i18n("Toggle Invert Effect on Window"));
    b->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(b, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_U));
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_U));

    mShortcutEditor = new KShortcutsEditor(actionCollection, widget(),
                                           KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, &KShortcutsEditor::keyChange, this, &KCModule::markAsChanged);
    layout->addWidget(mShortcutEditor);
}

InvertEffectConfig::~InvertEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undo();
}

void InvertEffectConfig::load()
{
    KCModule::load();

    setNeedsSave(false);
}

void InvertEffectConfig::save()
{
    KCModule::save();

    mShortcutEditor->save(); // undo() will restore to this state from now on

    setNeedsSave(false);
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("invert"));
}

void InvertEffectConfig::defaults()
{
    mShortcutEditor->allDefault();

    setNeedsSave(true);
}

} // namespace

#include "invert_config.moc"

#include "moc_invert_config.cpp"
