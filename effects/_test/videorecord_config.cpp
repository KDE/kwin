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

#include "videorecord_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KUrlRequester>
#include <QLabel>
#include <KGlobalSettings>
#include <KConfigGroup>

#include <QVBoxLayout>
#include <QHBoxLayout>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif
K_PLUGIN_FACTORY_DEFINITION(EffectFactory,
                            registerPlugin<KWin::VideoRecordEffectConfig>("videorecord");
                           )
K_EXPORT_PLUGIN(EffectFactory("kwin"))

namespace KWin
{

VideoRecordEffectConfig::VideoRecordEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    QHBoxLayout* hlayout = new QHBoxLayout(this);
    QLabel *label = new QLabel(i18n("Path to save video:"), this);
    hlayout->addWidget(label);
    saveVideo = new KUrlRequester(this);
    saveVideo->setMode(KFile::Directory | KFile::LocalOnly);
    hlayout->addWidget(saveVideo);
    layout->addLayout(hlayout);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection* actionCollection = new KActionCollection(this, KComponentData("kwin"));

    KAction* a = static_cast<KAction*>(actionCollection->addAction("VideoRecord"));
    a->setText(i18n("Toggle Video Recording"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_V));

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
                                           KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    layout->addWidget(mShortcutEditor);

    connect(saveVideo, SIGNAL(textChanged(QString)), this, SLOT(changed()));
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));

    layout->addStretch();

    load();
}

VideoRecordEffectConfig::~VideoRecordEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undoChanges();
}

void VideoRecordEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("VideoRecord");
    saveVideo->setPath(conf.readEntry("videopath", KGlobalSettings::documentPath()));
    emit changed(false);
}

void VideoRecordEffectConfig::save()
{
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("VideoRecord");

    if (saveVideo->url().isEmpty())
        conf.writeEntry("videopath", KGlobalSettings::documentPath());
    else
        conf.writeEntry("videopath", saveVideo->url().path());

    conf.sync();

    mShortcutEditor->save();    // undo() will restore to this state from now on

    emit changed(false);
    EffectsHandler::sendReloadMessage("videorecord");
}

void VideoRecordEffectConfig::defaults()
{
    saveVideo->setPath(KGlobalSettings::documentPath());
    mShortcutEditor->allDefault();
    emit changed(true);
}


} // namespace

#include "videorecord_config.moc"
