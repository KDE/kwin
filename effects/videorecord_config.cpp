/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "videorecord_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>

#include <QVBoxLayout>
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
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    kDebug() ;

    QVBoxLayout* layout = new QVBoxLayout(this);
    KActionCollection* actionCollection = new KActionCollection( this, KComponentData("kwin") );
    KAction* a = static_cast<KAction*>(actionCollection->addAction( "VideoRecord" ));
    a->setText( i18n("Toggle Video Recording" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_F11));

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(mShortcutEditor);

    layout->addStretch();

    load();
    }

VideoRecordEffectConfig::~VideoRecordEffectConfig()
    {
    kDebug() ;
    }

void VideoRecordEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    emit changed(false);
    }

void VideoRecordEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "videorecord" );
    }

void VideoRecordEffectConfig::defaults()
    {
    kDebug() ;
    mShortcutEditor->allDefault();
    emit changed(true);
    }


} // namespace

#include "videorecord_config.moc"
