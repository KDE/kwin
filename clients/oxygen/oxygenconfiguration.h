#ifndef oxygenconfiguration_h
#define oxygenconfiguration_h

//////////////////////////////////////////////////////////////////////////////
// oxygenconfiguration.h
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

#include <KConfigGroup>

namespace OxygenConfig
{

    static const QString TITLE_ALIGNMENT = "TitleAlignment";
    static const QString CENTER_TITLE_ON_FULL_WIDTH = "CenterTitleOnFullWidth";
    static const QString BUTTON_SIZE = "ButtonSize";
    static const QString DRAW_SEPARATOR = "DrawSeparator";
    static const QString SEPARATOR_ACTIVE_ONLY = "SeparatorActiveOnly";
    static const QString DRAW_TITLE_OUTLINE = "DrawTitleOutline";
    static const QString FRAME_BORDER = "FrameBorder";
    static const QString BLEND_COLOR = "BlendColor";
    static const QString SIZE_GRIP_MODE = "SizeGripMode";
    static const QString HIDE_TITLEBAR = "HideTitleBar";
    static const QString ANIMATIONS_ENABLED = "AnimationsEnabled";
    static const QString NARROW_BUTTON_SPACING = "UseNarrowButtonSpacing";

    //!@name animation flags
    //@{
    static const QString BUTTON_ANIMATIONS_ENABLED = "ButtonAnimationsEnabled";
    static const QString TITLE_ANIMATIONS_ENABLED = "TitleAnimationsEnabled";
    static const QString SHADOW_ANIMATIONS_ENABLED = "ShadowAnimationsEnabled";
    static const QString TAB_ANIMATIONS_ENABLED = "TabAnimationsEnabled";
    //@}

    //!@name animations duration
    //@{
    static const QString BUTTON_ANIMATIONS_DURATION = "ButtonAnimationsDuration";
    static const QString TITLE_ANIMATIONS_DURATION = "TitleAnimationsDuration";
    static const QString SHADOW_ANIMATIONS_DURATION = "ShadowAnimationsDuration";
    static const QString TAB_ANIMATIONS_DURATION = "TabAnimationsDuration";
    //@}

}

namespace Oxygen
{

    // forward declaration
    class Exception;

    //! stores all configuration options needed for decoration appearance
    class Configuration
    {

        public:

        //! button size enumeration
        enum ButtonSize {
            ButtonSmall = 18,
            ButtonDefault = 20,
            ButtonLarge = 24,
            ButtonVeryLarge = 32,
            ButtonHuge = 48
        };

        //! frame border enumeration
        enum FrameBorder {
            BorderNone = 0,
            BorderNoSide = 1,
            BorderTiny = 2,
            BorderDefault = 4,
            BorderLarge = 8,
            BorderVeryLarge = 12,
            BorderHuge = 18,
            BorderVeryHuge = 27,
            BorderOversized = 40
        };

        //! blend color enumeration
        enum BlendColorType {
            NoBlending,
            RadialBlending,
            BlendFromStyle
        };

        //! size grip mode
        enum SizeGripMode {
            SizeGripNever,
            SizeGripWhenNeeded
        };

        //! decide when separator is to be drawn
        enum SeparatorMode {

            //! never
            SeparatorNever,

            //! active window only
            SeparatorActive,

            //! always
            SeparatorAlways

        };

        //! default constructor
        Configuration( void );

        //! constructor from KConfig
        Configuration( KConfigGroup );

        //! destructor
        virtual ~Configuration( void )
        {}

        //! equal to operator
        bool operator == ( const Configuration& ) const;

        //! propagate all features found in exception mask to this configuration
        void readException( const Exception& );

        //! write to kconfig group
        virtual void write( KConfigGroup& ) const;

        //!@name title alignment
        //@{

        static QString titleAlignmentName( Qt::Alignment, bool translated, bool fullWidth = false );
        static Qt::Alignment titleAlignment( QString, bool translated );

        virtual Qt::Alignment titleAlignment() const
        { return _titleAlignment; }

        virtual QString titleAlignmentName( bool translated ) const
        { return titleAlignmentName( titleAlignment(), translated, centerTitleOnFullWidth() ); }

        virtual void setTitleAlignment( Qt::Alignment value )
        { _titleAlignment = value; }

        virtual void setTitleAlignment( QString value, bool translated )
        { _titleAlignment = titleAlignment( value, translated ); }

        virtual bool centerTitleOnFullWidth( void ) const
        { return _centerTitleOnFullWidth; }

        virtual void setCenterTitleOnFullWidth( bool value )
        { _centerTitleOnFullWidth = value; }

        //@}

        //!@name button size
        //@{

        static QString buttonSizeName( ButtonSize, bool translated );
        static ButtonSize buttonSize( QString, bool translated );
        static int iconScale( ButtonSize );

        virtual ButtonSize buttonSize( void ) const
        { return _buttonSize; }

        virtual int iconScale( void ) const
        { return iconScale( buttonSize() ); }

        virtual QString buttonSizeName( bool translated ) const
        { return buttonSizeName( buttonSize(), translated ); }

        virtual void setButtonSize( ButtonSize value )
        { _buttonSize = value; }

        virtual bool useNarrowButtonSpacing( void ) const
        { return _useNarrowButtonSpacing; }

        virtual void  setUseNarrowButtonSpacing( bool value )
        { _useNarrowButtonSpacing = value; }

        //@}

        //!@name frame border
        //@{

        static QString frameBorderName( FrameBorder, bool translated );
        static FrameBorder frameBorder( QString, bool translated );

        virtual FrameBorder frameBorder() const
        { return _frameBorder; }

        virtual QString frameBorderName( bool translated ) const
        { return frameBorderName( frameBorder(), translated ); }

        virtual void setFrameBorder( FrameBorder value )
        { _frameBorder = value; }

        virtual void setFrameBorder( QString value, bool translated )
        { _frameBorder = frameBorder( value, translated ); }

        //@}

        //!@name blend color
        //@{

        static QString blendColorName( BlendColorType, bool translated );
        static BlendColorType blendColor( QString, bool translated );

        virtual BlendColorType blendColor( void ) const
        { return _blendColor; }

        virtual QString blendColorName( bool translated ) const
        { return blendColorName( blendColor(), translated ); }

        virtual void setBlendColor( BlendColorType value )
        { _blendColor = value; }

        virtual void setBlendColor( QString value, bool translated )
        { _blendColor = blendColor( value, translated ); }

        //@}

        //!@name size grip
        //@{

        static QString sizeGripModeName( SizeGripMode, bool translated );
        static SizeGripMode sizeGripMode( QString, bool translated );

        virtual SizeGripMode sizeGripMode( void ) const
        { return _sizeGripMode; }

        virtual QString sizeGripModeName( bool translated ) const
        { return sizeGripModeName( sizeGripMode(), translated ); }

        virtual void setSizeGripMode( SizeGripMode value )
        { _sizeGripMode = value; }

        virtual void setSizeGripMode( QString value, bool translated )
        { _sizeGripMode = sizeGripMode( value, translated ); }

        //! draw size grip
        virtual bool drawSizeGrip( void ) const
        { return (sizeGripMode() == SizeGripWhenNeeded && frameBorder() == BorderNone ); }

        //@}

        //! separator
        virtual SeparatorMode separatorMode( void ) const
        { return _separatorMode; }

        //! separator
        virtual void setSeparatorMode( SeparatorMode value )
        { _separatorMode = value; }

        //! title outline
        virtual bool drawTitleOutline( void ) const
        { return _drawTitleOutline; }

        //! title outline
        virtual void setDrawTitleOutline( bool value )
        { _drawTitleOutline = value; }

        //! hide title bar
        virtual bool hideTitleBar( void ) const
        { return _hideTitleBar; }

        //! hide title bar
        virtual void setHideTitleBar( bool value )
        { _hideTitleBar = value; }

        //! drop shadows
        virtual bool useDropShadows( void ) const
        { return _useDropShadows; }

        //! drop shadows
        virtual void setUseDropShadows( bool value )
        { _useDropShadows = value; }

        //! oxygen shadows
        virtual bool useOxygenShadows( void ) const
        { return _useOxygenShadows; }

        //! oxygen shadows
        virtual void setUseOxygenShadows( bool value )
        { _useOxygenShadows = value; }

        //!@name animations
        //@{

        //! global flag
        virtual bool animationsEnabled( void ) const
        { return _animationsEnabled; }

        //! global flag
        virtual void setAnimationsEnabled( bool value )
        { _animationsEnabled = value; }

        //! buttons
        virtual bool buttonAnimationsEnabled( void ) const
        { return _buttonAnimationsEnabled; }

        //! buttons
        virtual void setButtonAnimationsEnabled( bool value )
        { _buttonAnimationsEnabled = value; }

        //! buttons
        virtual int buttonAnimationsDuration( void ) const
        { return _buttonAnimationsDuration; }

        //! buttons
        virtual void setButtonAnimationsDuration( int value )
        { _buttonAnimationsDuration = value; }

        //! titles
        virtual bool titleAnimationsEnabled( void ) const
        { return _titleAnimationsEnabled; }

        //! title
        virtual void setTitleAnimationsEnabled( bool value )
        { _titleAnimationsEnabled = value; }

        //! title
        virtual int titleAnimationsDuration( void ) const
        { return _titleAnimationsDuration; }

        //! title
        virtual void setTitleAnimationsDuration( int value )
        { _titleAnimationsDuration = value; }

        //! shadows
        virtual bool shadowAnimationsEnabled( void ) const
        { return _shadowAnimationsEnabled; }

        //! shadows
        virtual void setShadowAnimationsEnabled( bool value )
        { _shadowAnimationsEnabled = value; }

        //! shadows
        virtual int shadowAnimationsDuration( void ) const
        { return _shadowAnimationsDuration; }

        //! shadows
        virtual void setShadowAnimationsDuration( int value )
        { _shadowAnimationsDuration = value; }

        //! tabs
        virtual bool tabAnimationsEnabled( void ) const
        { return _tabAnimationsEnabled; }

        //! tabs
        virtual void setTabAnimationsEnabled( bool value )
        { _tabAnimationsEnabled = value; }

        //! tabs
        virtual int tabAnimationsDuration( void ) const
        { return _tabAnimationsDuration; }

        //! tabs
        virtual void setTabAnimationsDuration( int value )
        { _tabAnimationsDuration = value; }

        //@}

        private:

        //! title alignment
        Qt::Alignment _titleAlignment;

        //! full width alignment (makes sense only for Center alignment
        bool _centerTitleOnFullWidth;

        //! button size
        ButtonSize _buttonSize;

        //! blend color
        FrameBorder _frameBorder;

        //! frame border
        BlendColorType _blendColor;

        //! size grip mode
        SizeGripMode _sizeGripMode;

        //! separator
        SeparatorMode _separatorMode;

        //! active window title outline
        bool _drawTitleOutline;

        //! hide titlebar completely (but not window border)
        bool _hideTitleBar;

        //! drop shadows
        bool _useDropShadows;

        //! oxygen shadows
        bool _useOxygenShadows;

        //! narrow button spacing
        bool _useNarrowButtonSpacing;

        //!@name animation flags
        //@{
        bool _animationsEnabled;
        bool _buttonAnimationsEnabled;
        bool _titleAnimationsEnabled;
        bool _shadowAnimationsEnabled;
        bool _tabAnimationsEnabled;
        //@}

        //!@name animation durations
        //@{
        int _buttonAnimationsDuration;
        int _titleAnimationsDuration;
        int _shadowAnimationsDuration;
        int _tabAnimationsDuration;
        //@}

    };

}

#endif
