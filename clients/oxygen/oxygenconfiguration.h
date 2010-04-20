#ifndef oxygenconfiguration_h
#define oxygenconfiguration_h

//////////////////////////////////////////////////////////////////////////////
// oxygenconfiguration.h
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
  static const QString BUTTON_SIZE = "ButtonSize";
  static const QString DRAW_SEPARATOR = "DrawSeparator";
  static const QString DRAW_TITLE_OUTLINE = "DrawTitleOutline";
  static const QString FRAME_BORDER = "FrameBorder";
  static const QString BLEND_COLOR = "BlendColor";
  static const QString SIZE_GRIP_MODE = "SizeGripMode";
  static const QString USE_DROP_SHADOWS = "UseDropShadows";
  static const QString USE_OXYGEN_SHADOWS = "UseOxygenShadows";
  static const QString HIDE_TITLEBAR = "HideTitleBar";
  static const QString USE_ANIMATIONS = "UseAnimations";
  static const QString ANIMATE_TITLE_CHANGE = "AnimateTitleChange";
  static const QString ANIMATIONS_DURATION = "AnimationsDuration";
  static const QString TABS_ENABLED = "TabsEnabled";
  static const QString NARROW_BUTTON_SPACING = "UseNarrowButtonSpacing";
  static const QString SHADOW_CACHE_MODE = "ShadowCacheMode";
}

namespace Oxygen
{

  class OxygenConfiguration
  {

    public:

    //! button size enumeration
    enum ButtonSize
    {
      ButtonSmall = 18,
      ButtonDefault = 20,
      ButtonLarge = 32,
      ButtonHuge = 48
    };

    //! frame border enumeration
    enum FrameBorder
    {
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
      RadialBlending
    };

    //! size grip mode
    enum SizeGripMode
    {
      SizeGripNever,
      SizeGripWhenNeeded
    };

    //! cache mode
    enum ShadowCacheMode
    {
      // no shadow cache
      CacheDisabled,

      // shadow cache depends
      // on animation duration
      CacheVariable,

      // shadow cache has maximum size
      CacheMaximum
    };

    //! default constructor
    OxygenConfiguration( void );

    //! constructor from KConfig
    OxygenConfiguration( KConfigGroup );

    //! destructor
    virtual ~OxygenConfiguration( void )
    {}

    //! equal to operator
    bool operator == ( const OxygenConfiguration& ) const;

    //! write to kconfig group
    virtual void write( KConfigGroup& ) const;

    //!@name title alignment
    //@{

    static QString titleAlignmentName( Qt::Alignment, bool translated );
    static Qt::Alignment titleAlignment( QString, bool translated );

    virtual Qt::Alignment titleAlignment() const
    { return titleAlignment_; }

    virtual QString titleAlignmentName( bool translated ) const
    { return titleAlignmentName( titleAlignment(), translated ); }

    virtual void setTitleAlignment( Qt::Alignment value )
    { titleAlignment_ = value; }

    virtual void setTitleAlignment( QString value, bool translated )
    { titleAlignment_ = titleAlignment( value, translated ); }

    //@}

    //!@name button size
    //@{

    static QString buttonSizeName( ButtonSize, bool translated );
    static ButtonSize buttonSize( QString, bool translated );
    static int iconScale( ButtonSize );

    virtual ButtonSize buttonSize( void ) const
    { return buttonSize_; }

    virtual int iconScale( void ) const
    { return iconScale( buttonSize() ); }

    virtual QString buttonSizeName( bool translated ) const
    { return buttonSizeName( buttonSize(), translated ); }

    virtual void setButtonSize( ButtonSize value )
    { buttonSize_ = value; }

    virtual bool useNarrowButtonSpacing( void ) const
    { return useNarrowButtonSpacing_; }

    virtual void  setUseNarrowButtonSpacing( bool value )
    { useNarrowButtonSpacing_ = value; }

    //@}

    //!@name cache mode
    //@{

    static QString shadowCacheModeName( ShadowCacheMode, bool translated );
    static ShadowCacheMode shadowCacheMode( QString, bool translated );

    QString shadowCacheModeName( bool translated ) const
    { return shadowCacheModeName( shadowCacheMode(), translated ); }

    void setShadowCacheMode( ShadowCacheMode mode )
    { shadowCacheMode_ = mode; }

    ShadowCacheMode shadowCacheMode( void ) const
    { return shadowCacheMode_; }

    //@]

    //!@name frame border
    //@{

    static QString frameBorderName( FrameBorder, bool translated );
    static FrameBorder frameBorder( QString, bool translated );

    virtual FrameBorder frameBorder() const
    { return frameBorder_; }

    virtual QString frameBorderName( bool translated ) const
    { return frameBorderName( frameBorder(), translated ); }

    virtual void setFrameBorder( FrameBorder value )
    { frameBorder_ = value; }

    virtual void setFrameBorder( QString value, bool translated )
    { frameBorder_ = frameBorder( value, translated ); }

    //@}

    //!@name blend color
    //@{

    static QString blendColorName( BlendColorType, bool translated );
    static BlendColorType blendColor( QString, bool translated );

    virtual BlendColorType blendColor( void ) const
    { return blendColor_; }

    virtual QString blendColorName( bool translated ) const
    { return blendColorName( blendColor(), translated ); }

    virtual void setBlendColor( BlendColorType value )
    { blendColor_ = value; }

    virtual void setBlendColor( QString value, bool translated )
    { blendColor_ = blendColor( value, translated ); }

    //@}

    //!@name size grip
    //@{

    static QString sizeGripModeName( SizeGripMode, bool translated );
    static SizeGripMode sizeGripMode( QString, bool translated );

    virtual SizeGripMode sizeGripMode( void ) const
    { return sizeGripMode_; }

    virtual QString sizeGripModeName( bool translated ) const
    { return sizeGripModeName( sizeGripMode(), translated ); }

    virtual void setSizeGripMode( SizeGripMode value )
    { sizeGripMode_ = value; }

    virtual void setSizeGripMode( QString value, bool translated )
    { sizeGripMode_ = sizeGripMode( value, translated ); }

    //! draw size grip
    virtual bool drawSizeGrip( void ) const
    { return (sizeGripMode() == SizeGripWhenNeeded && frameBorder() == BorderNone ); }

    //@}

    //! separator
    virtual bool drawSeparator( void ) const
    { return drawSeparator_; }

    //! separator
    virtual void setDrawSeparator( bool value )
    { drawSeparator_ = value; }

    //! title outline
    virtual bool drawTitleOutline( void ) const
    { return drawTitleOutline_; }

    //! title outline
    virtual void setDrawTitleOutline( bool value )
    { drawTitleOutline_ = value; }

    //! hide title bar
    virtual bool hideTitleBar( void ) const
    { return hideTitleBar_; }

    //! hide title bar
    virtual void setHideTitleBar( bool value )
    { hideTitleBar_ = value; }

    //! drop shadows
    virtual bool useDropShadows( void ) const
    { return useDropShadows_; }

    //! drop shadows
    virtual void setUseDropShadows( bool value )
    { useDropShadows_ = value; }

    //! oxygen shadows
    virtual bool useOxygenShadows( void ) const
    { return useOxygenShadows_; }

    //! oxygen shadows
    virtual void setUseOxygenShadows( bool value )
    { useOxygenShadows_ = value; }

    //! animations
    virtual bool useAnimations( void ) const
    { return useAnimations_; }

    //! animations
    virtual void setUseAnimations( bool value )
    { useAnimations_ = value; }

    //! animations
    virtual bool animateTitleChange( void ) const
    { return animateTitleChange_; }

    //! animations
    virtual void setAnimateTitleChange( bool value )
    { animateTitleChange_ = value; }

    //! animations
    virtual int animationsDuration( void ) const
    { return animationsDuration_; }

    //! animations
    virtual void setAnimationsDuration( int value )
    { animationsDuration_ = value; }

    //! tabbing
    virtual bool tabsEnabled( void ) const
    { return tabsEnabled_; }

    //! tabbing
    virtual void setTabsEnabled( bool value )
    { tabsEnabled_ = value; }

    private:

    //! title alignment
    Qt::Alignment titleAlignment_;

    //! button size
    ButtonSize buttonSize_;

    //! blend color
    FrameBorder frameBorder_;

    //! frame border
    BlendColorType blendColor_;

    //! size grip mode
    SizeGripMode sizeGripMode_;

    //! separator
    bool drawSeparator_;

    //! active window title outline
    bool drawTitleOutline_;

    //! hide titlebar completely (but not window border)
    bool hideTitleBar_;

    //! drop shadows
    bool useDropShadows_;

    //! oxygen shadows
    bool useOxygenShadows_;

    //! animations
    bool useAnimations_;

    //! animations
    bool animateTitleChange_;

    //! animations
    int animationsDuration_;

    //! tabbing
    bool tabsEnabled_;

    //! narrow button spacing
    bool useNarrowButtonSpacing_;

    //! cache mode
    ShadowCacheMode shadowCacheMode_;

  };

}

#endif
