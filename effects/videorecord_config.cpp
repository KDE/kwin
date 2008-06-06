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
#include <KGlobalAccel>
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
    kDebug();

    QVBoxLayout* layout = new QVBoxLayout(this);
    QHBoxLayout* hlayout = new QHBoxLayout( this );
    QLabel *label = new QLabel( i18n( "Path to save video:" ), this );
    hlayout->addWidget( label );
    saveVideo = new KUrlRequester( this );
    saveVideo->setMode( KFile::Directory | KFile::LocalOnly );
    hlayout->addWidget( saveVideo );
    layout->addLayout( hlayout );

    KActionCollection* actionCollection = new KActionCollection( this, componentData() );
    KAction* a = static_cast<KAction*>(actionCollection->addAction( "VideoRecord" ));
    a->setText( i18n("Toggle Video Recording" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_V));

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(mShortcutEditor);

    layout->addStretch();

    load();
    }

VideoRecordEffectConfig::~VideoRecordEffectConfig()
    {
    kDebug();
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undoChanges();
    }

void VideoRecordEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("VideoRecord");
    saveVideo->setPath( conf.readEntry( "videopath", KGlobalSettings::documentPath() ) );
    emit changed(false);
    }

void VideoRecordEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("VideoRecord");

    conf.writeEntry("videopath", saveVideo->url().path());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "videorecord" );
    }

void VideoRecordEffectConfig::defaults()
    {
    kDebug() ;
    saveVideo->setPath(KGlobalSettings::documentPath() );
    mShortcutEditor->allDefault();
    emit changed(true);
    }


} // namespace

#include "videorecord_config.moc"
