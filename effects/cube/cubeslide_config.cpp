/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "cubeslide_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <QVBoxLayout>
#include <QColor>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

CubeSlideEffectConfigForm::CubeSlideEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

CubeSlideEffectConfig::CubeSlideEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    m_ui = new CubeSlideEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->rotationDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->dontSlidePanelsBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->dontSlideStickyWindowsBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    load();
    }

void CubeSlideEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "CubeSlide" );

    int duration       = conf.readEntry( "RotationDuration", 0 );
    bool dontSlidePanels = conf.readEntry( "DontSlidePanels", true );
    bool dontSlideStickyWindows = conf.readEntry( "DontSlideStickyWindows", true );

    m_ui->rotationDurationSpin->setValue( duration );
    m_ui->dontSlidePanelsBox->setChecked( dontSlidePanels );
    m_ui->dontSlideStickyWindowsBox->setChecked( dontSlideStickyWindows );

    emit changed(false);
    }

void CubeSlideEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig( "CubeSlide" );

    conf.writeEntry( "RotationDuration", m_ui->rotationDurationSpin->value() );
    conf.writeEntry( "DontSlidePanels", m_ui->dontSlidePanelsBox->isChecked() );
    conf.writeEntry( "DontSlideStickyWindows", m_ui->dontSlideStickyWindowsBox->isChecked() );

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "cubeslide" );
    }

void CubeSlideEffectConfig::defaults()
    {
    m_ui->rotationDurationSpin->setValue( 0 );
    m_ui->dontSlidePanelsBox->setChecked( true );
    m_ui->dontSlideStickyWindowsBox->setChecked( true );
    emit changed(true);
    }

} // namespace

#include "cubeslide_config.moc"
