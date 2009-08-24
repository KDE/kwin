//////////////////////////////////////////////////////////////////////////////
// config.cpp
// -------------------
// 
// Copyright (c) 2009, 2010 Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

#include "nitrogenexceptionlistdialog.h"
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
    connect( user_interface_->buttonType, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( user_interface_->frameBorder, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( user_interface_->blendColor, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
   
    connect( user_interface_->showStripes, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->drawSeparator, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->overwriteColors, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->drawSizeGrip, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( user_interface_->useOxygenShadows, SIGNAL(clicked()), SIGNAL(changed()) );

    connect( user_interface_->showExceptions, SIGNAL(clicked()), SLOT( showExceptions() ) );
    
    NitrogenConfiguration::checkUseCompiz();
    if( NitrogenConfiguration::useCompiz() ) 
    { user_interface_->useOxygenShadows->setEnabled( false ); }
    
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
    exceptions_.read( *configuration_ );
    
    if( exceptions_.empty() )
    { exceptions_ = NitrogenExceptionList::defaultList(); }
    
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
      NitrogenConfig::BUTTON_TYPE, 
      NitrogenConfiguration::buttonTypeName( NitrogenConfiguration::buttonType( user_interface_->buttonType->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( 
      NitrogenConfig::BLEND_COLOR, 
      NitrogenConfiguration::blendColorName( NitrogenConfiguration::blendColor( user_interface_->blendColor->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( 
      NitrogenConfig::FRAME_BORDER, 
      NitrogenConfiguration::frameBorderName( NitrogenConfiguration::frameBorder( user_interface_->frameBorder->currentText(), true ), false ) );
    
    configurationGroup.writeEntry( NitrogenConfig::SHOW_STRIPES, user_interface_->showStripes->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::DRAW_SEPARATOR, user_interface_->drawSeparator->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::OVERWRITE_COLORS, user_interface_->overwriteColors->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::DRAW_SIZE_GRIP, user_interface_->drawSizeGrip->isChecked() );
    configurationGroup.writeEntry( NitrogenConfig::USE_OXYGEN_SHADOWS, user_interface_->useOxygenShadows->isChecked() );

    // write number of exceptions
    exceptions_.write( *configuration_ );      
    configuration_->sync();
    
  }
  
  
  //_______________________________________________________________________
  void Config::defaults()
  {
    
    // install default configuration
    loadConfiguration( NitrogenConfiguration() );
    
    // install default exceptions
    exceptions_ = NitrogenExceptionList::defaultList();
    
    // emit changed signal
    emit changed();
    
  }

  //_______________________________________________________________________
  void Config::loadConfiguration( const NitrogenConfiguration& configuration )
  {
    
    user_interface_->titleAlignment->setCurrentIndex( user_interface_->titleAlignment->findText( configuration.titleAlignmentName( true ) ) );    
    user_interface_->buttonSize->setCurrentIndex( user_interface_->buttonSize->findText( configuration.buttonSizeName( true ) ) );    
    user_interface_->buttonType->setCurrentIndex( user_interface_->buttonType->findText( configuration.buttonTypeName( true ) ) );    
    user_interface_->blendColor->setCurrentIndex( user_interface_->blendColor->findText( configuration.blendColorName( true ) ) );
    user_interface_->frameBorder->setCurrentIndex( user_interface_->frameBorder->findText( configuration.frameBorderName( true ) ) );

    user_interface_->showStripes->setChecked( configuration.showStripes() );
    user_interface_->drawSeparator->setChecked( configuration.drawSeparator() );
    user_interface_->overwriteColors->setChecked( configuration.overwriteColors() );
    user_interface_->drawSizeGrip->setChecked( configuration.drawSizeGrip() );
    user_interface_->useOxygenShadows->setChecked( configuration.useOxygenShadows() );
  }
  
  //_______________________________________________________________________
  void Config::showExceptions( void )
  {
          
    // get default configuration (from current)
    KConfigGroup configurationGroup( configuration_, "Windeco");
    
    // create dialog
    NitrogenExceptionListDialog dialog( 
      dynamic_cast<QWidget*>(parent()), 
      NitrogenConfiguration( configurationGroup ) );
    
    dialog.setExceptions( exceptions_ );

    if( !dialog.exec() ) return;
    
    // get new exception list from dialog
    NitrogenExceptionList new_exceptions = dialog.exceptions();

    // compare to current.
    if( new_exceptions == exceptions_ ) return;
    else {    
      
      // update and emit change signal if they differ
      exceptions_ = new_exceptions;
      emit changed();
      
    }
    
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
      ki18n( "Hugo Pereira Da Costa" ), ki18n( "Developper" ),
      "hugo.pereira@free.fr",
      "http://hugo.pereira.free.fr/index.php" );
    
    aboutData.addCredit( ki18n( "Oxygen team" ) );

    // create dialog
    KAboutApplicationDialog( &aboutData, 0 ).exec();
      
      
  }
} 
