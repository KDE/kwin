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

#include "nitrogenconfiguration.h"

namespace Nitrogen
{

  //__________________________________________________
  NitrogenConfiguration::NitrogenConfiguration( void ):
    titleAlignment_( Qt::AlignLeft ),
    buttonSize_( ButtonDefault ),
    frameBorder_( BorderDefault ),
    blendColor_( RadialBlending ),
    sizeGripMode_( SizeGripWhenNeeded ),
    drawSeparator_( true ),
    drawTitleOutline_( false ),
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

    // frame border
    setFrameBorder( frameBorder(
      group.readEntry( NitrogenConfig::FRAME_BORDER,
      defaultConfiguration.frameBorderName( false ) ), false ) );

    // blend color
    setBlendColor( blendColor(
      group.readEntry( NitrogenConfig::BLEND_COLOR,
      defaultConfiguration.blendColorName( false ) ), false ) );

    // size grip
    setSizeGripMode( sizeGripMode(
      group.readEntry( NitrogenConfig::SIZE_GRIP_MODE,
      defaultConfiguration.sizeGripModeName( false ) ), false ) );

    // draw separator
    setDrawSeparator( group.readEntry(
      NitrogenConfig::DRAW_SEPARATOR,
      defaultConfiguration.drawSeparator() ) );

    // title outline
    setDrawTitleOutline( group.readEntry(
      NitrogenConfig::DRAW_TITLE_OUTLINE,
      defaultConfiguration.drawTitleOutline() ) );

    // oxygen shadows
    setUseOxygenShadows( group.readEntry(
      NitrogenConfig::USE_OXYGEN_SHADOWS,
      defaultConfiguration.useOxygenShadows() ) );
  }

  //__________________________________________________
  void NitrogenConfiguration::write( KConfigGroup& group ) const
  {

    group.writeEntry( NitrogenConfig::TITLE_ALIGNMENT, titleAlignmentName( false ) );
    group.writeEntry( NitrogenConfig::BUTTON_SIZE, buttonSizeName( false ) );
    group.writeEntry( NitrogenConfig::BLEND_COLOR, blendColorName( false ) );
    group.writeEntry( NitrogenConfig::FRAME_BORDER, frameBorderName( false ) );
    group.writeEntry( NitrogenConfig::SIZE_GRIP_MODE, sizeGripModeName( false ) );

    group.writeEntry( NitrogenConfig::DRAW_SEPARATOR, drawSeparator() );
    group.writeEntry( NitrogenConfig::DRAW_TITLE_OUTLINE, drawTitleOutline() );
    group.writeEntry( NitrogenConfig::USE_OXYGEN_SHADOWS, useOxygenShadows() );

  }

  //__________________________________________________
  QString NitrogenConfiguration::titleAlignmentName( Qt::Alignment value, bool translated )
  {
    QString out;
    switch( value )
    {
      case Qt::AlignLeft: out = translated ? i18n( "Left" ):"Left"; break;
      case Qt::AlignHCenter: out = translated ? i18n( "Center" ):"Center"; break;
      case Qt::AlignRight: out = translated ? i18n( "Right" ):"Right"; break;
      default: return NitrogenConfiguration().titleAlignmentName( translated );
    }

    return out;

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
    QString out;
    switch( value )
    {
      case ButtonSmall: out = translated ? i18n( "Small" ):"Small"; break;
      case ButtonDefault: out = translated ? i18n( "Normal" ):"Normal"; break;
      case ButtonLarge: out = translated ? i18n( "Large" ):"Large"; break;
      case ButtonHuge: out = translated ? i18n( "Huge" ):"Huge"; break;
      default: return NitrogenConfiguration().buttonSizeName( translated );
    }

    return out;

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
  int NitrogenConfiguration::iconScale( ButtonSize value )
  {
    switch( value )
    {
      case ButtonSmall: return 13;
      case ButtonDefault: return 16;
      case ButtonLarge: return 24;
      case ButtonHuge: return 35;
      default: return NitrogenConfiguration().iconScale();
    }

  }

  //__________________________________________________
  QString NitrogenConfiguration::frameBorderName( FrameBorder value, bool translated )
  {
    QString out;
    switch( value )
    {
      case BorderNone: out = translated ? i18n( "No Border" ):"No Border"; break;
      case BorderNoSide: out = translated ? i18n( "No Side Border" ):"No Side Border"; break;
      case BorderTiny: out = translated ? i18n( "Tiny" ):"Tiny"; break;
      case BorderDefault: out = translated ? i18n( "Normal" ):"Normal"; break;
      case BorderLarge: out = translated ? i18n( "Large" ):"Large"; break;
      case BorderVeryLarge: out = translated ? i18n( "Very Large" ):"Very Large"; break;
      case BorderHuge: out = translated ? i18n( "Huge" ):"Huge"; break;
      case BorderVeryHuge: out = translated ? i18n( "Very Huge" ):"Very Huge"; break;
      case BorderOversized: out = translated ? i18n( "Oversized" ):"Oversized"; break;
      default: return NitrogenConfiguration().frameBorderName( translated );
    }

    return out;

  }

  //__________________________________________________
  NitrogenConfiguration::FrameBorder NitrogenConfiguration::frameBorder( QString value, bool translated )
  {
      if( value == frameBorderName( BorderNone, translated ) ) return BorderNone;
      else if( value == frameBorderName( BorderNoSide, translated ) ) return BorderNoSide;
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
    QString out;
    switch( value )
    {
      case NoBlending: out = translated ? i18n( "Solid Color" ):"Solid Color"; break;
      case RadialBlending: out = translated ? i18n( "Radial Gradient" ):"Radial Gradient"; break;
      default: return NitrogenConfiguration().blendColorName( translated );
    }

    return out;

  }

  //__________________________________________________
  NitrogenConfiguration::BlendColorType NitrogenConfiguration::blendColor( QString value, bool translated )
  {
    if( value == blendColorName( NoBlending, translated ) ) return NoBlending;
    else if( value == blendColorName( RadialBlending, translated ) ) return RadialBlending;
    else return NitrogenConfiguration().blendColor();
  }

  //__________________________________________________
  QString NitrogenConfiguration::sizeGripModeName( SizeGripMode value, bool translated )
  {
    QString out;
    switch( value )
    {
      case SizeGripNever: out = translated ? i18n( "Always Hide Extra Size Grip" ):"Always Hide Extra Size Grip"; break;
      case SizeGripWhenNeeded: out = translated ? i18n( "Show Extra Size Grip When Needed" ):"Show Extra Size Grip When Needed"; break;
      default: return NitrogenConfiguration().sizeGripModeName( translated );
    }

    return out;

  }

  //__________________________________________________
  NitrogenConfiguration::SizeGripMode NitrogenConfiguration::sizeGripMode( QString value, bool translated )
  {
    if( value == sizeGripModeName( SizeGripNever, translated ) ) return SizeGripNever;
    else if( value == sizeGripModeName( SizeGripWhenNeeded, translated ) ) return SizeGripWhenNeeded;
    else return NitrogenConfiguration().sizeGripMode();
  }

  //________________________________________________________
  bool NitrogenConfiguration::operator == (const NitrogenConfiguration& other ) const
  {

    return
      titleAlignment() == other.titleAlignment() &&
      buttonSize() == other.buttonSize() &&
      frameBorder() == other.frameBorder() &&
      blendColor() == other.blendColor() &&
      sizeGripMode() == other.sizeGripMode() &&
      drawSeparator() == other.drawSeparator() &&
      drawTitleOutline() == other.drawTitleOutline() &&
      useOxygenShadows() == other.useOxygenShadows();
  }

}
