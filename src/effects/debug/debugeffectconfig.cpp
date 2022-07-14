/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "debugeffectconfig.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KShortcutsEditor>

#include <QVBoxLayout>

K_PLUGIN_CLASS(KWin::DebugEffectConfig)

namespace KWin
{

DebugEffectConfig::DebugEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction *a = actionCollection->addAction(QStringLiteral("Debug"));
    a->setText(i18n("Toggle Debugging Fractional Scaling"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_F));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_F));

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
                                           KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, &KShortcutsEditor::keyChange, this, &DebugEffectConfig::markAsChanged);
    layout->addWidget(mShortcutEditor);
}

DebugEffectConfig::~DebugEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undo();
}

void DebugEffectConfig::load()
{
    KCModule::load();

    Q_EMIT changed(false);
}

void DebugEffectConfig::save()
{
    KCModule::save();

    mShortcutEditor->save(); // undo() will restore to this state from now on

    Q_EMIT changed(false);
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("invert"));
}

void DebugEffectConfig::defaults()
{
    mShortcutEditor->allDefault();

    Q_EMIT changed(true);
}

} // namespace

#include "debugeffectconfig.moc"
