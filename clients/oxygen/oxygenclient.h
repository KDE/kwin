#ifndef oxygenclient_h
#define oxygenclient_h

//////////////////////////////////////////////////////////////////////////////
// oxygenclient.h
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (c) 2003, 2004 David Johnson <david@usermode.org>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
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

#include "oxygenanimation.h"
#include "oxygenclientgroupitemdata.h"
#include "oxygenconfiguration.h"
#include "oxygendecohelper.h"
#include "oxygenfactory.h"
#include "oxygenshadowcache.h"
#include "oxygentitleanimationdata.h"

#include <kcommondecoration.h>
#include <QtCore/QBasicTimer>
#include <QtCore/QTextStream>
#include <QtCore/QTimerEvent>

#include <X11/Xdefs.h>

namespace Oxygen
{

    class SizeGrip;
    class Client : public KCommonDecorationUnstable
    {

        Q_OBJECT

        //! declare glow intensity property
        Q_PROPERTY( qreal glowIntensity READ glowIntensityUnbiased WRITE setGlowIntensity )

        public:

        //! constructor
        Client(KDecorationBridge *b, Factory *f);

        //! destructor
        virtual ~Client();

        //! decoration name
        virtual QString visibleName() const;

        //! buttons
        virtual KCommonDecorationButton *createButton(::ButtonType type);

        //!@name flags
        //@{

        //! true if decoration has iquired behavior
        virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;

        //! true if window is maximized
        virtual bool isMaximized( void ) const
        { return maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();  }

        //! true if animations are used
        bool animationsEnabled( void ) const
        { return configuration().animationsEnabled(); }

        //! true if glow is animated
        bool glowIsAnimated( void ) const
        { return _glowAnimation->isRunning(); }

        //! true when decoration is forced active
        bool isForcedActive( void ) const
        { return _forceActive && tabCount() > 1; }

        //! true when separator is to be drawn
        bool drawSeparator( void ) const
        {
            if( configuration().drawTitleOutline() ) return false;
            switch( configuration().separatorMode() )
            {
                case Configuration::SeparatorAlways:
                return true;

                case Configuration::SeparatorActive:
                return ( glowIsAnimated() || isActive() );

                default:
                case Configuration::SeparatorNever:
                return false;
            }

        }

        //! true if titlebar is hidden
        bool hideTitleBar( void ) const
        {
            return
                configuration().hideTitleBar() &&
                !isShade() &&
                tabCount() == 1;
        }

        //@}

        //! window shape
        virtual void updateWindowShape();

        //! initialization
        virtual void init();

        // reset
        virtual void reset( unsigned long changed );

        //! return associated configuration
        const Configuration& configuration( void ) const
        { return _configuration; }

        //!@name glow animation
        //@{

        void setGlowIntensity( qreal value )
        {
            if( _glowIntensity == value ) return;
            _glowIntensity = value;
            widget()->update();
        }

        //! unbiased glow intensity
        qreal glowIntensityUnbiased( void ) const
        { return _glowIntensity; }

        //! glow bias
        static qreal glowBias( void )
        { return 0.2; }

        //! true (biased) intensity
        /*! this is needed to have glow go from either 0.2->1 or 0.8->0 depending on the animation direction */
        qreal glowIntensity( void ) const
        { return _glowAnimation->direction() == Animation::Forward ? _glowIntensity : _glowIntensity-glowBias(); }

        //@}

        //! helper class
        DecoHelper& helper( void ) const
        { return _factory->helper(); }

        //! helper class
        ShadowCache& shadowCache( void ) const
        { return _factory->shadowCache(); }

        //!@name metrics and color definitions
        //@{

        //! dimensions
        virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;

        //! get title rect for untabbed window
        virtual QRect defaultTitleRect( bool active = true ) const;

        //! get title bounding rect
        virtual QRect titleBoundingRect( const QFont& font, const QString& caption ) const
        { return titleBoundingRect( font, titleRect(), caption ); }

        //! get title bounding rect
        virtual QRect titleBoundingRect( const QFont&, QRect, const QString& ) const;

        //! palette background
        QPalette backgroundPalette( const QWidget*, QPalette ) const;

        //! background
        QColor backgroundColor( const QWidget* widget, QPalette palette ) const
        { return backgroundColor( widget, palette, isActive() || isForcedActive() ); }

        //! background
        QColor backgroundColor( const QWidget*, QPalette, bool ) const;

        //@}

        //! default buttons located on the left
        virtual QString defaultButtonsLeft() const;

        //! default buttons located on the right
        virtual QString defaultButtonsRight() const;

        //!@name status change methods (overloaded from KCommonDecorationUnstable)
        //@{

        //! triggered when window activity is changed
        virtual void activeChange();

        //! triggered when maximize state changed
        virtual void maximizeChange();

        //! triggered when window shade is changed
        virtual void shadeChange();

        //! triggered when window shade is changed
        virtual void captionChange();

        //@}

        //! event filter
        virtual bool eventFilter( QObject*, QEvent* );

        //! resize event
        virtual void resizeEvent( QResizeEvent* );

        //! paint background to painter
        void paintBackground( QPainter& ) const;

        public slots:

        //! triggers widget update in titleRect only
        /*! one needs to add the title top margin to avoid some clipping glitches */
        void updateTitleRect( void )
        { widget()->update( titleRect().adjusted( 0, -layoutMetric( LM_TitleEdgeTop ), 0, 1 ) ); }

        protected:

        //! return shadow cache key associated to this client
        ShadowCache::Key key( void ) const
        {
            ShadowCache::Key key;
            key.active = ( isActive() || isForcedActive() ) && configuration().useOxygenShadows();
            key.isShade = isShade();
            key.hasBorder = ( configuration().frameBorder() > Configuration::BorderNone );
            return key;
        }

        //! true when decoration is forced active
        void setForceActive( bool value )
        { _forceActive = value; }

        //!@name event filters
        //@{

        //! paint
        virtual void paintEvent( QPaintEvent* );

        //! render full decoration to provided painter
        virtual void paint( QPainter& );

        //! mouse press event
        virtual bool mousePressEvent( QMouseEvent* );

        //! mouse release event
        virtual bool mouseReleaseEvent( QMouseEvent* );

        //! mouse move event
        virtual bool mouseMoveEvent( QMouseEvent* );

        //! drag enter event
        virtual bool dragEnterEvent( QDragEnterEvent* );

        //! drag move event
        virtual bool dragMoveEvent( QDragMoveEvent* );

        //! drag leave event
        virtual bool dragLeaveEvent( QDragLeaveEvent* );

        //! drop event
        virtual bool dropEvent( QDropEvent* );

        //! timer event
        virtual void timerEvent( QTimerEvent* );

        //@}

        //!@name rendering methods (called in paintEvent)
        //@{

        //! window background
        virtual void renderWindowBackground( QPainter*, const QRect&, const QWidget*, const QPalette& ) const;

        //! window border
        // this draws a "blue" border around active window
        virtual void renderWindowBorder( QPainter*, const QRect&, const QWidget*, const QPalette& ) const;

        //! separator
        virtual void renderSeparator( QPainter*, const QRect&, const QWidget*, const QColor& ) const;

        //! title outline
        virtual void renderTitleOutline( QPainter*, const QRect&, const QPalette& ) const;

        //! title text
        /*! second color, if valid, is for contrast pixel */
        virtual void renderTitleText( QPainter*, const QRect&, const QColor&, const QColor& = QColor() ) const;

        //! title text
        /*! second color, if valid, is for contrast pixel */
        virtual void renderTitleText( QPainter*, const QRect&, const QString&, const QColor&, const QColor& = QColor(), bool elide = true ) const;

        //! title text
        virtual QPixmap renderTitleText( const QRect&, const QString&, const QColor&, bool elide = true ) const;

        //! GroupItem
        virtual void renderItem( QPainter*, int, const QPalette& );

        //! tabbing target rect
        virtual void renderTargetRect( QPainter*, const QPalette& );

        //! render corners
        virtual void renderCorners( QPainter*, const QRect&, const QPalette& ) const;

        //! render float frame
        virtual void renderFloatFrame( QPainter*, const QRect&, const QPalette& ) const;

        //! render dots
        virtual void renderDots( QPainter*, const QRect&, const QColor& ) const;

        //@}

        //! close tab matching give button
        virtual bool closeItem( const Button* );

        //! index of item matching point
        int tabIndexAt( const QPoint& position, bool between = false ) const
        { return _itemData.itemAt( position , between ); }

        //! return pixmap corresponding to a given tab, for dragging
        QPixmap itemDragPixmap( int, QRect, bool = false );

        //! return true when activity change are animated
        bool shadowAnimationsEnabled( void ) const
        { return ( animationsEnabled() && configuration().shadowAnimationsEnabled() && !isPreview() ); }

        //! return true when activity change are animated
        bool titleAnimationsEnabled( void ) const
        {
            return
                animationsEnabled() &&
                configuration().titleAnimationsEnabled() &&
                !configuration().drawTitleOutline() &&
                !hideTitleBar() &&
                !isPreview();
        }

        //! true if some title outline is rendered
        bool hasTitleOutline( void ) const
        {
            return
                tabCount() >= 2 ||
                _itemData.isAnimated() ||
                ( (isActive()||glowIsAnimated()) && configuration().drawTitleOutline() );
        }

        //! calculate mask
        QRegion calcMask( void ) const;

        //! text color
        QColor titlebarTextColor(const QPalette&) const;

        //! text color
        QColor titlebarTextColor(const QPalette& palette, bool active) const
        {
            return active ?
                palette.color(QPalette::Active, QPalette::WindowText):
                helper().inactiveTitleBarTextColor( palette );
        }

        //! text color
        QColor titlebarContrastColor(const QPalette& palette ) const
        { return titlebarContrastColor( palette.color( widget()->window()->backgroundRole() ) ); }

        //! text color
        QColor titlebarContrastColor(const QColor& color ) const
        { return helper().calcLightColor( color ); }

        //!@name size grip
        //@{

        //! create size grip
        void createSizeGrip( void );

        //! delete size grip
        void deleteSizeGrip( void );

        // size grip
        bool hasSizeGrip( void ) const
        { return (bool)_sizeGrip; }

        //! size grip
        SizeGrip& sizeGrip( void ) const
        { return *_sizeGrip; }

        //@}

        //! remove shadow hint
        void removeShadowHint( void );

        protected slots:

        //! set target item to -1
        void clearTargetItem( void );

        //! clear force active flag
        void clearForceActive( void )
        { if( isActive() ) setForceActive( false ); }

        //! title bounding rects
        /*! calculate and return title bounding rects in case of tabbed window */
        void updateItemBoundingRects( bool alsoUpdate = true );

        //! bound one rect to another
        void boundRectTo( QRect&, const QRect& ) const;

        private:

        //! factory
        Factory* _factory;

        //! backing store pixmap (when compositing is not active)
        QPixmap _pixmap;

        //! size grip widget
        SizeGrip* _sizeGrip;

        //! configuration
        Configuration _configuration;

        //! glow animation
        Animation* _glowAnimation;

        //! title animation data
        TitleAnimationData* _titleAnimationData;

        //! glow intensity
        qreal _glowIntensity;

        //! true when initialized
        bool _initialized;

        //! true when decoration is forced active
        bool _forceActive;

        //! mouse button
        Qt::MouseButton _mouseButton;

        //! tab bounding rects
        ClientGroupItemDataList _itemData;

        //! index of tab being dragged if any, -1 otherwise
        int _sourceItem;

        //! drag start point
        QPoint _dragPoint;

        //! drag start timer.
        /*!
        it is needed to activate animations when this was not done via either
        dragMoveEvent or dragLeaveEvent
        */
        QBasicTimer _dragStartTimer;

        //! shadow atom
        Atom _shadowAtom;

    };

} // namespace Oxygen

#endif // EXAMPLECLIENT_H
