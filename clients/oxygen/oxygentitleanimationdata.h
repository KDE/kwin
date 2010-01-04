#ifndef oxygentitleanimationdata_h
#define oxygentitleanimationdata_h

//////////////////////////////////////////////////////////////////////////////
// oxygentitleanimationdata.h
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

#include "lib/oxygenanimation.h"

#include <cassert>
#include <QtCore/QObject>
#include <QtCore/QWeakPointer>
#include <QtGui/QPixmap>

namespace Oxygen
{

    class TitleAnimationData: public QObject
    {

        Q_OBJECT

        //! declare title opacity
        Q_PROPERTY( qreal opacity READ opacity WRITE setOpacity )

        public:

        typedef QWeakPointer<TitleAnimationData> Pointer;

        //! constructor
        TitleAnimationData( QObject* );

        //! reset
        void reset( void )
        {
            setOpacity(0);
            contrastPixmap_.reset();
            pixmap_.reset();
        }

        //! initialize
        void initialize( void );

        // reset
        void reset( QRect rect, QPixmap pixmap, QPixmap contrast )
        {
            setOpacity(0);
            contrastPixmap_.reset( rect, contrast );
            pixmap_.reset( rect, pixmap );
        }

        //! set pixmaps
        void setPixmaps( QRect, QPixmap pixmap, QPixmap contrast );

        //! duration
        void setDuration( int duration )
        {
            assert( animation() );
            animation().data()->setDuration( duration );
        }

        //! retrieve contrast pixmap
        QPixmap contrastPixmap( void ) const
        { return contrastPixmap_.currentPixmap(); }

        //! pixmap
        QPixmap pixmap( void ) const
        { return pixmap_.currentPixmap(); }

        //!@name animation
        //@{


        bool isAnimated( void ) const
        { return animation().data()->isRunning(); }

        //! start animation
        void startAnimation( void )
        {
            assert( !isAnimated() );
            animation().data()->start();
        }

        //@}

        //!@name opacity
        //@{

        qreal opacity( void ) const
        { return opacity_; }

        void setOpacity( qreal value )
        { opacity_ = value; }

        //@}

        //! validity
        bool isValid( void ) const
        { return pixmap_.isValid(); }

        //! dirty flag
        void setDirty( bool value )
        { dirty_ = value; }

        //! dirty flag
        bool isDirty( void ) const
        { return dirty_; }

        signals:

        virtual void pixmapsChanged( void );

        protected slots:

        //! update pixmaps
        virtual void updatePixmaps( void );

        protected:

        //! animation object
        const Animation::Pointer& animation( void ) const
        { return animation_; }

        private:

        //! used to blend pixmap
        class BlendedPixmap
        {

            public:

            // reset everything
            void reset( void )
            {
                startRect_ = endRect_ = QRect();
                startPixmap_ = endPixmap_ = currentPixmap_ = QPixmap();
            }

            //! reset
            void reset( const QRect& rect, QPixmap pixmap )
            {
                startRect_ = endRect_ = rect;
                startPixmap_ = endPixmap_ = currentPixmap_ = pixmap;
            }

            // update pixmaps
            void initialize( const QRect& rect, QPixmap pixmap )
            {
                startRect_ = endRect_;
                endRect_ = rect;
                startPixmap_ = currentPixmap_;
                endPixmap_ = pixmap;
            }

            //! update currentPixmap by blending start and end pixmap
            void blend( qreal opacity );

            //! current pixmap
            QPixmap currentPixmap( void ) const
            { return currentPixmap_; }

            //! validity
            bool isValid( void ) const
            { return !(endRect_.isNull() || endPixmap_.isNull() ); }

            protected:

            // fade pixmap by some amount
            QPixmap fade( QPixmap, qreal ) const;

            private:

            //! animation starting pixmap
            QPixmap startPixmap_;

            //! animation ending pixmap
            QPixmap endPixmap_;

            //! animation current pixmap
            QPixmap currentPixmap_;

            QRect startRect_;
            QRect endRect_;

        };

        bool dirty_;

        BlendedPixmap contrastPixmap_;
        BlendedPixmap pixmap_;

        //! title animation
        Animation::Pointer animation_;

        //! title opacity
        qreal opacity_;

    };


}

#endif
