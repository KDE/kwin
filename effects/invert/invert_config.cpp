/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "invert_config.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KGlobalAccel>
#include <KLocalizedString>
#include <KActionCollection>
#include <KShortcutsEditor>
#include <KAboutData>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(InvertEffectConfigFactory,
                           "invert_config.json",
                           registerPlugin<KWin::InvertEffectConfig>();)

namespace KWin
{

InvertEffectConfig::InvertEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("invert")), parent, args)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction* a = actionCollection->addAction(QStringLiteral("Invert"));
    a->setText(i18n("Toggle Invert Effect"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);

    QAction* b = actionCollection->addAction(QStringLiteral("InvertWindow"));
    b->setText(i18n("Toggle Invert Effect on Window"));
    b->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
                                           KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, &KShortcutsEditor::keyChange, this, qOverload<>(&InvertEffectConfig::changed));
    layout->addWidget(mShortcutEditor);

    load();
}

InvertEffectConfig::~InvertEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undoChanges();
}

void InvertEffectConfig::load()
{
    KCModule::load();

    emit changed(false);
}

void InvertEffectConfig::save()
{
    KCModule::save();

    mShortcutEditor->save();    // undo() will restore to this state from now on

    emit changed(false);
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("invert"));
}

void InvertEffectConfig::defaults()
{
    mShortcutEditor->allDefault();

    emit changed(true);
}


} // namespace

#include "invert_config.moc"
