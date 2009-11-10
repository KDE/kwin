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

#include "oxygen.h"
#include "oxygenconfiguration.h"
#include "lib/helper.h"

namespace Oxygen
{

    class OxygenSizeGrip;
    class OxygenClient : public KCommonDecorationUnstable
    {

        Q_OBJECT

        public:

        //! constructor
        OxygenClient(KDecorationBridge *b, OxygenFactory *f);

        //! destructor
        virtual ~OxygenClient();

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

        //! return true if timeLine is running
        bool timeLineIsRunning( void ) const
        { return timeLine_.state() == QTimeLine::Running; }

        //! true when separator is to be drawn
        bool drawSeparator( void ) const
        {
            return
                ( timeLineIsRunning() || isActive() ) &&
                configuration().drawSeparator() &&
                !configuration().hideTitleBar() &&
                !configuration().drawTitleOutline();
        }

        //@}

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
        { return factory_->helper(); }

        //! helper class
        OxygenShadowCache& shadowCache( void ) const
        { return factory_->shadowCache(); }

        //! return animation opacity
        qreal opacity( void ) const
        {
            int frame( timeLine_.currentFrame() );
            if( timeLine_.direction() == QTimeLine::Backward ) frame -= timeLine_.startFrame();
            return qreal( frame )/qreal( timeLine_.endFrame() );
        }

        //!@name metrics and color definitions
        //@{

        //! dimensions
        virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;

        //! get title bounding rect
        virtual QRect titleBoundingRect( QPainter* painter, const QString& caption ) const
        { return titleBoundingRect( painter, titleRect(), caption ); }

        //! get title bounding rect
        virtual QRect titleBoundingRect( QPainter*, const QRect&, const QString& ) const;

        //! palette background
        QPalette backgroundPalette( const QWidget*, QPalette ) const;

        //! background
        QColor backgroundColor( const QWidget* widget, QPalette palette ) const
        { return backgroundColor( widget, palette, isActive() ); }

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

        public slots:

        //! reset configuration
        void resetConfiguration( void );

        protected:

        //! paint
        virtual void paintEvent( QPaintEvent* );

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
        virtual void renderTitleText( QPainter*, const QRect&, QColor ) const;

        //! title text
        virtual void renderTitleText( QPainter*, const QRect&, const QString&, const QColor& ) const;

        //! render float frame
        virtual void renderFloatFrame( QPainter*, const QRect&, const QPalette& ) const;

        //! render dots
        virtual void renderDots( QPainter*, const QRect&, const QColor& ) const;

        //@}

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
                !configuration().hideTitleBar() &&
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

        protected slots:

        //! update old caption with current
        void updateOldCaption( void )
        { setOldCaption( caption() ); }

        private:

        //! factory
        OxygenFactory* factory_;

        //! size grip widget
        OxygenSizeGrip* sizeGrip_;

        //! configuration
        OxygenConfiguration configuration_;

        //! animation timeLine
        QTimeLine timeLine_;

        //! title animation timeLine
        QTimeLine titleTimeLine_;

        //! old caption
        QString oldCaption_;

        //! true when initialized
        bool initialized_;

    };

    //!@name utility functions
    //@{

    // dot
    void renderDot(QPainter*, const QPointF&, qreal );

    // contrast
    QColor reduceContrast(const QColor&, const QColor&, double t);

    //@}

} // namespace Oxygen

#endif // EXAMPLECLIENT_H
