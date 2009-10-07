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

#include <kcommondecoration.h>
#include <QtCore/QTimeLine>

#include "oxygenconfiguration.h"
#include "lib/helper.h"
#include "lib/tileset.h"


namespace Oxygen
{

    class OxygenSizeGrip;

    class OxygenClient : public KCommonDecorationUnstable
    {

        Q_OBJECT

        public:

        //! constructor
        OxygenClient(KDecorationBridge *b, KDecorationFactory *f);

        //! destructor
        virtual ~OxygenClient();

        virtual QString visibleName() const;
        virtual KCommonDecorationButton *createButton(::ButtonType type);
        virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;

        //!@name flags
        //@{

        //! true if window is maximized
        virtual bool isMaximized( void ) const
        { return maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();  }

        //! return true if timeLine is running
        bool timeLineIsRunning( void ) const
        { return timeLine_.state() == QTimeLine::Running; }

        //! true when title outline is to be drawn
        bool drawTitleOutline( void ) const
        {
          return
            ( timeLineIsRunning() || isActive() ) &&
            configuration().drawTitleOutline();
        }

        //! true when separator is to be drawn
        bool drawSeparator( void ) const
        {
          return
            ( timeLineIsRunning() || isActive() ) &&
            configuration().drawSeparator() &&
            !configuration().drawTitleOutline();
        }

        //@}

        //! dimensions
        virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;

        //! window shape
        virtual void updateWindowShape();

        //! initialization
        virtual void init();

        // reset
        virtual void reset( unsigned long changed );

        //! return associated configuration
        const OxygenConfiguration& configuration( void ) const
        { return configuration_; }

        //! return timeLine
        const QTimeLine& timeLine( void ) const
        { return timeLine_; }

        //! helper class
        OxygenHelper& helper( void ) const
        { return helper_; }

        //! return animation opacity
        qreal opacity( void ) const
        { return qreal( timeLine_.currentFrame() )/qreal( timeLine_.endFrame() ); }

        //! palette background
        QPalette backgroundPalette( const QWidget*, QPalette ) const;

        //! background
        QColor backgroundColor( const QWidget* widget, QPalette palette ) const
        { return backgroundColor( widget, palette, isActive() ); }

        //! background
        QColor backgroundColor( const QWidget*, QPalette, bool ) const;

        //! window background
        virtual void renderWindowBackground( QPainter*, const QRect&, const QWidget*, const QPalette& ) const;

        //! window border
        // this draws a "blue" border around active window
        virtual void renderWindowBorder( QPainter*, const QRect&, const QWidget*, const QPalette&, TileSet::Tiles tiles = TileSet::Ring ) const;

        //! separator
        virtual void renderSeparator( QPainter*, const QRect&, const QWidget*, const QColor& ) const;

        //! get maximum space available for title
        virtual QRect titleRect( const QRect& ) const;

        //! get title bounding rect
        virtual QRect titleBoundingRect( QPainter*, const QRect&, const QString& ) const;

        //! title outline
        virtual void renderTitleOutline( QPainter*, const QRect&, const QPalette& ) const;

        //! title text
        virtual void renderTitleText( QPainter*, const QRect&, Qt::Alignment, QColor ) const;

        //! triggered when window activity is changed
        virtual void activeChange();

        //! triggered when maximize state changed
        virtual void maximizeChange();

        //! triggered when window shade is changed
        virtual void shadeChange();

        //! triggered when window shade is changed
        virtual void captionChange();

        public slots:

        //! reset configuration
        void resetConfiguration( void );

        protected:

        //! paint
        void paintEvent( QPaintEvent* );

        protected slots:

        //! update old caption with current
        void updateOldCaption( void )
        { setOldCaption( caption() ); }

        private:

        //! title timeline
        bool titleTimeLineIsRunning( void ) const
        { return titleTimeLine_.state() == QTimeLine::Running; }

        //! title opacity
        qreal titleOpacity( void ) const
        { return qreal( titleTimeLine_.currentFrame() )/qreal( titleTimeLine_.endFrame() ); }

        //! old caption if any
        const QString& oldCaption( void ) const
        { return oldCaption_; }

        //! old caption
        void setOldCaption( const QString& value )
        { oldCaption_ = value; }

        //! return true when activity change are animated
        bool animateActiveChange( void ) const
        { return ( configuration().useAnimations() && !isPreview() ); }

        //! return true when activity change are animated
        bool animateTitleChange( void ) const
        {
          return
            configuration().useAnimations() &&
            !configuration().drawTitleOutline() &&
            !isPreview();
        }

        //! calculate mask
        QRegion calcMask( void ) const;

        //! text color
        QColor titlebarTextColor(const QPalette&);

        //! text color
        QColor titlebarTextColor(const QPalette&, bool active);

        //!@name size grip
        //@{

        //! create size grip
        void createSizeGrip( void );

        //! delete size grip
        void deleteSizeGrip( void );

        // size grip
        bool hasSizeGrip( void ) const
        { return (bool)sizeGrip_; }

        //! size grip
        OxygenSizeGrip& sizeGrip( void ) const
        { return *sizeGrip_; }

        //@}

        //! configuration
        OxygenConfiguration configuration_;

        //! used to invalidate color cache
        bool colorCacheInvalid_;

        //! stored color
        QColor cachedTitlebarTextColor_;

        //! size grip widget
        OxygenSizeGrip* sizeGrip_;

        //! animation timeLine
        QTimeLine timeLine_;

        //! title animation timeLine
        QTimeLine titleTimeLine_;

        //! old caption
        QString oldCaption_;

        //! helper
        OxygenHelper& helper_;

        //! true when initialized
        bool initialized_;

    };


} // namespace Oxygen

#endif // EXAMPLECLIENT_H
