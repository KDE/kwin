/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "trackmouse_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>

#include <QVBoxLayout>
#include <QLabel>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif
namespace KWin
{

TrackMouseEffectConfig::TrackMouseEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    kDebug() ;

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* label = new QLabel(i18n("Hold Ctrl+Meta keys to see where the mouse cursor is."), this);
    layout->addWidget(label);

    layout->addStretch();

    load();
    }

TrackMouseEffectConfig::~TrackMouseEffectConfig()
    {
    kDebug() ;
    }

void TrackMouseEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    emit changed(false);
    }

void TrackMouseEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "trackmouse" );
    }

void TrackMouseEffectConfig::defaults()
    {
    kDebug() ;
    emit changed(true);
    }


} // namespace

#include "trackmouse_config.moc"
