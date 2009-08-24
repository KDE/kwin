/*
* Nitrogen KWin client configuration module
*
* Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
*
* Based on the Quartz configuration module,
*     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the license, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/

// $Id: config.cpp,v 1.25 2009/07/05 20:45:40 hpereira Exp $

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
    configurationGroup.writeEntry( NitrogenConfig::TITLE_ALIGNMENT, user_interface_->titleAlignment->currentText() );
    configurationGroup.writeEntry( NitrogenConfig::BUTTON_SIZE, user_interface_->buttonSize->currentText() );
    configurationGroup.writeEntry( NitrogenConfig::BUTTON_TYPE, user_interface_->buttonType->currentText() );
    configurationGroup.writeEntry( NitrogenConfig::BLEND_COLOR, user_interface_->blendColor->currentText() );
    configurationGroup.writeEntry( NitrogenConfig::FRAME_BORDER, user_interface_->frameBorder->currentText() );
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
    
    user_interface_->titleAlignment->setCurrentIndex( user_interface_->titleAlignment->findText( configuration.titleAlignmentName() ) );    
    user_interface_->buttonSize->setCurrentIndex( user_interface_->buttonSize->findText( configuration.buttonSizeName() ) );    
    user_interface_->buttonType->setCurrentIndex( user_interface_->buttonType->findText( configuration.buttonTypeName() ) );    
    user_interface_->blendColor->setCurrentIndex( user_interface_->blendColor->findText( configuration.blendColorName() ) );
    user_interface_->frameBorder->setCurrentIndex( user_interface_->frameBorder->findText( configuration.frameBorderName() ) );

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
