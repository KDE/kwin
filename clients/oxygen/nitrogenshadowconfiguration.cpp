//////////////////////////////////////////////////////////////////////////////
// nitrogenshadowconfiguration.cpp
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

#include <cassert>
#include <kdecoration.h>

#include "nitrogenshadowconfiguration.h"

namespace Nitrogen
{

  //_________________________________________________________
  NitrogenShadowConfiguration::NitrogenShadowConfiguration( QPalette::ColorGroup colorGroup ):
    colorGroup_( colorGroup ),
    shadowSize_( 25.5 ),
    horizontalOffset_( 0 ),
    useOuterColor_( false )
  {

    // check colorgroup
    assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );

    // vertical offset
    verticalOffset_ = ( NitrogenShadowConfiguration::colorGroup() == QPalette::Active ) ? 0:0.2;

    // colors
    innerColor_ = ( NitrogenShadowConfiguration::colorGroup() == QPalette::Active ) ?
      KDecoration::options()->color( KDecorationDefines::ColorTitleBar, true ):
      QColor( Qt::black );

    outerColor_ = outerColor2_ = calcOuterColor();
    midColor_ = calcMidColor();

  }

  //_________________________________________________________
  NitrogenShadowConfiguration::NitrogenShadowConfiguration( QPalette::ColorGroup colorGroup, KConfigGroup group ):
    colorGroup_( colorGroup )
  {

    // get default configuration
    NitrogenShadowConfiguration defaultConfiguration( NitrogenShadowConfiguration::colorGroup() );

    setShadowSize( group.readEntry( NitrogenConfig::SHADOW_SIZE, defaultConfiguration.shadowSize() ) );
    setHorizontalOffset( group.readEntry( NitrogenConfig::SHADOW_HOFFSET, defaultConfiguration.horizontalOffset() ) );
    setVerticalOffset( group.readEntry( NitrogenConfig::SHADOW_VOFFSET, defaultConfiguration.verticalOffset() ) );
    setInnerColor( group.readEntry( NitrogenConfig::SHADOW_INNER_COLOR, defaultConfiguration.innerColor() ) );
    setOuterColor( group.readEntry( NitrogenConfig::SHADOW_OUTER_COLOR, defaultConfiguration.outerColor() ) );
    setUseOuterColor( group.readEntry( NitrogenConfig::SHADOW_USE_OUTER_COLOR, defaultConfiguration.useOuterColor() ) );
    setOuterColor2( calcOuterColor() );
    setMidColor( calcMidColor() );

  }

  //_________________________________________________________
  bool NitrogenShadowConfiguration::operator == ( const NitrogenShadowConfiguration& other ) const
  {
    assert( colorGroup() == other.colorGroup() );
    return
      shadowSize() == other.shadowSize() &&
      horizontalOffset() == other.horizontalOffset() &&
      verticalOffset() == other.verticalOffset() &&
      innerColor() == other.innerColor() &&
      outerColor() == other.outerColor() &&
      useOuterColor() == other.useOuterColor();
  }

  //_________________________________________________________
  void NitrogenShadowConfiguration::write( KConfigGroup& group ) const
  {
    group.writeEntry( NitrogenConfig::SHADOW_SIZE, shadowSize() );
    group.writeEntry( NitrogenConfig::SHADOW_HOFFSET, horizontalOffset() );
    group.writeEntry( NitrogenConfig::SHADOW_VOFFSET, verticalOffset() );
    group.writeEntry( NitrogenConfig::SHADOW_INNER_COLOR, innerColor().name() );
    group.writeEntry( NitrogenConfig::SHADOW_OUTER_COLOR, outerColor().name() );
    group.writeEntry( NitrogenConfig::SHADOW_USE_OUTER_COLOR, useOuterColor() );
  }

  //_________________________________________________________
  void NitrogenShadowConfiguration::setInnerColor( QColor color )
  { innerColor_ = color.isValid() ? color : NitrogenShadowConfiguration( colorGroup() ).innerColor(); }

  //_________________________________________________________
  void NitrogenShadowConfiguration::setMidColor( QColor color )
  { midColor_ = color.isValid() ? color : NitrogenShadowConfiguration( colorGroup() ).midColor(); }

  //_________________________________________________________
  void NitrogenShadowConfiguration::setOuterColor( QColor color )
  { outerColor_ = color.isValid() ? color : NitrogenShadowConfiguration( colorGroup() ).outerColor(); }

  //_________________________________________________________
  void NitrogenShadowConfiguration::setOuterColor2( QColor color )
  { outerColor2_ = color.isValid() ? color : NitrogenShadowConfiguration( colorGroup() ).outerColor2(); }

  //_________________________________________________________
  QColor NitrogenShadowConfiguration::calcOuterColor( void ) const
  {
    QColor innerColor( NitrogenShadowConfiguration::innerColor() );
    assert( innerColor.isValid() );

    // should contain a more ellaborate mathematical formula
    // to calculate outer color from inner color
    return innerColor;
  }

  //_________________________________________________________
  QColor NitrogenShadowConfiguration::calcMidColor( void ) const
  {
    QColor innerColor( NitrogenShadowConfiguration::innerColor() );
    QColor outerColor( NitrogenShadowConfiguration::outerColor() );
    assert( innerColor.isValid() && outerColor.isValid() );

    // should contain a more ellaborate mathematical formula
    // to calculate mid color from inner and outer colors
    return outerColor;
  }

}
