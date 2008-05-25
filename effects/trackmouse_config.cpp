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
    kDebug() ;

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* label = new QLabel(i18n("Hold Ctrl+Meta keys to see where the mouse cursor is."), this);
    label->setWordWrap(true);
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
