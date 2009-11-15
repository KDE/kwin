/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>

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

#include "swiveltabs_config.h"

#include <kwineffects.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

SwivelTabsEffectConfigForm::SwivelTabsEffectConfigForm( QWidget* parent ) : QWidget( parent )
    {
    setupUi( this );
    }

SwivelTabsEffectConfig::SwivelTabsEffectConfig(QWidget* parent, const QVariantList& args)
    : KCModule( EffectFactory::componentData(), parent, args )
    {
    m_ui = new SwivelTabsEffectConfigForm( this );
    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( m_ui );
    connect( m_ui->vertical, SIGNAL( toggled( bool )), this, SLOT( changed() ) );
    connect( m_ui->horizontal, SIGNAL( toggled( bool )), this, SLOT( changed() ) );
    connect( m_ui->duration, SIGNAL( valueChanged( int ) ), this, SLOT( changed() ) );
    load();
    }

SwivelTabsEffectConfig::~SwivelTabsEffectConfig()
    {
    delete m_ui;
    }

void SwivelTabsEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig( "SwivelTabs" );

    conf.writeEntry( "SwivelVertical", m_ui->vertical->isChecked() );
    conf.writeEntry( "SwivelHorizontal", m_ui->horizontal->isChecked() );
    conf.writeEntry( "SwivelDuration", m_ui->duration->value() );

    conf.sync();

    KCModule::save();
    emit changed( false );
    EffectsHandler::sendReloadMessage( "swiveltabs" );
    }

void SwivelTabsEffectConfig::load()
    {
    KCModule::load();
    KConfigGroup conf = EffectsHandler::effectConfig( "SwivelTabs" );

    bool vertical = conf.readEntry( "SwivelVertical", true );
    bool horizontal = conf.readEntry( "SwivelHorizontal", true );
    int duration = conf.readEntry("SwivelDuration", 500 );
    m_ui->vertical->setChecked( vertical );
    m_ui->horizontal->setChecked( horizontal );
    m_ui->duration->setValue( duration );
    emit changed( false );
    }

void SwivelTabsEffectConfig::defaults()
    {
    m_ui->vertical->setChecked( true );
    m_ui->horizontal->setChecked( true );
    m_ui->duration->setValue( 500 );
    emit changed( true );
    }

}
