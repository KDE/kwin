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
#include "sphere_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <KActionCollection>
#include <kaction.h>
#include <KFileDialog>
#include <KImageFilePreview>

#include <QGridLayout>
#include <QColor>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

SphereEffectConfigForm::SphereEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

SphereEffectConfig::SphereEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    m_ui = new SphereEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    m_ui->tabWidget->setTabText( 0, i18nc("@title:tab Basic Settings", "Basic") );
    m_ui->tabWidget->setTabText( 1, i18nc("@title:tab Advanced Settings", "Advanced") );

    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup( "Cube" );
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*) m_actionCollection->addAction( "Cube" );
    a->setText( i18n("Desktop Cube" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F11 ));

    m_ui->editor->addCollection(m_actionCollection);

    connect(m_ui->rotationDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeOpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeOpacitySpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->displayDesktopNameBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->backgroundColorButton, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->cubeCapsBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->cubeCapsBox, SIGNAL(stateChanged(int)), this, SLOT(capsSelectionChanged()));
    connect(m_ui->capsImageBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->capColorButton, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->wallpaperRequester, SIGNAL(textChanged(QString)), this, SLOT(changed()));
    connect(m_ui->closeOnMouseReleaseBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->zPositionSlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->capDeformationSlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->walkThroughDesktopBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    load();
    }

void SphereEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "Sphere" );

    int duration       = conf.readEntry( "RotationDuration", 0 );
    float opacity      = conf.readEntry( "Opacity", 80 );
    bool desktopName   = conf.readEntry( "DisplayDesktopName", true );
    QColor background  = conf.readEntry( "BackgroundColor", QColor( Qt::black ) );
    QColor capColor = conf.readEntry( "CapColor", KColorScheme( QPalette::Active, KColorScheme::Window ).background().color() );
    bool texturedCaps = conf.readEntry( "TexturedCaps", true );
    bool caps = conf.readEntry( "Caps", true );
    bool closeOnMouseRelease = conf.readEntry( "CloseOnMouseRelease", false );
    bool walkThroughDesktop = conf.readEntry( "TabBox", false );
    m_ui->zPositionSlider->setValue( conf.readEntry( "ZPosition", 450 ) );
    m_ui->capDeformationSlider->setValue( conf.readEntry( "CapDeformation", 0 ) );
    m_ui->wallpaperRequester->setPath( conf.readEntry( "Wallpaper", "" ) );
    
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
    if( caps )
        {
        m_ui->cubeCapsBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->cubeCapsBox->setCheckState( Qt::Unchecked );
        }
    if( texturedCaps )
        {
        m_ui->capsImageBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->capsImageBox->setCheckState( Qt::Unchecked );
        }
    if( closeOnMouseRelease )
        {
        m_ui->closeOnMouseReleaseBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->closeOnMouseReleaseBox->setCheckState( Qt::Unchecked );
        }
    if( walkThroughDesktop )
        {
        m_ui->walkThroughDesktopBox->setCheckState( Qt::Checked );
        }
    else
        {
        m_ui->walkThroughDesktopBox->setCheckState( Qt::Unchecked );
        }
    m_ui->backgroundColorButton->setColor( background );
    m_ui->capColorButton->setColor( capColor );
    capsSelectionChanged();

    emit changed(false);
    }

void SphereEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig( "Sphere" );

    conf.writeEntry( "RotationDuration", m_ui->rotationDurationSpin->value() );
    conf.writeEntry( "DisplayDesktopName", m_ui->displayDesktopNameBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "Opacity", m_ui->cubeOpacitySpin->value() );
    conf.writeEntry( "BackgroundColor", m_ui->backgroundColorButton->color() );
    conf.writeEntry( "Caps", m_ui->cubeCapsBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "CapColor", m_ui->capColorButton->color() );
    conf.writeEntry( "TexturedCaps", m_ui->capsImageBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "CloseOnMouseRelease", m_ui->closeOnMouseReleaseBox->checkState() == Qt::Checked ? true : false );
    conf.writeEntry( "Wallpaper", m_ui->wallpaperRequester->url().path() );
    conf.writeEntry( "ZPosition", m_ui->zPositionSlider->value() );
    conf.writeEntry( "CapDeformation", m_ui->capDeformationSlider->value() );
    conf.writeEntry( "TabBox", m_ui->walkThroughDesktopBox->checkState() == Qt::Checked ? true : false );

    m_ui->editor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "sphere" );
    }

void SphereEffectConfig::defaults()
    {
    m_ui->rotationDurationSpin->setValue( 0 );
    m_ui->displayDesktopNameBox->setCheckState( Qt::Checked );
    m_ui->cubeOpacitySpin->setValue( 80 );
    m_ui->cubeOpacitySlider->setValue( 80 );
    m_ui->backgroundColorButton->setColor( QColor( Qt::black ) );
    m_ui->cubeCapsBox->setCheckState( Qt::Checked );
    m_ui->capColorButton->setColor( KColorScheme( QPalette::Active, KColorScheme::Window ).background().color() );
    m_ui->capsImageBox->setCheckState( Qt::Checked );
    m_ui->closeOnMouseReleaseBox->setCheckState( Qt::Unchecked );
    m_ui->wallpaperRequester->setPath( "" );
    m_ui->zPositionSlider->setValue( 450 );
    m_ui->capDeformationSlider->setValue( 0 );
    m_ui->walkThroughDesktopBox->setCheckState( Qt::Unchecked );
    m_ui->editor->allDefault();
    emit changed(true);
    }

void SphereEffectConfig::capsSelectionChanged()
    {
    if( m_ui->cubeCapsBox->checkState() == Qt::Checked )
        {
        // activate cap color
        m_ui->capColorButton->setEnabled( true );
        m_ui->capColorLabel->setEnabled( true );
        m_ui->capsImageBox->setEnabled( true );
        m_ui->capDeformationGroupBox->setEnabled( true );
        m_ui->capDeformationSlider->setEnabled( true );
        m_ui->capDeformationSphereLabel->setEnabled( true );
        m_ui->capDeformationPlaneLabel->setEnabled( true );
        }
    else
        {
        // deactivate cap color
        m_ui->capColorButton->setEnabled( false );
        m_ui->capColorLabel->setEnabled( false );
        m_ui->capsImageBox->setEnabled( false );
        m_ui->capDeformationGroupBox->setEnabled( false );
        m_ui->capDeformationSlider->setEnabled( false );
        m_ui->capDeformationSphereLabel->setEnabled( false );
        m_ui->capDeformationPlaneLabel->setEnabled( false );
        }
    }

} // namespace

#include "sphere_config.moc"
