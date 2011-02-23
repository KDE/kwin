//////////////////////////////////////////////////////////////////////////////
// oxygenshadowconfiguration.cpp
// shadow configuration
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

#include "oxygenshadowconfiguration.h"

#include <cassert>

namespace Oxygen
{

    //_________________________________________________________
    ShadowConfiguration::ShadowConfiguration( QPalette::ColorGroup colorGroup ):
        colorGroup_( colorGroup ),
        enabled_( true )
    {

        // check colorgroup
        assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );

        if( colorGroup == QPalette::Active )
        {

            shadowSize_ = 40;
            horizontalOffset_ = 0;
            verticalOffset_ = 0.1;

            innerColor_ = QColor( "#70EFFF" );
            outerColor_ = QColor( "#54A7F0" );
            outerColor2_ = calcOuterColor();
            midColor_ = calcMidColor();
            useOuterColor_ = true;

        } else {

            shadowSize_ = 40;
            horizontalOffset_ = 0;
            verticalOffset_ = 0.2;

            innerColor_ = QColor( Qt::black );
            outerColor_ = outerColor2_ = calcOuterColor();
            midColor_ = calcMidColor();
            useOuterColor_ = false;

        }

    }

    //_________________________________________________________
    ShadowConfiguration::ShadowConfiguration( QPalette::ColorGroup colorGroup, KConfigGroup group ):
        colorGroup_( colorGroup ),
        enabled_( true )
    {

        // get default configuration
        ShadowConfiguration defaultConfiguration( ShadowConfiguration::colorGroup() );

        setShadowSize( group.readEntry( OxygenConfig::SHADOW_SIZE, defaultConfiguration.shadowSize() ) );
        setHorizontalOffset( group.readEntry( OxygenConfig::SHADOW_HOFFSET, defaultConfiguration.horizontalOffset() ) );
        setVerticalOffset( group.readEntry( OxygenConfig::SHADOW_VOFFSET, defaultConfiguration.verticalOffset() ) );
        setInnerColor( group.readEntry( OxygenConfig::SHADOW_INNER_COLOR, defaultConfiguration.innerColor() ) );
        setOuterColor( group.readEntry( OxygenConfig::SHADOW_OUTER_COLOR, defaultConfiguration.outerColor() ) );
        setUseOuterColor( group.readEntry( OxygenConfig::SHADOW_USE_OUTER_COLOR, defaultConfiguration.useOuterColor() ) );
        setOuterColor2( calcOuterColor() );
        setMidColor( calcMidColor() );

    }

    //_________________________________________________________
    void ShadowConfiguration::write( KConfigGroup& group ) const
    {
        ShadowConfiguration defaultConfiguration( colorGroup_ );
        if( shadowSize() != defaultConfiguration.shadowSize() ) group.writeEntry( OxygenConfig::SHADOW_SIZE, shadowSize() );
        if( horizontalOffset() != defaultConfiguration.horizontalOffset() ) group.writeEntry( OxygenConfig::SHADOW_HOFFSET, horizontalOffset() );
        if( verticalOffset() != defaultConfiguration.verticalOffset() ) group.writeEntry( OxygenConfig::SHADOW_VOFFSET, verticalOffset() );
        if( innerColor() != defaultConfiguration.innerColor() ) group.writeEntry( OxygenConfig::SHADOW_INNER_COLOR, innerColor().name() );
        if( outerColor() != defaultConfiguration.outerColor() ) group.writeEntry( OxygenConfig::SHADOW_OUTER_COLOR, outerColor().name() );
        if( useOuterColor() != defaultConfiguration.useOuterColor() ) group.writeEntry( OxygenConfig::SHADOW_USE_OUTER_COLOR, useOuterColor() );
    }

    //_________________________________________________________
    void ShadowConfiguration::setInnerColor( QColor color )
    { innerColor_ = color.isValid() ? color : ShadowConfiguration( colorGroup() ).innerColor(); }

    //_________________________________________________________
    void ShadowConfiguration::setMidColor( QColor color )
    { midColor_ = color.isValid() ? color : ShadowConfiguration( colorGroup() ).midColor(); }

    //_________________________________________________________
    void ShadowConfiguration::setOuterColor( QColor color )
    { outerColor_ = color.isValid() ? color : ShadowConfiguration( colorGroup() ).outerColor(); }

    //_________________________________________________________
    void ShadowConfiguration::setOuterColor2( QColor color )
    { outerColor2_ = color.isValid() ? color : ShadowConfiguration( colorGroup() ).outerColor2(); }

    //_________________________________________________________
    QColor ShadowConfiguration::calcOuterColor( void ) const
    {
        QColor innerColor( ShadowConfiguration::innerColor() );
        assert( innerColor.isValid() );

        // should contain a more ellaborate mathematical formula
        // to calculate outer color from inner color
        return innerColor;
    }

    //_________________________________________________________
    QColor ShadowConfiguration::calcMidColor( void ) const
    {
        QColor innerColor( ShadowConfiguration::innerColor() );
        QColor outerColor( ShadowConfiguration::outerColor() );
        assert( innerColor.isValid() && outerColor.isValid() );

        // should contain a more ellaborate mathematical formula
        // to calculate mid color from inner and outer colors
        return outerColor;
    }

}
