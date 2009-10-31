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

#include "config.h"
#include "config.moc"

#include "../oxygenconfiguration.h"
#include "../oxygenshadowconfiguration.h"

#include <KAboutData>
#include <KAboutApplicationDialog>
#include <KConfigGroup>
#include <KGlobal>
#include <KLocale>

//_______________________________________________________________________
extern "C"
{
  KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
  { return ( new Oxygen::Config( conf, parent ) ); }
}

namespace Oxygen
{

  //_______________________________________________________________________
  Config::Config( KConfig*, QWidget* parent ): QObject( parent )
  {

    KGlobal::locale()->insertCatalog("kwin_clients");

    configuration_ = new KConfig( "oxygenrc" );
    KConfigGroup configurationGroup( configuration_, "Windeco");

    userInterface_ = new OxygenConfigurationUI( parent );
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
    loadConfiguration( OxygenConfiguration( KConfigGroup( configuration_, "Windeco") ) );
    loadShadowConfiguration( QPalette::Active, OxygenShadowConfiguration( QPalette::Active, KConfigGroup( configuration_, "ActiveShadow") ) );
    loadShadowConfiguration( QPalette::Inactive, OxygenShadowConfiguration( QPalette::Inactive, KConfigGroup( configuration_, "InactiveShadow") ) );

    // load exceptions
    OxygenExceptionList exceptions;
    exceptions.read( *configuration_ );
    if( exceptions.empty() )
    { exceptions = OxygenExceptionList::defaultList(); }

    // install in ui
    userInterface_->exceptions->setExceptions( exceptions );


  }


  //_______________________________________________________________________
  void Config::save( KConfigGroup& )
  {

    // save standard configuration
    KConfigGroup configurationGroup( configuration_, "Windeco");

    // when writing text entries, on needs to make sure that strings written
    // to the configuration file are *not* translated using current locale
    configurationGroup.writeEntry(
      OxygenConfig::TITLE_ALIGNMENT,
      OxygenConfiguration::titleAlignmentName( OxygenConfiguration::titleAlignment( userInterface_->titleAlignment->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      OxygenConfig::BUTTON_SIZE,
      OxygenConfiguration::buttonSizeName( OxygenConfiguration::buttonSize( userInterface_->buttonSize->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      OxygenConfig::BLEND_COLOR,
      OxygenConfiguration::blendColorName( OxygenConfiguration::blendColor( userInterface_->blendColor->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      OxygenConfig::FRAME_BORDER,
      OxygenConfiguration::frameBorderName( OxygenConfiguration::frameBorder( userInterface_->frameBorder->currentText(), true ), false ) );

    configurationGroup.writeEntry(
      OxygenConfig::SIZE_GRIP_MODE,
      OxygenConfiguration::sizeGripModeName( OxygenConfiguration::sizeGripMode( userInterface_->sizeGripMode->currentText(), true ), false ) );

    configurationGroup.writeEntry( OxygenConfig::DRAW_SEPARATOR, userInterface_->drawSeparator->isChecked() );
    configurationGroup.writeEntry( OxygenConfig::DRAW_TITLE_OUTLINE, userInterface_->titleOutline->isChecked() );
    configurationGroup.writeEntry( OxygenConfig::USE_OXYGEN_SHADOWS, userInterface_->useOxygenShadows->isChecked() );

    // write exceptions
    userInterface_->exceptions->exceptions().write( *configuration_ );

    // write shadow configuration
    saveShadowConfiguration( QPalette::Active, *userInterface_->shadowConfigurations[0] );
    saveShadowConfiguration( QPalette::Inactive, *userInterface_->shadowConfigurations[1] );

    // sync configuration
    configuration_->sync();

  }


  //_______________________________________________________________________
  void Config::saveShadowConfiguration( QPalette::ColorGroup colorGroup, const OxygenShadowConfigurationUI& ui ) const
  {

    assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );

    // save shadow configuration
    KConfigGroup configurationGroup( configuration_, ( (colorGroup == QPalette::Active) ? "ActiveShadow":"InactiveShadow" ) );
    configurationGroup.writeEntry( OxygenConfig::SHADOW_SIZE, ui.ui.shadowSize->value() );
    configurationGroup.writeEntry( OxygenConfig::SHADOW_VOFFSET, 0.1*ui.ui.verticalOffset->value() );
    configurationGroup.writeEntry( OxygenConfig::SHADOW_INNER_COLOR, ui.ui.innerColor->color() );
    configurationGroup.writeEntry( OxygenConfig::SHADOW_OUTER_COLOR, ui.ui.outerColor->color() );
    configurationGroup.writeEntry( OxygenConfig::SHADOW_USE_OUTER_COLOR, ui.ui.useOuterColor->isChecked() );

  }

  //_______________________________________________________________________
  void Config::defaults()
  {

    // install default configuration
    loadConfiguration( OxygenConfiguration() );

    // load shadows
    loadShadowConfiguration( QPalette::Active, OxygenShadowConfiguration( QPalette::Active ) );
    loadShadowConfiguration( QPalette::Inactive, OxygenShadowConfiguration( QPalette::Inactive ) );

    // install default exceptions
    userInterface_->exceptions->setExceptions( OxygenExceptionList::defaultList() );

    // emit changed signal
    emit changed();

  }

  //_______________________________________________________________________
  void Config::loadConfiguration( const OxygenConfiguration& configuration )
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
  void Config::loadShadowConfiguration( QPalette::ColorGroup colorGroup, const OxygenShadowConfiguration& configuration )
  {
    assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );
    OxygenShadowConfigurationUI* ui = userInterface_->shadowConfigurations[ (colorGroup == QPalette::Active) ? 0:1 ];
    ui->ui.shadowSize->setValue( configuration.shadowSize() );
    ui->ui.verticalOffset->setValue( 10*configuration.verticalOffset() );
    ui->ui.innerColor->setColor( configuration.innerColor() );
    ui->ui.outerColor->setColor( configuration.outerColor() );
    ui->ui.useOuterColor->setChecked( configuration.useOuterColor() );
  }
  //_______________________________________________________________________
  void Config::aboutOxygen( void )
  {}

}
