//////////////////////////////////////////////////////////////////////////////
// config.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
//
// Based on the Quartz configuration module,
//     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include <KAboutData>
#include <KAboutApplicationDialog>
#include <KConfigGroup>
#include <KGlobal>
#include <KLocale>
#include <kstandarddirs.h>

#include "config.h"
#include "config.moc"

#include "../nitrogenconfiguration.h"
#include "../nitrogenshadowconfiguration.h"

//_______________________________________________________________________
extern "C"
{
  KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
  { return ( new Nitrogen::Config( conf, parent ) ); }
}

namespace Nitrogen
{

  //_______________________________________________________________________
  Config::Config( KConfig*, QWidget* parent ): QObject( parent )
  {

    KGlobal::locale()->insertCatalog("kwin_clients");

    configuration_ = new KConfig( "oxygenrc" );
    KConfigGroup configurationGroup( configuration_, "Windeco");

    userInterface_ = new NitrogenConfigurationUI( parent );
    connect( userInterface_, SIGNAL(changed()), SIGNAL( changed() ) );

    load( configurationGroup );
    userInterface_->show();

  }


  //_______________________________________________________________________
  Config::~Config()
  {
    delete userInterface_;
    delete configuration_;
  }


  //_______________________________________________________________________
  void Config::load( const KConfigGroup& )
  {

    // load standard configuration
    loadConfiguration( NitrogenConfiguration( KConfigGroup( configuration_, "Windeco") ) );
    loadShadowConfiguration( QPalette::Active, NitrogenShadowConfiguration( QPalette::Active, KConfigGroup( configuration_, "ActiveShadow") ) );
    loadShadowConfiguration( QPalette::Inactive, NitrogenShadowConfiguration( QPalette::Inactive, KConfigGroup( configuration_, "InactiveShadow") ) );

    // load exceptions
    NitrogenExceptionList exceptions;
    exceptions.read( *configuration_ );
    if( exceptions.empty() )
    { exceptions = NitrogenExceptionList::defaultList(); }

    // install in ui
    userInterface_->exceptions->setExceptions( exceptions );


  }


  //_______________________________________________________________________
  void Config::save( KConfigGroup& )
  {

    // save standard configuration
    KConfigGroup configurationGroup( configuration_, "Windeco");

    // when writting text entries, on needs to make sure that strings written
    // to the configuration file are *not* translated using current locale
    configurationGroup.writeEntry(
      NitrogenConfig::TITLE_ALIGNMENT,
      NitrogenConfiguration::titleAlignmentName( NitrogenConfiguration::titleAlignment( userInterface_->titleAlignment->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      NitrogenConfig::BUTTON_SIZE,
      NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::buttonSize( userInterface_->buttonSize->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      NitrogenConfig::BLEND_COLOR,
      NitrogenConfiguration::blendColorName( NitrogenConfiguration::blendColor( userInterface_->blendColor->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      NitrogenConfig::FRAME_BORDER,
      NitrogenConfiguration::frameBorderName( NitrogenConfiguration::frameBorder( userInterface_->frameBorder->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      NitrogenConfig::SIZE_GRIP_MODE,
      NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::sizeGripMode( userInterface_->sizeGripMode->currentText(), true ), false ) );

    configurationGroup.writeEntry( NitrogenConfig::DRAW_SEPARATOR, userInterface_->drawSeparator->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::DRAW_TITLE_OUTLINE, userInterface_->titleOutline->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::USE_OXYGEN_SHADOWS, userInterface_->useOxygenShadows->isChecked() );

    // write exceptions
    userInterface_->exceptions->exceptions().write( *configuration_ );

    // write shadow configuration
    saveShadowConfiguration( QPalette::Active, *userInterface_->shadowConfigurations[0] );
    saveShadowConfiguration( QPalette::Inactive, *userInterface_->shadowConfigurations[1] );

    // sync configuration
    configuration_->sync();

  }


  //_______________________________________________________________________
  void Config::saveShadowConfiguration( QPalette::ColorGroup colorGroup, const NitrogenShadowConfigurationUI& ui ) const
  {

    assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );

    // save shadow configuration
    KConfigGroup configurationGroup( configuration_, ( (colorGroup == QPalette::Active) ? "ActiveShadow":"InactiveShadow" ) );
    configurationGroup.writeEntry( NitrogenConfig::SHADOW_SIZE, 0.1*ui.shadowSize->value() );
    configurationGroup.writeEntry( NitrogenConfig::SHADOW_HOFFSET, 0.1*ui.horizontalOffset->value() );
    configurationGroup.writeEntry( NitrogenConfig::SHADOW_VOFFSET, 0.1*ui.verticalOffset->value() );
    configurationGroup.writeEntry( NitrogenConfig::SHADOW_INNER_COLOR, ui.innerColor->color() );
    configurationGroup.writeEntry( NitrogenConfig::SHADOW_OUTER_COLOR, ui.outerColor->color() );
    configurationGroup.writeEntry( NitrogenConfig::SHADOW_USE_OUTER_COLOR, ui.useOuterColor->isChecked() );

  }

  //_______________________________________________________________________
  void Config::defaults()
  {

    // install default configuration
    loadConfiguration( NitrogenConfiguration() );

    // load shadows
    loadShadowConfiguration( QPalette::Active, NitrogenShadowConfiguration( QPalette::Active ) );
    loadShadowConfiguration( QPalette::Inactive, NitrogenShadowConfiguration( QPalette::Inactive ) );

    // install default exceptions
    userInterface_->exceptions->setExceptions( NitrogenExceptionList::defaultList() );

    // emit changed signal
    emit changed();

  }

  //_______________________________________________________________________
  void Config::loadConfiguration( const NitrogenConfiguration& configuration )
  {

    userInterface_->titleAlignment->setCurrentIndex( userInterface_->titleAlignment->findText( configuration.titleAlignmentName( true ) ) );
    userInterface_->buttonSize->setCurrentIndex( userInterface_->buttonSize->findText( configuration.buttonSizeName( true ) ) );
    userInterface_->blendColor->setCurrentIndex( userInterface_->blendColor->findText( configuration.blendColorName( true ) ) );
    userInterface_->frameBorder->setCurrentIndex( userInterface_->frameBorder->findText( configuration.frameBorderName( true ) ) );
    userInterface_->sizeGripMode->setCurrentIndex( userInterface_->sizeGripMode->findText( configuration.sizeGripModeName( true ) ) );

    userInterface_->drawSeparator->setChecked( configuration.drawSeparator() );
    userInterface_->titleOutline->setChecked( configuration.drawTitleOutline() );
    userInterface_->useOxygenShadows->setChecked( configuration.useOxygenShadows() );
  }


  //_______________________________________________________________________
  void Config::loadShadowConfiguration( QPalette::ColorGroup colorGroup, const NitrogenShadowConfiguration& configuration )
  {
    assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );
    NitrogenShadowConfigurationUI* ui = userInterface_->shadowConfigurations[ (colorGroup == QPalette::Active) ? 0:1 ];
    ui->shadowSize->setValue( 10*configuration.shadowSize() );
    ui->horizontalOffset->setValue( 10*configuration.horizontalOffset() );
    ui->verticalOffset->setValue( 10*configuration.verticalOffset() );
    ui->innerColor->setColor( configuration.innerColor() );
    ui->outerColor->setColor( configuration.outerColor() );
    ui->useOuterColor->setChecked( configuration.useOuterColor() );
  }
  //_______________________________________________________________________
  void Config::aboutNitrogen( void )
  {}

}
