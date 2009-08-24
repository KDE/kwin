/******************************************************************************
 *                        
 * Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>            
 *                        
 * This is free software; you can redistribute it and/or modify it under the     
 * terms of the GNU General Public License as published by the Free Software     
 * Foundation; either version 2 of the License, or (at your option) any later   
 * version.                            
 *                         
 * This software is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY;  without even the implied warranty of MERCHANTABILITY or         
 * FITNESS FOR A PARTICULAR PURPOSE.   See the GNU General Public License         
 * for more details.                    
 *                         
 * You should have received a copy of the GNU General Public License along with 
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple     
 * Place, Suite 330, Boston, MA   02111-1307 USA                          
 *                        
 *                        
 *******************************************************************************/

/*!
   \file nitrogenconfiguration.cpp
   \brief encapsulated window decoration configuration
   \author Hugo Pereira
   \version $Revision: 1.20 $
   \date $Date: 2009/07/05 20:50:42 $
*/

#include <QDBusMessage>
#include <QDBusConnection>
#include <QProcess>
#include <kdeversion.h>

#include "nitrogenconfiguration.h"

namespace Nitrogen
{
 
  //__________________________________________________
  bool NitrogenConfiguration::useCompiz_;
  
  //__________________________________________________
  NitrogenConfiguration::NitrogenConfiguration( void ):
    titleAlignment_( Qt::AlignLeft ),
    buttonSize_( ButtonDefault ),
    buttonType_( ButtonKde43 ),
    frameBorder_( BorderDefault ),
    blendColor_( RadialBlending ),
    showStripes_( false ),
    drawSeparator_( false ),
    overwriteColors_( true ),
    drawSizeGrip_( false ),
    useOxygenShadows_( true )
  {}

  //__________________________________________________
  NitrogenConfiguration::NitrogenConfiguration( KConfigGroup group )
  { 
    
    // used to set default values when entries are not found in kconfig
    NitrogenConfiguration defaultConfiguration;
    
    // title alignment
    setTitleAlignment( titleAlignment( 
      group.readEntry( NitrogenConfig::TITLE_ALIGNMENT, 
      defaultConfiguration.titleAlignmentName() ) ) );

    // button size
    setButtonSize( buttonSize( 
      group.readEntry( NitrogenConfig::BUTTON_SIZE,
      defaultConfiguration.buttonSizeName() ) ) );

    // button type
    setButtonType( buttonType( 
      group.readEntry( NitrogenConfig::BUTTON_TYPE,
      defaultConfiguration.buttonTypeName() ) ) );
    
    // frame border
    setFrameBorder( frameBorder( 
      group.readEntry( NitrogenConfig::FRAME_BORDER, 
      defaultConfiguration.frameBorderName() ) ) );
    
    // blend color
    setBlendColor( blendColor(
      group.readEntry( NitrogenConfig::BLEND_COLOR, 
      defaultConfiguration.blendColorName() ) ) );
    
    // show stripes
    setShowStripes( group.readEntry( 
      NitrogenConfig::SHOW_STRIPES, 
      defaultConfiguration.showStripes() ) );
    
    // draw separator
    setDrawSeparator( group.readEntry( 
      NitrogenConfig::DRAW_SEPARATOR, 
      defaultConfiguration.drawSeparator() ) );
    
    // overwrite color
    setOverwriteColors( group.readEntry( 
      NitrogenConfig::OVERWRITE_COLORS, 
      defaultConfiguration.overwriteColors() ) );
    
    // size grip
    setDrawSizeGrip( group.readEntry( 
      NitrogenConfig::DRAW_SIZE_GRIP, 
      defaultConfiguration.drawSizeGrip() ) );
    
    // oxygen shadows
    setUseOxygenShadows( group.readEntry(
      NitrogenConfig::USE_OXYGEN_SHADOWS,
      defaultConfiguration.useOxygenShadows() ) );
  }
  
  //__________________________________________________
  bool NitrogenConfiguration::checkUseCompiz( void )
  {
    
    #if KDE_IS_VERSION(4,2,92)
    // for some reason, org.kde.compiz does not appear
    // in dbus list with kde 4.3
    QProcess p;
    p.start( "ps", QStringList() << "-A" );
    p.waitForFinished();
    QString output( p.readAll() );
    return ( useCompiz_ =  ( output.indexOf( QRegExp( "\\b(compiz)\\b" ) ) >= 0 ) );
    #else
    QDBusMessage message = QDBusMessage::createMethodCall( 
      "org.freedesktop.compiz", 
      "/org/freedesktop/compiz", 
      "org.freedesktop.compiz", 
      "getPlugins" );
    QDBusMessage reply = QDBusConnection::sessionBus().call( message );
    return ( useCompiz_ = ( reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().empty() ) );
    #endif
  }
  
  //__________________________________________________
  void NitrogenConfiguration::write( KConfigGroup& group ) const
  {
    
    group.writeEntry( NitrogenConfig::TITLE_ALIGNMENT, titleAlignmentName() );
    group.writeEntry( NitrogenConfig::BUTTON_SIZE, buttonSizeName() );
    group.writeEntry( NitrogenConfig::BUTTON_TYPE, buttonTypeName() );
    group.writeEntry( NitrogenConfig::BLEND_COLOR, blendColorName() );
    group.writeEntry( NitrogenConfig::FRAME_BORDER, frameBorderName() );
    group.writeEntry( NitrogenConfig::SHOW_STRIPES, showStripes() );
    group.writeEntry( NitrogenConfig::DRAW_SEPARATOR, drawSeparator() );
    group.writeEntry( NitrogenConfig::OVERWRITE_COLORS, overwriteColors() );
    group.writeEntry( NitrogenConfig::DRAW_SIZE_GRIP, drawSizeGrip() );
    group.writeEntry( NitrogenConfig::USE_OXYGEN_SHADOWS, useOxygenShadows() );
  }
    
  //__________________________________________________
  QString NitrogenConfiguration::titleAlignmentName( Qt::Alignment value )
  {
    switch( value )
    {
      case Qt::AlignLeft: return "Left";
      case Qt::AlignHCenter: return "Center";
      case Qt::AlignRight: return "Right";
      default: return NitrogenConfiguration().titleAlignmentName();
    }
    
    return QString();
    
  }
  
  //__________________________________________________
  Qt::Alignment NitrogenConfiguration::titleAlignment( const QString& value )
  {
    if (value == "Left") return Qt::AlignLeft;
    else if (value == "Center") return Qt::AlignHCenter;
    else if (value == "Right") return Qt::AlignRight;
    else return NitrogenConfiguration().titleAlignment();
  }
  
  //__________________________________________________
  QString NitrogenConfiguration::buttonSizeName( ButtonSize value )
  {
    switch( value )
    {
      case ButtonSmall: return "Small";
      case ButtonDefault: return "Default";
      case ButtonLarge: return "Large";
      case ButtonHuge: return "Huge";
      default: return NitrogenConfiguration().buttonSizeName();
    }
    
    return QString();
    
  }

  //__________________________________________________
  NitrogenConfiguration::ButtonSize NitrogenConfiguration::buttonSize( QString value )
  {
    if( value == "Small" ) return ButtonSmall;
    else if( value == "Default" ) return ButtonDefault;
    else if( value == "Large" ) return ButtonLarge;
    else if( value == "Huge" ) return ButtonHuge;
    else return NitrogenConfiguration().buttonSize();
  }

  //__________________________________________________
  QString NitrogenConfiguration::buttonTypeName( ButtonType value )
  {
    switch( value )
    {
      case ButtonKde42: return "KDE 4.2";
      case ButtonKde43: return "KDE 4.3";
      default: return NitrogenConfiguration().buttonTypeName();
    }
    
    return QString();
    
  }

  //__________________________________________________
  NitrogenConfiguration::ButtonType NitrogenConfiguration::buttonType( QString value )
  {
    if( value == "KDE 4.2" ) return ButtonKde42;
    else if( value == "KDE 4.3" ) return ButtonKde43;
    else return NitrogenConfiguration().buttonType();
  }

  //__________________________________________________
  QString NitrogenConfiguration::frameBorderName( FrameBorder value )
  {
    switch( value )
    {
      case BorderNone: return "No Border";
      case BorderTiny: return "Tiny";
      case BorderSmall: return "Small";
      case BorderDefault: return "Default";
      case BorderLarge: return "Large";
      default: return NitrogenConfiguration().frameBorderName();
    }
    
    return QString();
    
  }
  
  //__________________________________________________
  NitrogenConfiguration::FrameBorder NitrogenConfiguration::frameBorder( QString value )
  {
      if( value == "No Border" ) return BorderNone;
      else if( value == "Tiny" ) return BorderTiny;
      else if( value == "Small" ) return BorderSmall;
      else if( value == "Default" ) return BorderDefault;
      else if( value == "Large" ) return BorderLarge;
      else return NitrogenConfiguration().frameBorder();
  }
  
  //__________________________________________________
  QString NitrogenConfiguration::blendColorName( BlendColorType value )
  {
    switch( value )
    {
      case NoBlending: return "No Blending";
      case RadialBlending: return "Radial Blending";
      default: return NitrogenConfiguration().blendColorName();
    }
    
    return QString();
  }
  
  //__________________________________________________
  NitrogenConfiguration::BlendColorType NitrogenConfiguration::blendColor( QString value )
  {
    if( value == "No Blending" ) return NoBlending;
    else if( value == "Radial Blending" ) return RadialBlending;
    else if( value == "Window Contents" ) return RadialBlending;
    else return NitrogenConfiguration().blendColor();
  }
  
  
  //________________________________________________________
  bool NitrogenConfiguration::operator == (const NitrogenConfiguration& other ) const
  {
    
    return
      titleAlignment() == other.titleAlignment() &&
      buttonSize() == other.buttonSize() &&
      buttonType() == other.buttonType() &&
      frameBorder() == other.frameBorder() &&
      blendColor() == other.blendColor() &&
      showStripes() == other.showStripes() &&
      drawSeparator() == other.drawSeparator() &&
      overwriteColors() == other.overwriteColors() &&
      drawSizeGrip() == other.drawSizeGrip() &&
      useOxygenShadows() == other.useOxygenShadows();
  }

}
