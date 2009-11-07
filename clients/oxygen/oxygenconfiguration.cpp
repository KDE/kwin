//////////////////////////////////////////////////////////////////////////////
// oxygenconfiguration.cpp
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

#include "oxygenconfiguration.h"

#include <KLocale>

namespace Oxygen
{

  //__________________________________________________
  OxygenConfiguration::OxygenConfiguration( void ):
    titleAlignment_( Qt::AlignHCenter ),
    buttonSize_( ButtonDefault ),
    frameBorder_( BorderDefault ),
    blendColor_( RadialBlending ),
    sizeGripMode_( SizeGripWhenNeeded ),
    drawSeparator_( false ),
    drawTitleOutline_( false ),
    hideTitleBar_( false ),
    useOxygenShadows_( true ),
    useAnimations_( true ),
    animationsDuration_( 150 )
  {}

  //__________________________________________________
  OxygenConfiguration::OxygenConfiguration( KConfigGroup group )
  {

    // used to set default values when entries are not found in kconfig
    OxygenConfiguration defaultConfiguration;

    // title alignment
    setTitleAlignment( titleAlignment(
      group.readEntry( OxygenConfig::TITLE_ALIGNMENT,
      defaultConfiguration.titleAlignmentName( false ) ), false ) );

    // button size
    setButtonSize( buttonSize(
      group.readEntry( OxygenConfig::BUTTON_SIZE,
      defaultConfiguration.buttonSizeName( false ) ), false ) );

    // frame border
    setFrameBorder( frameBorder(
      group.readEntry( OxygenConfig::FRAME_BORDER,
      defaultConfiguration.frameBorderName( false ) ), false ) );

    // blend color
    setBlendColor( blendColor(
      group.readEntry( OxygenConfig::BLEND_COLOR,
      defaultConfiguration.blendColorName( false ) ), false ) );

    // size grip
    setSizeGripMode( sizeGripMode(
      group.readEntry( OxygenConfig::SIZE_GRIP_MODE,
      defaultConfiguration.sizeGripModeName( false ) ), false ) );

    // draw separator
    setDrawSeparator( group.readEntry(
      OxygenConfig::DRAW_SEPARATOR,
      defaultConfiguration.drawSeparator() ) );

    // title outline
    setDrawTitleOutline( group.readEntry(
      OxygenConfig::DRAW_TITLE_OUTLINE,
      defaultConfiguration.drawTitleOutline() ) );

    // hide title bar
    setHideTitleBar( group.readEntry(
      OxygenConfig::HIDE_TITLEBAR,
      defaultConfiguration.hideTitleBar() ) );

    // oxygen shadows
    setUseOxygenShadows( group.readEntry(
      OxygenConfig::USE_OXYGEN_SHADOWS,
      defaultConfiguration.useOxygenShadows() ) );

    // animations
    setUseAnimations( group.readEntry(
      OxygenConfig::USE_ANIMATIONS,
      defaultConfiguration.useAnimations() ) );

    // animations
    setAnimationsDuration( group.readEntry(
      OxygenConfig::ANIMATIONS_DURATION,
      defaultConfiguration.animationsDuration() ) );
  }

  //__________________________________________________
  void OxygenConfiguration::write( KConfigGroup& group ) const
  {

    group.writeEntry( OxygenConfig::TITLE_ALIGNMENT, titleAlignmentName( false ) );
    group.writeEntry( OxygenConfig::BUTTON_SIZE, buttonSizeName( false ) );
    group.writeEntry( OxygenConfig::BLEND_COLOR, blendColorName( false ) );
    group.writeEntry( OxygenConfig::FRAME_BORDER, frameBorderName( false ) );
    group.writeEntry( OxygenConfig::SIZE_GRIP_MODE, sizeGripModeName( false ) );

    group.writeEntry( OxygenConfig::DRAW_SEPARATOR, drawSeparator() );
    group.writeEntry( OxygenConfig::DRAW_TITLE_OUTLINE, drawTitleOutline() );
    group.writeEntry( OxygenConfig::HIDE_TITLEBAR, hideTitleBar() );
    group.writeEntry( OxygenConfig::USE_OXYGEN_SHADOWS, useOxygenShadows() );
    group.writeEntry( OxygenConfig::USE_ANIMATIONS, useAnimations() );
    group.writeEntry( OxygenConfig::ANIMATIONS_DURATION, animationsDuration() );
  }

  //__________________________________________________
  QString OxygenConfiguration::titleAlignmentName( Qt::Alignment value, bool translated )
  {
    QString out;
    switch( value )
    {
      case Qt::AlignLeft: out = translated ? i18n( "Left" ):"Left"; break;
      case Qt::AlignHCenter: out = translated ? i18n( "Center" ):"Center"; break;
      case Qt::AlignRight: out = translated ? i18n( "Right" ):"Right"; break;
      default: return OxygenConfiguration().titleAlignmentName( translated );
    }

    return out;

  }

  //__________________________________________________
  Qt::Alignment OxygenConfiguration::titleAlignment( QString value, bool translated )
  {
    if (value == titleAlignmentName( Qt::AlignLeft, translated ) ) return Qt::AlignLeft;
    else if (value == titleAlignmentName( Qt::AlignHCenter, translated ) ) return Qt::AlignHCenter;
    else if (value == titleAlignmentName( Qt::AlignRight, translated ) ) return Qt::AlignRight;
    else return OxygenConfiguration().titleAlignment();
  }

  //__________________________________________________
  QString OxygenConfiguration::buttonSizeName( ButtonSize value, bool translated )
  {
    QString out;
    switch( value )
    {
      case ButtonSmall: out = translated ? i18n( "Small" ):"Small"; break;
      case ButtonDefault: out = translated ? i18n( "Normal" ):"Normal"; break;
      case ButtonLarge: out = translated ? i18n( "Large" ):"Large"; break;
      case ButtonHuge: out = translated ? i18n( "Huge" ):"Huge"; break;
      default: return OxygenConfiguration().buttonSizeName( translated );
    }

    return out;

  }

  //__________________________________________________
  OxygenConfiguration::ButtonSize OxygenConfiguration::buttonSize( QString value, bool translated )
  {
    if( value == buttonSizeName( ButtonSmall, translated ) ) return ButtonSmall;
    else if( value == buttonSizeName( ButtonDefault, translated ) ) return ButtonDefault;
    else if( value == buttonSizeName( ButtonLarge, translated ) ) return ButtonLarge;
    else if( value == buttonSizeName( ButtonHuge, translated ) ) return ButtonHuge;
    else return OxygenConfiguration().buttonSize();
  }

  //__________________________________________________
  int OxygenConfiguration::iconScale( ButtonSize value )
  {
    switch( value )
    {
      case ButtonSmall: return 13;
      case ButtonDefault: return 16;
      case ButtonLarge: return 24;
      case ButtonHuge: return 35;
      default: return OxygenConfiguration().iconScale();
    }

  }

  //__________________________________________________
  QString OxygenConfiguration::frameBorderName( FrameBorder value, bool translated )
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
      default: return OxygenConfiguration().frameBorderName( translated );
    }

    return out;

  }

  //__________________________________________________
  OxygenConfiguration::FrameBorder OxygenConfiguration::frameBorder( QString value, bool translated )
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
      else return OxygenConfiguration().frameBorder();
  }

  //__________________________________________________
  QString OxygenConfiguration::blendColorName( BlendColorType value, bool translated )
  {
    QString out;
    switch( value )
    {
      case NoBlending: out = translated ? i18n( "Solid Color" ):"Solid Color"; break;
      case RadialBlending: out = translated ? i18n( "Radial Gradient" ):"Radial Gradient"; break;
      default: return OxygenConfiguration().blendColorName( translated );
    }

    return out;

  }

  //__________________________________________________
  OxygenConfiguration::BlendColorType OxygenConfiguration::blendColor( QString value, bool translated )
  {
    if( value == blendColorName( NoBlending, translated ) ) return NoBlending;
    else if( value == blendColorName( RadialBlending, translated ) ) return RadialBlending;
    else return OxygenConfiguration().blendColor();
  }

  //__________________________________________________
  QString OxygenConfiguration::sizeGripModeName( SizeGripMode value, bool translated )
  {
    QString out;
    switch( value )
    {
      case SizeGripNever: out = translated ? i18n( "Always Hide Extra Size Grip" ):"Always Hide Extra Size Grip"; break;
      case SizeGripWhenNeeded: out = translated ? i18n( "Show Extra Size Grip When Needed" ):"Show Extra Size Grip When Needed"; break;
      default: return OxygenConfiguration().sizeGripModeName( translated );
    }

    return out;

  }

  //__________________________________________________
  OxygenConfiguration::SizeGripMode OxygenConfiguration::sizeGripMode( QString value, bool translated )
  {
    if( value == sizeGripModeName( SizeGripNever, translated ) ) return SizeGripNever;
    else if( value == sizeGripModeName( SizeGripWhenNeeded, translated ) ) return SizeGripWhenNeeded;
    else return OxygenConfiguration().sizeGripMode();
  }

  //________________________________________________________
  bool OxygenConfiguration::operator == (const OxygenConfiguration& other ) const
  {

    return
      titleAlignment() == other.titleAlignment() &&
      buttonSize() == other.buttonSize() &&
      frameBorder() == other.frameBorder() &&
      blendColor() == other.blendColor() &&
      sizeGripMode() == other.sizeGripMode() &&
      drawSeparator() == other.drawSeparator() &&
      drawTitleOutline() == other.drawTitleOutline() &&
      hideTitleBar() == other.hideTitleBar() &&
      useOxygenShadows() == other.useOxygenShadows() &&
      useAnimations() == other.useAnimations() &&
      animationsDuration() == other.animationsDuration();

  }

}
