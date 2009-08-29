//////////////////////////////////////////////////////////////////////////////
// nitrogenconfiguration.cpp
// -------------------
// 
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
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
#include <KLocale>

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
    showStripes_( true ),
    drawSeparator_( true ),
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
      defaultConfiguration.titleAlignmentName( false ) ), false ) );

    // button size
    setButtonSize( buttonSize( 
      group.readEntry( NitrogenConfig::BUTTON_SIZE,
      defaultConfiguration.buttonSizeName( false ) ), false ) );

    // button type
    setButtonType( buttonType( 
      group.readEntry( NitrogenConfig::BUTTON_TYPE,
      defaultConfiguration.buttonTypeName( false ) ), false ) );
    
    // frame border
    setFrameBorder( frameBorder( 
      group.readEntry( NitrogenConfig::FRAME_BORDER, 
      defaultConfiguration.frameBorderName( false ) ), false ) );
    
    // blend color
    setBlendColor( blendColor(
      group.readEntry( NitrogenConfig::BLEND_COLOR, 
      defaultConfiguration.blendColorName( false ) ), false ) );
    
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
    
    // for some reason, org.kde.compiz does not appear
    // in dbus list with kde 4.3
    QProcess p;
    p.start( "ps", QStringList() << "-A" );
    p.waitForFinished();
    QString output( p.readAll() );
    return ( useCompiz_ =  ( output.indexOf( QRegExp( "\\b(compiz)\\b" ) ) >= 0 ) );

  }
  
  //__________________________________________________
  void NitrogenConfiguration::write( KConfigGroup& group ) const
  {
    
    group.writeEntry( NitrogenConfig::TITLE_ALIGNMENT, titleAlignmentName( false ) );
    group.writeEntry( NitrogenConfig::BUTTON_SIZE, buttonSizeName( false ) );
    group.writeEntry( NitrogenConfig::BUTTON_TYPE, buttonTypeName( false ) );
    group.writeEntry( NitrogenConfig::BLEND_COLOR, blendColorName( false ) );
    group.writeEntry( NitrogenConfig::FRAME_BORDER, frameBorderName( false ) );
    
    group.writeEntry( NitrogenConfig::SHOW_STRIPES, showStripes() );
    group.writeEntry( NitrogenConfig::DRAW_SEPARATOR, drawSeparator() );
    group.writeEntry( NitrogenConfig::OVERWRITE_COLORS, overwriteColors() );
    group.writeEntry( NitrogenConfig::DRAW_SIZE_GRIP, drawSizeGrip() );
    group.writeEntry( NitrogenConfig::USE_OXYGEN_SHADOWS, useOxygenShadows() );
    
  }
    
  //__________________________________________________
  QString NitrogenConfiguration::titleAlignmentName( Qt::Alignment value, bool translated )
  {
    const char* out;
    switch( value )
    {
      case Qt::AlignLeft: out = "Left"; break;
      case Qt::AlignHCenter: out = "Center"; break;
      case Qt::AlignRight: out = "Right"; break;
      default: return NitrogenConfiguration().titleAlignmentName( translated );
    }
    
    return translated ? i18n(out):out;
    
  }
  
  //__________________________________________________
  Qt::Alignment NitrogenConfiguration::titleAlignment( QString value, bool translated )
  {
    if (value == titleAlignmentName( Qt::AlignLeft, translated ) ) return Qt::AlignLeft;
    else if (value == titleAlignmentName( Qt::AlignHCenter, translated ) ) return Qt::AlignHCenter;
    else if (value == titleAlignmentName( Qt::AlignRight, translated ) ) return Qt::AlignRight;
    else return NitrogenConfiguration().titleAlignment();
  }
  
  //__________________________________________________
  QString NitrogenConfiguration::buttonSizeName( ButtonSize value, bool translated )
  {
    const char* out;
    switch( value )
    {
      case ButtonSmall: out = "Small"; break;
      case ButtonDefault: out = "Default"; break;
      case ButtonLarge: out = "Large"; break;
      case ButtonHuge: out = "Huge"; break;
      default: return NitrogenConfiguration().buttonSizeName( translated );
    }
    
    return translated ? i18n(out):out;
    
  }

  //__________________________________________________
  NitrogenConfiguration::ButtonSize NitrogenConfiguration::buttonSize( QString value, bool translated )
  {
    if( value == buttonSizeName( ButtonSmall, translated ) ) return ButtonSmall;
    else if( value == buttonSizeName( ButtonDefault, translated ) ) return ButtonDefault;
    else if( value == buttonSizeName( ButtonLarge, translated ) ) return ButtonLarge;
    else if( value == buttonSizeName( ButtonHuge, translated ) ) return ButtonHuge;
    else return NitrogenConfiguration().buttonSize();
  }

  //__________________________________________________
  QString NitrogenConfiguration::buttonTypeName( ButtonType value, bool translated )
  {
    const char* out;
    switch( value )
    {
      case ButtonKde42: out = "KDE 4.2"; break;
      case ButtonKde43: out = "Default"; break;
      default: return NitrogenConfiguration().buttonTypeName( translated );
    }
    
    return translated ? i18n(out):out;
    
  }

  //__________________________________________________
  NitrogenConfiguration::ButtonType NitrogenConfiguration::buttonType( QString value, bool translated )
  {
    if( value == buttonTypeName( ButtonKde42, translated ) ) return ButtonKde42;
    else if( value == buttonTypeName( ButtonKde43, translated ) ) return ButtonKde43;
    else return NitrogenConfiguration().buttonType();
  }

  //__________________________________________________
  QString NitrogenConfiguration::frameBorderName( FrameBorder value, bool translated )
  {
    const char* out;
    switch( value )
    {
      case BorderNone: out = "No Border"; break;
      case BorderTiny: out = "Tiny"; break;
      case BorderDefault: out = "Normal"; break;
      case BorderLarge: out = "Large"; break;
      case BorderVeryLarge: out = "Very Large"; break;
      case BorderHuge: out = "Huge"; break;
      case BorderVeryHuge: out = "Very Huge"; break;
      case BorderOversized: out = "Oversized"; break;
      default: return NitrogenConfiguration().frameBorderName( translated );
    }
    
    return translated ? i18n(out):out;
    
  }
  
  //__________________________________________________
  NitrogenConfiguration::FrameBorder NitrogenConfiguration::frameBorder( QString value, bool translated )
  {
      if( value == frameBorderName( BorderNone, translated ) ) return BorderNone;
      else if( value == frameBorderName( BorderTiny, translated ) ) return BorderTiny;
      else if( value == frameBorderName( BorderDefault, translated ) ) return BorderDefault;
      else if( value == frameBorderName( BorderLarge, translated ) ) return BorderLarge;
      else if( value == frameBorderName( BorderVeryLarge, translated ) ) return BorderVeryLarge;
      else if( value == frameBorderName( BorderHuge, translated ) ) return BorderHuge;
      else if( value == frameBorderName( BorderVeryHuge, translated ) ) return BorderVeryHuge;
      else if( value == frameBorderName( BorderOversized, translated ) ) return BorderOversized;
      else return NitrogenConfiguration().frameBorder();
  }
  
  //__________________________________________________
  QString NitrogenConfiguration::blendColorName( BlendColorType value, bool translated )
  {
    const char* out;
    switch( value )
    {
      case NoBlending: out = "No Blending"; break;
      case RadialBlending: out = "Radial Blending"; break;
      default: return NitrogenConfiguration().blendColorName( translated );
    }
    
    return translated ? i18n(out):out;
    
  }
  
  //__________________________________________________
  NitrogenConfiguration::BlendColorType NitrogenConfiguration::blendColor( QString value, bool translated )
  {
    if( value == blendColorName( NoBlending, translated ) ) return NoBlending;
    else if( value == blendColorName( RadialBlending, translated ) ) return RadialBlending;
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
