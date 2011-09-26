//////////////////////////////////////////////////////////////////////////////
// oxygenconfiguration.cpp
// decoration configuration
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
#include "oxygenshadowconfiguration.h"
#include "oxygenexception.h"

#include <QTextStream>
#include <KLocale>

namespace Oxygen
{

    //__________________________________________________
    Configuration::Configuration( void ):
        _titleAlignment( Qt::AlignHCenter ),
        _centerTitleOnFullWidth( true ),
        _buttonSize( ButtonDefault ),
        _frameBorder( BorderTiny ),
        _blendColor( BlendFromStyle ),
        _sizeGripMode( SizeGripWhenNeeded ),
        _separatorMode( SeparatorNever ),
        _drawTitleOutline( false ),
        _hideTitleBar( false ),
        _useDropShadows( true ),
        _useOxygenShadows( true ),
        _useNarrowButtonSpacing( false ),
        _animationsEnabled( true ),
        _buttonAnimationsEnabled( true ),
        _titleAnimationsEnabled( true ),
        _shadowAnimationsEnabled( true ),
        _tabAnimationsEnabled( true ),
        _buttonAnimationsDuration( 150 ),
        _titleAnimationsDuration( 150 ),
        _shadowAnimationsDuration( 150 ),
        _tabAnimationsDuration( 150 )
    {}

    //__________________________________________________
    Configuration::Configuration( KConfigGroup group )
    {

        // used to set default values when entries are not found in kconfig
        Configuration defaultConfiguration;

        // title alignment
        setTitleAlignment( titleAlignment(
            group.readEntry( OxygenConfig::TITLE_ALIGNMENT,
            defaultConfiguration.titleAlignmentName( false ) ), false ) );

        // center title on full width
        setCenterTitleOnFullWidth( group.readEntry( OxygenConfig::CENTER_TITLE_ON_FULL_WIDTH,
            defaultConfiguration.centerTitleOnFullWidth() ) );

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

        // separator mode
        if( !group.readEntry( OxygenConfig::DRAW_SEPARATOR, defaultConfiguration.separatorMode() != SeparatorNever ) )
        {

            setSeparatorMode( SeparatorNever );

        } else if( group.readEntry( OxygenConfig::SEPARATOR_ACTIVE_ONLY, defaultConfiguration.separatorMode() == SeparatorActive ) ) {

            setSeparatorMode( SeparatorActive );

        } else setSeparatorMode( SeparatorAlways );

        // title outline
        setDrawTitleOutline( group.readEntry(
            OxygenConfig::DRAW_TITLE_OUTLINE,
            defaultConfiguration.drawTitleOutline() ) );

        // hide title bar
        setHideTitleBar( group.readEntry(
            OxygenConfig::HIDE_TITLEBAR,
            defaultConfiguration.hideTitleBar() ) );

        // drop shadows
        setUseDropShadows( group.readEntry(
            OxygenConfig::USE_DROP_SHADOWS,
            defaultConfiguration.useDropShadows() ) );

        // oxygen shadows
        setUseOxygenShadows( group.readEntry(
            OxygenConfig::USE_OXYGEN_SHADOWS,
            defaultConfiguration.useOxygenShadows() ) );

        // buttonSpacing
        setUseNarrowButtonSpacing( group.readEntry(
            OxygenConfig::NARROW_BUTTON_SPACING,
            defaultConfiguration.useNarrowButtonSpacing() ) );

        // animations
        setAnimationsEnabled( group.readEntry(
            OxygenConfig::ANIMATIONS_ENABLED,
            defaultConfiguration.animationsEnabled() ) );

        setButtonAnimationsEnabled( group.readEntry(
            OxygenConfig::BUTTON_ANIMATIONS_ENABLED,
            defaultConfiguration.buttonAnimationsEnabled() ) );

        setTitleAnimationsEnabled( group.readEntry(
            OxygenConfig::TITLE_ANIMATIONS_ENABLED,
            defaultConfiguration.titleAnimationsEnabled() ) );

        setShadowAnimationsEnabled( group.readEntry(
            OxygenConfig::SHADOW_ANIMATIONS_ENABLED,
            defaultConfiguration.shadowAnimationsEnabled() ) );

        setTabAnimationsEnabled( group.readEntry(
            OxygenConfig::TAB_ANIMATIONS_ENABLED,
            defaultConfiguration.tabAnimationsEnabled() ) );

        // animations duration
        setButtonAnimationsDuration( group.readEntry(
            OxygenConfig::BUTTON_ANIMATIONS_DURATION,
            defaultConfiguration.buttonAnimationsDuration() ) );

        setTitleAnimationsDuration( group.readEntry(
            OxygenConfig::TITLE_ANIMATIONS_DURATION,
            defaultConfiguration.titleAnimationsDuration() ) );

        setShadowAnimationsDuration( group.readEntry(
            OxygenConfig::SHADOW_ANIMATIONS_DURATION,
            defaultConfiguration.shadowAnimationsDuration() ) );

        setTabAnimationsDuration( group.readEntry(
            OxygenConfig::TAB_ANIMATIONS_DURATION,
            defaultConfiguration.tabAnimationsDuration() ) );

    }

    //__________________________________________________
    void Configuration::readException( const Exception& exception )
    {
        // propagate all features found in mask to the output configuration
        if( exception.mask() & Exception::FrameBorder ) setFrameBorder( exception.frameBorder() );
        if( exception.mask() & Exception::BlendColor ) setBlendColor( exception.blendColor() );
        if( exception.mask() & Exception::DrawSeparator ) setSeparatorMode( exception.separatorMode() );
        if( exception.mask() & Exception::TitleOutline ) setDrawTitleOutline( exception.drawTitleOutline() );
        if( exception.mask() & Exception::SizeGripMode ) setSizeGripMode( exception.sizeGripMode() );
        setHideTitleBar( exception.hideTitleBar() );
    }

    //__________________________________________________
    void Configuration::write( KConfigGroup& group ) const
    {

        Configuration defaultConfiguration;

        if( titleAlignment() != defaultConfiguration.titleAlignment() ) group.writeEntry( OxygenConfig::TITLE_ALIGNMENT, titleAlignmentName( false ) );
        if( centerTitleOnFullWidth() != defaultConfiguration.centerTitleOnFullWidth() ) group.writeEntry( OxygenConfig::CENTER_TITLE_ON_FULL_WIDTH, centerTitleOnFullWidth() );
        if( buttonSize() != defaultConfiguration.buttonSize() ) group.writeEntry( OxygenConfig::BUTTON_SIZE, buttonSizeName( false ) );
        if( blendColor() != defaultConfiguration.blendColor() ) group.writeEntry( OxygenConfig::BLEND_COLOR, blendColorName( false ) );
        if( frameBorder() != defaultConfiguration.frameBorder() ) group.writeEntry( OxygenConfig::FRAME_BORDER, frameBorderName( false ) );
        if( sizeGripMode() != defaultConfiguration.sizeGripMode() ) group.writeEntry( OxygenConfig::SIZE_GRIP_MODE, sizeGripModeName( false ) );

        if( separatorMode() != defaultConfiguration.separatorMode() )
        {
            group.writeEntry( OxygenConfig::DRAW_SEPARATOR, separatorMode() != SeparatorNever );
            group.writeEntry( OxygenConfig::SEPARATOR_ACTIVE_ONLY, separatorMode() == SeparatorActive );
        }

        if( drawTitleOutline() != defaultConfiguration.drawTitleOutline() ) group.writeEntry( OxygenConfig::DRAW_TITLE_OUTLINE, drawTitleOutline() );
        if( hideTitleBar() != defaultConfiguration.hideTitleBar() ) group.writeEntry( OxygenConfig::HIDE_TITLEBAR, hideTitleBar() );
        if( useDropShadows() != defaultConfiguration.useDropShadows() ) group.writeEntry( OxygenConfig::USE_DROP_SHADOWS, useDropShadows() );
        if( useOxygenShadows() != defaultConfiguration.useOxygenShadows() ) group.writeEntry( OxygenConfig::USE_OXYGEN_SHADOWS, useOxygenShadows() );
        if( useNarrowButtonSpacing() != defaultConfiguration.useNarrowButtonSpacing() ) group.writeEntry( OxygenConfig::NARROW_BUTTON_SPACING, useNarrowButtonSpacing() );

        // animations
        if( animationsEnabled() != defaultConfiguration.animationsEnabled() ) group.writeEntry( OxygenConfig::ANIMATIONS_ENABLED, animationsEnabled() );
        if( buttonAnimationsEnabled() != defaultConfiguration.buttonAnimationsEnabled() ) group.writeEntry( OxygenConfig::BUTTON_ANIMATIONS_ENABLED, buttonAnimationsEnabled() );
        if( titleAnimationsEnabled() != defaultConfiguration.titleAnimationsEnabled() ) group.writeEntry( OxygenConfig::TITLE_ANIMATIONS_ENABLED, titleAnimationsEnabled() );
        if( shadowAnimationsEnabled() != defaultConfiguration.shadowAnimationsEnabled() ) group.writeEntry( OxygenConfig::SHADOW_ANIMATIONS_ENABLED, shadowAnimationsEnabled() );
        if( tabAnimationsEnabled() != defaultConfiguration.tabAnimationsEnabled() ) group.writeEntry( OxygenConfig::TAB_ANIMATIONS_ENABLED, tabAnimationsEnabled() );

        // animations duration
        if( buttonAnimationsDuration() != defaultConfiguration.buttonAnimationsDuration() ) group.writeEntry( OxygenConfig::BUTTON_ANIMATIONS_DURATION, buttonAnimationsDuration() );
        if( titleAnimationsDuration() != defaultConfiguration.titleAnimationsDuration() ) group.writeEntry( OxygenConfig::TITLE_ANIMATIONS_DURATION, titleAnimationsDuration() );
        if( shadowAnimationsDuration() != defaultConfiguration.shadowAnimationsDuration() ) group.writeEntry( OxygenConfig::SHADOW_ANIMATIONS_DURATION, shadowAnimationsDuration() );
        if( tabAnimationsDuration() != defaultConfiguration.tabAnimationsDuration() ) group.writeEntry( OxygenConfig::TAB_ANIMATIONS_DURATION, tabAnimationsDuration() );

    }

    //__________________________________________________
    QString Configuration::titleAlignmentName( Qt::Alignment value, bool translated, bool fullWidth )
    {
        QString out;
        switch( value )
        {
            case Qt::AlignLeft: out = translated ? i18n( "Left" ):"Left"; break;
            case Qt::AlignHCenter:
            {

                if( fullWidth ) out = translated ? i18n( "Center (Full Width)" ):"Center (Full Width)";
                else out = translated ? i18n( "Center" ):"Center";

            } break;
            case Qt::AlignRight: out = translated ? i18n( "Right" ):"Right"; break;
            default: return Configuration().titleAlignmentName( translated );
        }

        return out;

    }

    //__________________________________________________
    Qt::Alignment Configuration::titleAlignment( QString value, bool translated )
    {
        if (value == titleAlignmentName( Qt::AlignLeft, translated ) ) return Qt::AlignLeft;
        else if (value == titleAlignmentName( Qt::AlignHCenter, translated, false ) || value == titleAlignmentName( Qt::AlignHCenter, translated, true ) ) return Qt::AlignHCenter;
        else if (value == titleAlignmentName( Qt::AlignRight, translated ) ) return Qt::AlignRight;
        else return Configuration().titleAlignment();
    }

    //__________________________________________________
    QString Configuration::buttonSizeName( ButtonSize value, bool translated )
    {
        QString out;
        switch( value )
        {
            case ButtonSmall: out = translated ? i18nc( "@item:inlistbox Button size:", "Small" ):"Small"; break;
            case ButtonDefault: out = translated ? i18nc( "@item:inlistbox Button size:", "Normal" ):"Normal"; break;
            case ButtonLarge: out = translated ? i18nc( "@item:inlistbox Button size:", "Large" ):"Large"; break;
            case ButtonVeryLarge: out = translated ? i18nc( "@item:inlistbox Button size:", "Very Large" ):"Very Large"; break;
            case ButtonHuge: out = translated ? i18nc( "@item:inlistbox Button size:", "Huge" ):"Huge"; break;
            default: return Configuration().buttonSizeName( translated );
        }

        return out;

    }

    //__________________________________________________
    Configuration::ButtonSize Configuration::buttonSize( QString value, bool translated )
    {
        if( value == buttonSizeName( ButtonSmall, translated ) ) return ButtonSmall;
        else if( value == buttonSizeName( ButtonDefault, translated ) ) return ButtonDefault;
        else if( value == buttonSizeName( ButtonLarge, translated ) ) return ButtonLarge;
        else if( value == buttonSizeName( ButtonVeryLarge, translated ) ) return ButtonVeryLarge;
        else if( value == buttonSizeName( ButtonHuge, translated ) ) return ButtonHuge;
        else return Configuration().buttonSize();
    }

    //__________________________________________________
    int Configuration::iconScale( ButtonSize value )
    {
        switch( value )
        {
            case ButtonSmall: return 13;
            case ButtonDefault: return 16;
            case ButtonLarge: return 20;
            case ButtonVeryLarge: return 24;
            case ButtonHuge: return 35;
            default: return Configuration().iconScale();
        }

    }

    //__________________________________________________
    QString Configuration::frameBorderName( FrameBorder value, bool translated )
    {
        QString out;
        switch( value )
        {
            case BorderNone: out = translated ? i18nc( "@item:inlistbox Border size:", "No Border" ):"No Border"; break;
            case BorderNoSide: out = translated ? i18nc( "@item:inlistbox Border size:", "No Side Border" ):"No Side Border"; break;
            case BorderTiny: out = translated ? i18nc( "@item:inlistbox Border size:", "Tiny" ):"Tiny"; break;
            case BorderDefault: out = translated ? i18nc( "@item:inlistbox Border size:", "Normal" ):"Normal"; break;
            case BorderLarge: out = translated ? i18nc( "@item:inlistbox Border size:", "Large" ):"Large"; break;
            case BorderVeryLarge: out = translated ? i18nc( "@item:inlistbox Border size:", "Very Large" ):"Very Large"; break;
            case BorderHuge: out = translated ? i18nc( "@item:inlistbox Border size:", "Huge" ):"Huge"; break;
            case BorderVeryHuge: out = translated ? i18nc( "@item:inlistbox Border size:", "Very Huge" ):"Very Huge"; break;
            case BorderOversized: out = translated ? i18nc( "@item:inlistbox Border size:", "Oversized" ):"Oversized"; break;
            default: return Configuration().frameBorderName( translated );
        }

        return out;

    }

    //__________________________________________________
    Configuration::FrameBorder Configuration::frameBorder( QString value, bool translated )
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
        else return Configuration().frameBorder();
    }

    //__________________________________________________
    QString Configuration::blendColorName( BlendColorType value, bool translated )
    {
        QString out;
        switch( value )
        {
            case NoBlending: out = translated ? i18n( "Solid Color" ):"Solid Color"; break;
            case RadialBlending: out = translated ? i18n( "Radial Gradient" ):"Radial Gradient"; break;
            case BlendFromStyle: out = translated ? i18n( "Follow Style Hint" ):"Follow Style Hint"; break;
            default: return Configuration().blendColorName( translated );
        }

        return out;

    }

    //__________________________________________________
    Configuration::BlendColorType Configuration::blendColor( QString value, bool translated )
    {
        if( value == blendColorName( NoBlending, translated ) ) return NoBlending;
        else if( value == blendColorName( RadialBlending, translated ) ) return RadialBlending;
        else if( value == blendColorName( BlendFromStyle, translated ) ) return BlendFromStyle;
        else return Configuration().blendColor();
    }

    //__________________________________________________
    QString Configuration::sizeGripModeName( SizeGripMode value, bool translated )
    {
        QString out;
        switch( value )
        {
            case SizeGripNever: out = translated ? i18n( "Always Hide Extra Size Grip" ):"Always Hide Extra Size Grip"; break;
            case SizeGripWhenNeeded: out = translated ? i18n( "Show Extra Size Grip When Needed" ):"Show Extra Size Grip When Needed"; break;
            default: return Configuration().sizeGripModeName( translated );
        }

        return out;

    }

    //__________________________________________________
    Configuration::SizeGripMode Configuration::sizeGripMode( QString value, bool translated )
    {
        if( value == sizeGripModeName( SizeGripNever, translated ) ) return SizeGripNever;
        else if( value == sizeGripModeName( SizeGripWhenNeeded, translated ) ) return SizeGripWhenNeeded;
        else return Configuration().sizeGripMode();
    }

    //________________________________________________________
    bool Configuration::operator == (const Configuration& other ) const
    {

        return
            titleAlignment() == other.titleAlignment() &&
            centerTitleOnFullWidth() == other.centerTitleOnFullWidth() &&
            buttonSize() == other.buttonSize() &&
            frameBorder() == other.frameBorder() &&
            blendColor() == other.blendColor() &&
            sizeGripMode() == other.sizeGripMode() &&
            separatorMode() == other.separatorMode() &&
            drawTitleOutline() == other.drawTitleOutline() &&
            hideTitleBar() == other.hideTitleBar() &&
            useDropShadows() == other.useDropShadows() &&
            useOxygenShadows() == other.useOxygenShadows() &&
            useNarrowButtonSpacing() == other.useNarrowButtonSpacing() &&
            animationsEnabled() == other.animationsEnabled() &&
            buttonAnimationsEnabled() == other.buttonAnimationsEnabled() &&
            titleAnimationsEnabled() == other.titleAnimationsEnabled() &&
            shadowAnimationsEnabled() == other.shadowAnimationsEnabled() &&
            tabAnimationsEnabled() == other.tabAnimationsEnabled() &&
            buttonAnimationsDuration() == other.buttonAnimationsDuration() &&
            titleAnimationsDuration() == other.titleAnimationsDuration() &&
            shadowAnimationsDuration() == other.shadowAnimationsDuration() &&
            tabAnimationsDuration() == other.tabAnimationsDuration();

    }

}
