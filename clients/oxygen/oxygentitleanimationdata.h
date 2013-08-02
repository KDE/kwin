#ifndef oxygentitleanimationdata_h
#define oxygentitleanimationdata_h

//////////////////////////////////////////////////////////////////////////////
// oxygentitleanimationdata.h
// handles transition when window title is changed
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

#include "oxygenanimation.h"

#include <cassert>
#include <QObject>
#include <QWeakPointer>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QPixmap>


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
        explicit TitleAnimationData( QObject* );

        //! reset
        void reset( void )
        {
            setOpacity(0);
            _contrastPixmap.reset();
            _pixmap.reset();
        }

        //! initialize
        void initialize( void );

        // reset
        void reset( QRect rect, QPixmap pixmap, QPixmap contrast )
        {
            setOpacity(0);
            _contrastPixmap.reset( rect, contrast );
            _pixmap.reset( rect, pixmap );
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
        { return _contrastPixmap.currentPixmap(); }

        //! pixmap
        QPixmap pixmap( void ) const
        { return _pixmap.currentPixmap(); }

        //!@name animation
        //@{

        //! returns true if animations are locked
        bool isLocked( void ) const
        { return _animationLockTimer.isActive(); }

        //! returns true if title transition animation is currently running
        bool isAnimated( void ) const
        { return animation().data()->isRunning(); }

        //! start lock animation timer
        void lockAnimations( void )
        { _animationLockTimer.start( _lockTime, this ); }

        //! start lock animation timer
        void unlockAnimations( void )
        { _animationLockTimer.stop(); }

        //! start title transition animation
        void startAnimation( void )
        {
            assert( !isAnimated() );
            animation().data()->start();
        }

        //! finish title transition animation
        void finishAnimation( void )
        {
            assert( isAnimated() );
            animation().data()->stop();
        }

        //@}

        //!@name opacity
        //@{

        qreal opacity( void ) const
        { return _opacity; }

        void setOpacity( qreal value )
        {
            if( _opacity == value ) return;
            _opacity = value;
            updatePixmaps();
        }

        //@}

        //! validity
        bool isValid( void ) const
        { return _pixmap.isValid(); }

        //! dirty flag
        void setDirty( bool value )
        { _dirty = value; }

        //! dirty flag
        bool isDirty( void ) const
        { return _dirty; }

        Q_SIGNALS:

        void pixmapsChanged( void );

        protected:

        //! update pixmaps
        virtual void updatePixmaps( void );

        //! timer event
        void timerEvent( QTimerEvent* );

        //! animation object
        const Animation::Pointer& animation( void ) const
        { return _animation; }

        private:

        //! used to blend pixmap
        class BlendedPixmap
        {

            public:

            // reset everything
            void reset( void )
            {
                _startRect = _endRect = QRect();
                _startPixmap = _endPixmap = _currentPixmap = QPixmap();
            }

            //! reset
            void reset( const QRect& rect, QPixmap pixmap )
            {
                _startRect = _endRect = rect;
                _startPixmap = _endPixmap = _currentPixmap = pixmap;
            }

            // update pixmaps
            void initialize( const QRect& rect, QPixmap pixmap )
            {
                _startRect = _endRect;
                _endRect = rect;
                _startPixmap = _currentPixmap;
                _endPixmap = pixmap;
            }

            //! update currentPixmap by blending start and end pixmap
            void blend( qreal opacity );

            //! current pixmap
            QPixmap currentPixmap( void ) const
            { return _currentPixmap; }

            //! validity
            bool isValid( void ) const
            { return !(_endRect.isNull() || _endPixmap.isNull() ); }

            protected:

            // fade pixmap by some amount
            QPixmap fade( QPixmap, qreal ) const;

            private:

            //! animation starting pixmap
            QPixmap _startPixmap;

            //! animation ending pixmap
            QPixmap _endPixmap;

            //! animation current pixmap
            QPixmap _currentPixmap;

            QRect _startRect;
            QRect _endRect;

        };

        bool _dirty;

        BlendedPixmap _contrastPixmap;
        BlendedPixmap _pixmap;

        //! lock time (milliseconds
        static const int _lockTime;

        //! timer used to disable animations when triggered too early
        QBasicTimer _animationLockTimer;

        //! title animation
        Animation::Pointer _animation;

        //! title opacity
        qreal _opacity;

    };


}

#endif
