/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com

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
#include "coverswitch_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QGridLayout>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

CoverSwitchEffectConfigForm::CoverSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

CoverSwitchEffectConfig::CoverSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    m_ui = new CoverSwitchEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->checkAnimateSwitch, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkAnimateStart, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkAnimateStop, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkReflection, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->spinDuration, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    load();
    }

CoverSwitchEffectConfig::~CoverSwitchEffectConfig()
    {
    }

void CoverSwitchEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "CoverSwitch" );

    int duration       = conf.readEntry( "Duration", 200 );
    bool animateSwitch = conf.readEntry( "AnimateSwitch", true );
    bool animateStart  = conf.readEntry( "AnimateStart", true );
    bool animateStop   = conf.readEntry( "AnimateStop", true );
    bool reflection    = conf.readEntry( "Reflection", true );
    m_ui->spinDuration->setValue( duration );
    if( animateSwitch )
        {
        m_ui->checkAnimateSwitch->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->checkAnimateSwitch->setCheckState( Qt::Unchecked );
        }
    if( animateStart )
        {
        m_ui->checkAnimateStart->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->checkAnimateStart->setCheckState( Qt::Unchecked );
        }
    if( animateStop )
        {
        m_ui->checkAnimateStop->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->checkAnimateStop->setCheckState( Qt::Unchecked );
        }
    if( reflection )
        {
        m_ui->checkReflection->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->checkReflection->setCheckState( Qt::Unchecked );
        }

    emit changed(false);
    }

void CoverSwitchEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig( "CoverSwitch" );

    conf.writeEntry( "Duration", m_ui->spinDuration->value() );
    conf.writeEntry( "AnimateSwitch", m_ui->checkAnimateSwitch->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "AnimateStart", m_ui->checkAnimateStart->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "AnimateStop", m_ui->checkAnimateStop->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "Reflection", m_ui->checkReflection->checkState() == Qt::Checked ? true : false );

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "coverswitch" );
    }

void CoverSwitchEffectConfig::defaults()
    {
    m_ui->spinDuration->setValue( 200 );
    m_ui->checkAnimateSwitch->setCheckState( Qt::Checked );
    m_ui->checkAnimateStart->setCheckState( Qt::Checked );
    m_ui->checkAnimateStop->setCheckState( Qt::Checked );
    m_ui->checkReflection->setCheckState( Qt::Checked );
    emit changed(true);
    }


} // namespace

#include "coverswitch_config.moc"
