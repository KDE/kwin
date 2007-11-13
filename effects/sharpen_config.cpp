/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "sharpen_config.h"

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
namespace KWin
{

SharpenEffectConfig::SharpenEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    kDebug() ;

    QVBoxLayout* layout = new QVBoxLayout(this);
    KActionCollection* actionCollection = new KActionCollection( this, KComponentData("kwin") );
    KAction* a = static_cast<KAction*>(actionCollection->addAction( "Sharpen" ));
    a->setText( i18n("Toggle Sharpen effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F7));

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(mShortcutEditor);

    layout->addStretch();

    load();
    }

SharpenEffectConfig::~SharpenEffectConfig()
    {
    kDebug() ;
    }

void SharpenEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    emit changed(false);
    }

void SharpenEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "sharpen" );
    }

void SharpenEffectConfig::defaults()
    {
    kDebug() ;
    mShortcutEditor->allDefault();
    emit changed(true);
    }


} // namespace

#include "sharpen_config.moc"
