
//////////////////////////////////////////////////////////////////////////////
// oxygenclientgroupitemdata.cpp
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

#include "oxygenclientgroupitemdata.h"
#include "oxygenclientgroupitemdata.moc"
#include "oxygenclient.h"

namespace Oxygen
{

    //____________________________________________________________________________
    ClientGroupItemDataList::ClientGroupItemDataList( Client* parent ):
        QObject( parent ),
        QList<ClientGroupItemData>(),
        _client( *parent ),
        _dirty( false ),
        animationsEnabled_( true ),
        _animation( new Animation( 150, this ) ),
        animationType_( AnimationNone ),
        progress_(0),
        draggedItem_( NoItem ),
        targetItem_( NoItem )
    {

        // setup animation
        animation().data()->setStartValue( 0 );
        animation().data()->setEndValue( 1.0 );
        animation().data()->setTargetObject( this );
        animation().data()->setPropertyName( "progress" );

    }

    //________________________________________________________________
    int ClientGroupItemDataList::itemAt( const QPoint& point, bool between ) const
    {

        for( int i=0; i < count(); i++ )
        {
            QRect rect = at(i)._activeRect;
            if( between ) rect.translate( -rect.width() / 2, 0 );
            if( rect.adjusted(0,0,0,2).contains( point ) )
            { return i; }
        }

        return NoItem;

    }

    //____________________________________________________________________________
    void ClientGroupItemDataList::animate( AnimationTypes type, int target )
    {

        // store animation type
        animationType_ = type;

        if( type == AnimationNone )
        {

            if( isAnimationRunning() ) animation().data()->stop();
            targetItem_ = NoItem;
            draggedItem_ = NoItem;
            targetRect_ = QRect();

        } else if( type & (AnimationEnter|AnimationMove ) ) {

            // store dragged item
            bool animate( true );

            if( (type&AnimationSameTarget) && draggedItem_ == NoItem )
            {

                animate = false;
                draggedItem_ = target;

            } else if( (type&AnimationMove) && targetItem_ == target ) return;

            // check animation state
            if( isAnimationRunning() ) animation().data()->stop();

            targetItem_ = target;
            targetRect_ = QRect();
            QRect titleRect( _client.titleRect() );
            int left( titleRect.left() );
            int width = (type&AnimationSameTarget) ?
                titleRect.width()/count():
                titleRect.width()/(count()+1);

            if( (type&AnimationSameTarget) && draggedItem_ < target )
            {
                target++;
                if( target >= count() ) target = NoItem;
            }

            // loop over items and update bounding rects
            for( int index = 0; index < count(); index++ )
            {

                ClientGroupItemData& item( ClientGroupItemDataList::operator[](index) );
                if( index == target )
                {
                    targetRect_ = item._refBoundingRect;
                    targetRect_.setLeft( left );
                    targetRect_.setWidth( width );
                    left+=width;
                }

                item._startBoundingRect = item._boundingRect;
                item._endBoundingRect = item._refBoundingRect;
                item._endBoundingRect.setLeft( left );

                if( (type&AnimationSameTarget) && index == draggedItem_ )
                {

                    item._endBoundingRect.setWidth( 0 );

                } else {

                    item._endBoundingRect.setWidth( width );
                    left+=width;

                }

            }

            if( targetRect_.isNull() )
            {
                targetRect_ = back()._refBoundingRect;
                targetRect_.setLeft( left );
                targetRect_.setWidth( width );
            }

            if( animate )
            {

                if( animationsEnabled() ) animation().data()->start();
                else {

                    // change progress to maximum
                    progress_ = 1;
                    updateBoundingRects();

                }

            } else {

                for( int index = 0; index < count(); index++ )
                {
                    ClientGroupItemData& item( ClientGroupItemDataList::operator[](index) );
                    item._boundingRect = item._endBoundingRect;
                }

                updateButtons( true );

            }

        } else if( type & AnimationLeave ) {

            // stop animation state
            if( isAnimationRunning() ) animation().data()->stop();

            // reset target
            targetItem_ = NoItem;
            targetRect_ = QRect();

            if( type & AnimationSameTarget )
            {

                // store dragged item
                draggedItem_ = target;

                // do nothing if only one item
                if( count() <= 1 ) return;

                QRect titleRect( _client.titleRect() );
                int left( titleRect.left() );
                int width = titleRect.width()/(count()-1);

                // loop over items and update bounding rects
                for( int index = 0; index < count(); index++ )
                {

                    ClientGroupItemData& item( ClientGroupItemDataList::operator[](index) );
                    item._startBoundingRect = item._boundingRect;
                    item._endBoundingRect = item._refBoundingRect;
                    item._endBoundingRect.setLeft( left );
                    if( index != target )
                    {

                        if( count() <= 2 )
                        {

                            item._endBoundingRect = _client.defaultTitleRect( _client.tabId(index) == _client.currentTabId() );

                        } else {


                            item._endBoundingRect.setWidth( width );
                            left+=width;

                        }

                    } else {

                        item._endBoundingRect.setWidth( 0 );

                    }

                }

            } else {

                // loop over items and update bounding rects
                for( int index = 0; index < count(); index++ )
                {
                    ClientGroupItemData& item( ClientGroupItemDataList::operator[](index) );
                    item._startBoundingRect = item._boundingRect;
                    item._endBoundingRect = item._refBoundingRect;
                }

            }

            if( animationsEnabled() ) animation().data()->start();
            else {

                // change progress to maximum
                progress_ = 1;
                updateBoundingRects();

            }

        }

        return;

    }

    //____________________________________________________________________________
    void ClientGroupItemDataList::updateButtonActivity( long visibleItem ) const
    {

        for( int index = 0; index < count(); index++ )
        {

            const ClientGroupItemData& item( at(index) );
            if( item._closeButton )
            { item._closeButton.data()->setForceInactive( _client.tabId(index) != visibleItem ); }

        }

    }

    //____________________________________________________________________________
    void ClientGroupItemDataList::updateButtons( bool alsoUpdate ) const
    {

        // move close buttons
        if( alsoUpdate ) _client.widget()->setUpdatesEnabled( false );
        for( int index = 0; index < count(); index++ )
        {

            const ClientGroupItemData& item( at(index) );
            if( !item._closeButton ) continue;

            if( (!item._boundingRect.isValid()) || ((animationType_ & AnimationSameTarget)&&count()<=2 ) )
            {

                item._closeButton.data()->hide();

            } else {

                QPoint position(
                    item._boundingRect.right() - _client.configuration().buttonSize() - _client.layoutMetric(KCommonDecoration::LM_TitleEdgeRight),
                    item._boundingRect.top() + _client.layoutMetric( KCommonDecoration::LM_TitleEdgeTop ) );

                if( item._closeButton.data()->isHidden() ) item._closeButton.data()->show();
                item._closeButton.data()->move( position );

            }

        }

        if( alsoUpdate )
        {
            _client.widget()->setUpdatesEnabled( true );
            _client.updateTitleRect();
        }

    }

    //____________________________________________________________________________
    void ClientGroupItemDataList::updateBoundingRects( bool alsoUpdate )
    {

        qreal ratio( progress() );
        for( iterator iter = begin(); iter != end(); ++iter )
        {

            // left
            if( iter->_endBoundingRect.left() == iter->_startBoundingRect.left() )
            {

                iter->_boundingRect.setLeft( iter->_startBoundingRect.left() );

            } else {

                iter->_boundingRect.setLeft( (1.0-ratio)*iter->_startBoundingRect.left() + ratio*iter->_endBoundingRect.left() );

            }

            // right
            if( iter->_endBoundingRect.right() == iter->_startBoundingRect.right() )
            {

                iter->_boundingRect.setRight( iter->_startBoundingRect.right() );

            } else {

                iter->_boundingRect.setRight( (1.0-ratio)*iter->_startBoundingRect.right() + ratio*iter->_endBoundingRect.right() );

            }

        }

        // update button position
        updateButtons( alsoUpdate );

    }
}
