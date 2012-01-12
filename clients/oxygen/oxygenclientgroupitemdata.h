#ifndef oxygenclientgroupitemdata_h
#define oxygenclientgroupitemdata_h

//////////////////////////////////////////////////////////////////////////////
// oxygenclientgroupitemdata.h
// handles tabs' geometry and animations
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

#include "oxygenbutton.h"
#include "oxygenanimation.h"

#include <QtCore/QList>
#include <QtCore/QWeakPointer>
#include <QtCore/QRect>

namespace Oxygen
{

    class Client;

    //! animation type
    enum AnimationType {
        AnimationNone = 0,
        AnimationEnter = 1<<0,
        AnimationMove = 1<<1,
        AnimationLeave = 1<<2,
        AnimationSameTarget = 1<<3
    };

    Q_DECLARE_FLAGS(AnimationTypes, AnimationType)

    //! tab data
    class ClientGroupItemData
    {

        public:

        //! constructor
        explicit ClientGroupItemData( void )
        {}

        //! destructor
        virtual ~ClientGroupItemData( void )
        {}

        //! reset all rects to argument
        void reset( const QRect& rect )
        {
            _refBoundingRect = rect;
            _startBoundingRect = rect;
            _endBoundingRect = rect;
            _boundingRect = rect;
        }

        //! tab active rect
        QRect _activeRect;

        //! reference bounding rect
        /*! it is usually identical to activeRect unless there is only one tab in window */
        QRect _refBoundingRect;

        //! tab drawing rect
        QRect _startBoundingRect;

        //! tab drawing rect
        QRect _endBoundingRect;

        //! tab drawing rect
        QRect _boundingRect;

        //! tab button
        typedef QWeakPointer<Button> ButtonPointer;
        ButtonPointer _closeButton;

    };

    class ClientGroupItemDataList: public QObject, public QList<ClientGroupItemData>
    {

        Q_OBJECT

        //! declare animation progress property
        Q_PROPERTY( qreal progress READ progress WRITE setProgress )

        public:

        //! invalid item index
        enum { NoItem = -1 };

        //! constructor
        ClientGroupItemDataList( Client* parent );

        //! dirty state
        void setDirty( const bool& value )
        { _dirty = value; }

        //! dirty state
        bool isDirty( void ) const
        { return _dirty; }

        //! enable animations
        void setAnimationsEnabled( bool value )
        { animationsEnabled_ = value; }

        //! animations enabled
        bool animationsEnabled( void ) const
        { return animationsEnabled_; }

        //! true if being animated
        bool isAnimated( void ) const
        { return animationType_ != AnimationNone; }

        //! animation type
        AnimationTypes animationType( void ) const
        { return animationType_; }

        //! return item index matching QPoint, or -1 if none
        int itemAt( const QPoint&, bool ) const;

        //! returns true if index is target
        bool isTarget( int index ) const
        { return index == targetItem_; }

        //! start animation
        /* might need to add the side of the target here */
        void animate( AnimationTypes, int = NoItem );

        //! true if animation is in progress
        bool isAnimationRunning( void ) const
        { return animation().data()->isRunning(); }

        //! update button activity
        void updateButtonActivity( long visibleItem ) const;

        //! update buttons
        void updateButtons( bool alsoUpdate ) const;

        //! target rect
        const QRect& targetRect( void ) const
        { return targetRect_; }

        //!@name animation progress
        //@{

        //! return animation object
        virtual const Animation::Pointer& animation() const
        { return _animation; }

        void setProgress( qreal value )
        {
            if( progress_ == value ) return;
            progress_ = value;
            updateBoundingRects();
        }

        qreal progress( void ) const
        { return progress_; }

        //@}

        protected:

        //! update bounding rects
        void updateBoundingRects( bool alsoUpdate = true );

        private:

        //! client
        Client& _client;

        //! dirty flag
        /* used to trigger update at next paintEvent */
        bool _dirty;

        //! true if animations are enabled
        bool animationsEnabled_;

        //! animation
        Animation::Pointer _animation;

        //! last animation type
        AnimationTypes animationType_;

        //! animation progress
        qreal progress_;

        //! dragged item
        int draggedItem_;

        //! target item
        int targetItem_;

        //! target rect
        QRect targetRect_;

    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(Oxygen::AnimationTypes)

}

#endif
