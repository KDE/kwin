/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "shadow_config.h"
#include "shadow_helper.h"
#include <kwineffects.h>

#include <kcolorscheme.h>

#include <QVBoxLayout>
#include <QColor>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

ShadowEffectConfigForm::ShadowEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

ShadowEffectConfig::ShadowEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule( EffectFactory::componentData(), parent, args )
    {
    m_ui = new ShadowEffectConfigForm( this );

    QVBoxLayout* layout = new QVBoxLayout( this );

    layout->addWidget( m_ui );

    connect( m_ui->xOffsetSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->yOffsetSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->opacitySpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->fuzzinessSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->sizeSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->colorButton, SIGNAL( changed( int )), this, SLOT( changed() ));
    connect( m_ui->strongerActiveBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));

    connect( m_ui->forceDecoratedBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->forceUndecoratedBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->forceOtherBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));

    load();
    }

ShadowEffectConfig::~ShadowEffectConfig()
    {
    }

void ShadowEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "Shadow" );

    m_ui->xOffsetSpin->setValue( conf.readEntry( "XOffset", 0 ));
    m_ui->yOffsetSpin->setValue( conf.readEntry( "YOffset", 3 ));
    m_ui->opacitySpin->setValue( (int)( conf.readEntry( "Opacity", 0.25 ) * 100 ));
    m_ui->fuzzinessSpin->setValue( conf.readEntry( "Fuzzyness", 10 ));
    m_ui->sizeSpin->setValue( conf.readEntry( "Size", 5 ));
    m_ui->colorButton->setColor( conf.readEntry( "Color", schemeShadowColor() ));
    m_ui->strongerActiveBox->setChecked( conf.readEntry( "IntensifyActiveShadow", true ));

    m_ui->forceDecoratedBox->setChecked( conf.readEntry( "forceDecoratedToDefault", false ));
    m_ui->forceUndecoratedBox->setChecked( conf.readEntry( "forceUndecoratedToDefault", false ));
    m_ui->forceOtherBox->setChecked( conf.readEntry( "forceOtherToDefault", false ));

    emit changed(false);
    }

void ShadowEffectConfig::save()
    {
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig( "Shadow" );

    conf.writeEntry( "XOffset", m_ui->xOffsetSpin->value() );
    conf.writeEntry( "YOffset", m_ui->yOffsetSpin->value() );
    conf.writeEntry( "Opacity", m_ui->opacitySpin->value() / 100.0 );
    conf.writeEntry( "Fuzzyness", m_ui->fuzzinessSpin->value() );
    conf.writeEntry( "Size", m_ui->sizeSpin->value() );
    QColor userColor = m_ui->colorButton->color();
    if( userColor == schemeShadowColor() )
        // If the user has reset the color to the default we want to start
        // picking up color scheme changes again in the shadow effect
        conf.deleteEntry( "Color" );
    else
        conf.writeEntry( "Color", userColor );
    conf.writeEntry( "IntensifyActiveShadow", m_ui->strongerActiveBox->isChecked() );

    conf.writeEntry( "forceDecoratedToDefault", m_ui->forceDecoratedBox->isChecked() );
    conf.writeEntry( "forceUndecoratedToDefault", m_ui->forceUndecoratedBox->isChecked() );
    conf.writeEntry( "forceOtherToDefault", m_ui->forceOtherBox->isChecked() );

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "shadow" );
    }

void ShadowEffectConfig::defaults()
    {
    m_ui->xOffsetSpin->setValue( 0 );
    m_ui->yOffsetSpin->setValue( 3 );
    m_ui->opacitySpin->setValue( 25 );
    m_ui->fuzzinessSpin->setValue( 10 );
    m_ui->sizeSpin->setValue( 5 );
    m_ui->colorButton->setColor( schemeShadowColor() );
    m_ui->strongerActiveBox->setChecked( true );

    m_ui->forceDecoratedBox->setChecked( false );
    m_ui->forceUndecoratedBox->setChecked( false );
    m_ui->forceOtherBox->setChecked( false );

    emit changed(true);
    }

} // namespace

#include "shadow_config.moc"
