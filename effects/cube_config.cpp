/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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
#include "cube_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <KActionCollection>
#include <kaction.h>

#include <QGridLayout>
#include <QColor>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

CubeEffectConfigForm::CubeEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

CubeEffectConfig::CubeEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    m_ui = new CubeEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    m_ui->screenEdgeCombo->addItem(i18n("Top"));
    m_ui->screenEdgeCombo->addItem(i18n("Top-right"));
    m_ui->screenEdgeCombo->addItem(i18n("Right"));
    m_ui->screenEdgeCombo->addItem(i18n("Bottom-right"));
    m_ui->screenEdgeCombo->addItem(i18n("Bottom"));
    m_ui->screenEdgeCombo->addItem(i18n("Bottom-left"));
    m_ui->screenEdgeCombo->addItem(i18n("Left"));
    m_ui->screenEdgeCombo->addItem(i18n("Top-left"));
    m_ui->screenEdgeCombo->addItem(i18n("None"));

    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup( "Cube" );
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*) m_actionCollection->addAction( "Cube" );
    a->setText( i18n("Desktop Cube" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F11 ));

    m_ui->editor->addCollection(m_actionCollection);

    connect(m_ui->screenEdgeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->rotationDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeOpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeOpacitySpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->displayDesktopNameBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->reflectionBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->backgroundColorButton, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->cubeCapsBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeCapsBox, SIGNAL(stateChanged(int)), this, SLOT(capsSelectionChanged()));
    connect(m_ui->capColorButton, SIGNAL(changed(QColor)), this, SLOT(changed()));

    load();
    }

void CubeEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "Cube" );

    int duration       = conf.readEntry( "RotationDuration", 500 );
    float opacity      = conf.readEntry( "Opacity", 80 );
    bool desktopName   = conf.readEntry( "DisplayDesktopName", true );
    bool reflection    = conf.readEntry( "Reflection", true );
    int activateBorder = conf.readEntry( "BorderActivate", (int)ElectricNone );
    QColor background  = conf.readEntry( "BackgroundColor", QColor( Qt::black ) );
    QColor capColor = conf.readEntry( "CapColor", KColorScheme( QPalette::Active, KColorScheme::Window ).background().color() );
    bool caps = conf.readEntry( "Caps", true );
    if( activateBorder == (int)ElectricNone )
        activateBorder--;
    m_ui->screenEdgeCombo->setCurrentIndex( activateBorder );
    
    m_ui->rotationDurationSpin->setValue( duration );
    m_ui->cubeOpacitySlider->setValue( opacity );
    m_ui->cubeOpacitySpin->setValue( opacity );
    if( desktopName )
        {
        m_ui->displayDesktopNameBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->displayDesktopNameBox->setCheckState( Qt::Unchecked );
        }
    if( reflection )
        {
        m_ui->reflectionBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->reflectionBox->setCheckState( Qt::Unchecked );
        }
    if( caps )
        {
        m_ui->cubeCapsBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->cubeCapsBox->setCheckState( Qt::Unchecked );
        }
    m_ui->backgroundColorButton->setColor( background );
    m_ui->capColorButton->setColor( capColor );
    capsSelectionChanged();

    emit changed(false);
    }

void CubeEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig( "Cube" );

    conf.writeEntry( "RotationDuration", m_ui->rotationDurationSpin->value() );
    conf.writeEntry( "DisplayDesktopName", m_ui->displayDesktopNameBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "Reflection", m_ui->reflectionBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "Opacity", m_ui->cubeOpacitySpin->value() );
    conf.writeEntry( "BackgroundColor", m_ui->backgroundColorButton->color() );
    conf.writeEntry( "Caps", m_ui->cubeCapsBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "CapColor", m_ui->capColorButton->color() );

    int activateBorder = m_ui->screenEdgeCombo->currentIndex();
    if( activateBorder == (int)ELECTRIC_COUNT )
        activateBorder = (int)ElectricNone;
    conf.writeEntry( "BorderActivate", activateBorder );

    m_ui->editor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "cube" );
    }

void CubeEffectConfig::defaults()
    {
    m_ui->rotationDurationSpin->setValue( 500 );
    m_ui->displayDesktopNameBox->setCheckState( Qt::Checked );
    m_ui->reflectionBox->setCheckState( Qt::Checked );
    m_ui->cubeOpacitySpin->setValue( 80 );
    m_ui->cubeOpacitySlider->setValue( 80 );
    m_ui->screenEdgeCombo->setCurrentIndex( (int)ElectricNone -1 );
    m_ui->backgroundColorButton->setColor( QColor( Qt::black ) );
    m_ui->cubeCapsBox->setCheckState( Qt::Checked );
    m_ui->capColorButton->setColor( KColorScheme( QPalette::Active, KColorScheme::Window ).background().color() );
    m_ui->editor->allDefault();
    emit changed(true);
    }

void CubeEffectConfig::capsSelectionChanged()
    {
    if( m_ui->cubeCapsBox->checkState() == Qt::Checked )
        {
        // activate cap color
        m_ui->capColorButton->setEnabled( true );
        m_ui->capColorLabel->setEnabled( true );
        }
    else
        {
        // deactivate cap color
        m_ui->capColorButton->setEnabled( false );
        m_ui->capColorLabel->setEnabled( false );
        }
    }

} // namespace

#include "cube_config.moc"
