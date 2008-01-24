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
#include "flipswitch_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QGridLayout>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

FlipSwitchEffectConfigForm::FlipSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

FlipSwitchEffectConfig::FlipSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    m_ui = new FlipSwitchEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->checkAnimateFlip, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->spinFlipDuration, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    load();
    }

FlipSwitchEffectConfig::~FlipSwitchEffectConfig()
    {
    }

void FlipSwitchEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "FlipSwitch" );

    int flipDuration = conf.readEntry( "FlipDuration", 300 );
    bool animateFlip = conf.readEntry( "AnimateFlip", true );
    m_ui->spinFlipDuration->setValue( flipDuration );
    if( animateFlip )
        {
        m_ui->checkAnimateFlip->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->checkAnimateFlip->setCheckState( Qt::Unchecked );
        }

    emit changed(false);
    }

void FlipSwitchEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig( "FlipSwitch" );

    conf.writeEntry( "FlipDuration", m_ui->spinFlipDuration->value() );
    conf.writeEntry( "AnimateFlip", m_ui->checkAnimateFlip->checkState() == Qt::Checked ? true : false );

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "flipswitch" );
    }

void FlipSwitchEffectConfig::defaults()
    {
    m_ui->spinFlipDuration->setValue( 300 );
    m_ui->checkAnimateFlip->setCheckState( Qt::Checked );
    emit changed(true);
    }


} // namespace

#include "flipswitch_config.moc"