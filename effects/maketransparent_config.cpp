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

#include "maketransparent_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>

#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

MakeTransparentEffectConfig::MakeTransparentEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(new QLabel(i18n("Changes opacity of following elements:"), this), 0, 0, 1, 2);

    layout->addWidget(new QLabel(i18n("Decorations:"), this), 1, 0);
    mDecoration = new QSpinBox(this);
    mDecoration->setRange(10, 100);
    mDecoration->setSuffix("%");
    connect(mDecoration, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mDecoration, 1, 1);

    layout->addWidget(new QLabel(i18n("Inactive windows:"), this), 2, 0);
    mInactive = new QSpinBox(this);
    mInactive->setRange(10, 100);
    mInactive->setSuffix("%");
    connect(mInactive, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mInactive, 2, 1);

    layout->addWidget(new QLabel(i18n("Moved/resized windows:"), this), 3, 0);
    mMoveResize = new QSpinBox(this);
    mMoveResize->setRange(10, 100);
    mMoveResize->setSuffix("%");
    connect(mMoveResize, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mMoveResize, 3, 1);

    layout->addWidget(new QLabel(i18n("Dialogs:"), this), 4, 0);
    mDialogs = new QSpinBox(this);
    mDialogs->setRange(10, 100);
    mDialogs->setSuffix("%");
    connect(mDialogs, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mDialogs, 4, 1);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 4, 0, 1, 2);

    load();
    }

void MakeTransparentEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("MakeTransparent");
    mDecoration->setValue( (int)( conf.readEntry( "Decoration", 1.0 ) * 100 ) );
    mMoveResize->setValue( (int)( conf.readEntry( "MoveResize", 0.8 ) * 100 ) );
    mDialogs->setValue( (int)( conf.readEntry( "Dialogs", 1.0 ) * 100 ) );
    mInactive->setValue( (int)( conf.readEntry( "Inactive", 0.6 ) * 100 ) );

    emit changed(false);
    }

void MakeTransparentEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("MakeTransparent");
    conf.writeEntry( "Decoration", mDecoration->value() / 100.0 );
    conf.writeEntry( "MoveResize", mMoveResize->value() / 100.0 );
    conf.writeEntry( "Dialogs", mDialogs->value() / 100.0 );
    conf.writeEntry( "Inactive", mInactive->value() / 100.0 );
    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "maketransparent" );
    }

void MakeTransparentEffectConfig::defaults()
    {
    kDebug() ;
    mDecoration->setValue( 70 );
    mMoveResize->setValue( 80 );
    mDialogs->setValue( 100 );
    mInactive->setValue( 100 );
    emit changed(true);
    }


} // namespace

#include "maketransparent_config.moc"
