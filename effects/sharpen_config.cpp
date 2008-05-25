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
    kDebug();

    QVBoxLayout* layout = new QVBoxLayout(this);
    KActionCollection* actionCollection = new KActionCollection( this, componentData() );
    KAction* a = static_cast<KAction*>(actionCollection->addAction( "Sharpen" ));
    a->setText( i18n("Toggle Sharpen Effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_S));
    a->setProperty("isConfigurationAction", true);

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(mShortcutEditor);

    layout->addStretch();

    load();
    }

SharpenEffectConfig::~SharpenEffectConfig()
    {
    kDebug();
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undoChanges();
    }

void SharpenEffectConfig::load()
    {
    kDebug();
    KCModule::load();

    emit changed(false);
    }

void SharpenEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    mShortcutEditor->save();    // undo() will restore to this state from now on

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
