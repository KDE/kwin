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
    
    configuration_ = new KConfig( "nitrogenrc" );
    KConfigGroup configurationGroup( configuration_, "Windeco");
    
    user_interface_ = new NitrogenConfigurationUI( parent );    
    connect( user_interface_->titleAlignment, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( user_interface_->buttonSize, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( user_interface_->frameBorder, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( user_interface_->blendColor, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( user_interface_->sizeGripMode, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
   
    connect( user_interface_->showStripes, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->drawSeparator, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->titleOutline, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->useOxygenShadows, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->exceptions, SIGNAL(changed()), SIGNAL(changed()) );
     
    connect( user_interface_->titleOutline, SIGNAL(toggled( bool )), user_interface_->drawSeparator, SLOT( setDisabled( bool ) ) );
   
    load( configurationGroup );
    user_interface_->show();
    
  }
  
  
  //_______________________________________________________________________
  Config::~Config()
  {
    delete user_interface_;
    delete configuration_;
  }
  
  
  //_______________________________________________________________________
  void Config::load( const KConfigGroup& )
  {
    
    // load standard configuration
    KConfigGroup configurationGroup( configuration_, "Windeco");
    loadConfiguration( NitrogenConfiguration( configurationGroup ) );
    
    // load exceptions
    NitrogenExceptionList exceptions;
    exceptions.read( *configuration_ );
    if( exceptions.empty() )
    { exceptions = NitrogenExceptionList::defaultList(); }
    
    // install in ui
    user_interface_->exceptions->setExceptions( exceptions );
    
    
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
      NitrogenConfiguration::titleAlignmentName( NitrogenConfiguration::titleAlignment( user_interface_->titleAlignment->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( 
      NitrogenConfig::BUTTON_SIZE, 
      NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::buttonSize( user_interface_->buttonSize->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( 
      NitrogenConfig::BLEND_COLOR, 
      NitrogenConfiguration::blendColorName( NitrogenConfiguration::blendColor( user_interface_->blendColor->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( 
      NitrogenConfig::FRAME_BORDER, 
      NitrogenConfiguration::frameBorderName( NitrogenConfiguration::frameBorder( user_interface_->frameBorder->currentText(), true ), false ) );

    configurationGroup.writeEntry( 
      NitrogenConfig::SIZE_GRIP_MODE,
      NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::sizeGripMode( user_interface_->sizeGripMode->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( NitrogenConfig::SHOW_STRIPES, user_interface_->showStripes->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::DRAW_SEPARATOR, user_interface_->drawSeparator->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::DRAW_TITLE_OUTLINE, user_interface_->titleOutline->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::USE_OXYGEN_SHADOWS, user_interface_->useOxygenShadows->isChecked() );

    // write exceptions
    user_interface_->exceptions->exceptions().write( *configuration_ );      
    
    // sync configuration
    configuration_->sync();
    
  }
  
  
  //_______________________________________________________________________
  void Config::defaults()
  {
    
    // install default configuration
    loadConfiguration( NitrogenConfiguration() );
    
    // install default exceptions
    user_interface_->exceptions->setExceptions( NitrogenExceptionList::defaultList() );
    
    // emit changed signal
    emit changed();
    
  }

  //_______________________________________________________________________
  void Config::loadConfiguration( const NitrogenConfiguration& configuration )
  {
    
    user_interface_->titleAlignment->setCurrentIndex( user_interface_->titleAlignment->findText( configuration.titleAlignmentName( true ) ) );    
    user_interface_->buttonSize->setCurrentIndex( user_interface_->buttonSize->findText( configuration.buttonSizeName( true ) ) );    
    user_interface_->blendColor->setCurrentIndex( user_interface_->blendColor->findText( configuration.blendColorName( true ) ) );
    user_interface_->frameBorder->setCurrentIndex( user_interface_->frameBorder->findText( configuration.frameBorderName( true ) ) );
    user_interface_->sizeGripMode->setCurrentIndex( user_interface_->sizeGripMode->findText( configuration.sizeGripModeName( true ) ) );

    user_interface_->showStripes->setChecked( configuration.showStripes() );
    user_interface_->drawSeparator->setChecked( configuration.drawSeparator() );
    user_interface_->titleOutline->setChecked( configuration.drawTitleOutline() );
    user_interface_->useOxygenShadows->setChecked( configuration.useOxygenShadows() );
  }
  
  
  //_______________________________________________________________________
  void Config::aboutNitrogen( void )
  {
    KAboutData aboutData( "nitrogen", 0,
      ki18n( "Nitrogen" ), APP_VERSION,
      ki18n( "Oxygen based Window decoration for KDE" ), KAboutData::License_GPL,
      KLocalizedString(),
      KLocalizedString(),
      ( "http://www.kde-look.org/content/show.php/Nitrogen?content=99551" ) );
    
    aboutData.addAuthor( 
      ki18n( "Hugo Pereira Da Costa" ), ki18n( "Developer" ),
      "hugo.pereira@free.fr",
      "http://hugo.pereira.free.fr/index.php" );
    
    aboutData.addCredit( ki18n( "Oxygen team" ) );

    // create dialog
    KAboutApplicationDialog( &aboutData, 0 ).exec();
      
      
  }

} 
