
//////////////////////////////////////////////////////////////////////////////
// oxygenclientgroupitemdata.cpp
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

#include "oxygenclientgroupitemdata.h"
#include "oxygenclientgroupitemdata.moc"
#include "oxygenclient.h"
#include "oxygen.h"

namespace Oxygen
{

  //____________________________________________________________________________
  ClientGroupItemDataList::ClientGroupItemDataList( OxygenClient* parent ):
    QObject( parent ),
    QList<ClientGroupItemData>(),
    client_( *parent ),
    dirty_( false ),
    animationsEnabled_( true ),
    animation_( new Animation( 150, this ) ),
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

    // setup connections
    connect( animation().data(), SIGNAL( valueChanged( const QVariant& ) ), SLOT( updateBoundingRects( void ) ) );
    connect( animation().data(), SIGNAL( finished( void ) ), SLOT( updateBoundingRects( void ) ) );

  }

  //________________________________________________________________
  int ClientGroupItemDataList::itemAt( const QPoint& point, bool between ) const
  {

    for( int i=0; i < count(); i++ )
    {
      QRect rect = at(i).activeRect_;
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
      QRect titleRect( client_.titleRect() );
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
          targetRect_ = item.refBoundingRect_;
          targetRect_.setLeft( left );
          targetRect_.setWidth( width );
          left+=width;
        }

        item.startBoundingRect_ = item.boundingRect_;
        item.endBoundingRect_ = item.refBoundingRect_;
        item.endBoundingRect_.setLeft( left );

        if( (type&AnimationSameTarget) && index == draggedItem_ )
        {

          item.endBoundingRect_.setWidth( 0 );

        } else {

          item.endBoundingRect_.setWidth( width );
          left+=width;

        }

      }

      if( targetRect_.isNull() )
      {
        targetRect_ = back().refBoundingRect_;
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
          item.boundingRect_ = item.endBoundingRect_;
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

        QRect titleRect( client_.titleRect() );
        int left( titleRect.left() );
        int width = titleRect.width()/(count()-1);

        // loop over items and update bounding rects
        for( int index = 0; index < count(); index++ )
        {

          ClientGroupItemData& item( ClientGroupItemDataList::operator[](index) );
          item.startBoundingRect_ = item.boundingRect_;
          item.endBoundingRect_ = item.refBoundingRect_;
          item.endBoundingRect_.setLeft( left );
          if( index != target )
          {

            if( count() <= 2 )
            {

              item.endBoundingRect_ = client_.defaultTitleRect( index == client_.visibleClientGroupItem() );

            } else {


              item.endBoundingRect_.setWidth( width );
              left+=width;

            }

          } else {

            item.endBoundingRect_.setWidth( 0 );

          }

        }

      } else {

        // loop over items and update bounding rects
        for( int index = 0; index < count(); index++ )
        {
          ClientGroupItemData& item( ClientGroupItemDataList::operator[](index) );
          item.startBoundingRect_ = item.boundingRect_;
          item.endBoundingRect_ = item.refBoundingRect_;
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
  void ClientGroupItemDataList::updateButtonActivity( int visibleItem ) const
  {

    for( int index = 0; index < count(); index++ )
    {

      const ClientGroupItemData& item( at(index) );
      if( item.closeButton_ )
      { item.closeButton_.data()->setForceInactive( index != visibleItem ); }

    }

  }

  //____________________________________________________________________________
  void ClientGroupItemDataList::updateButtons( bool alsoUpdate ) const
  {

    // move close buttons
    // this should move to ClientGroupItemDataList
    if( alsoUpdate ) client_.widget()->setUpdatesEnabled( false );
    for( int index = 0; index < count(); index++ )
    {

      const ClientGroupItemData& item( at(index) );
      if( !item.closeButton_ ) continue;

      if( (!item.boundingRect_.isValid()) || ((animationType_ & AnimationSameTarget)&&count()<=2 ) )
      {

        item.closeButton_.data()->hide();

      } else {

        QPoint position(
          item.boundingRect_.right() - client_.configuration().buttonSize() - client_.layoutMetric(KCommonDecoration::LM_TitleEdgeRight),
          item.boundingRect_.top() + client_.layoutMetric( KCommonDecoration::LM_TitleEdgeTop ) );

        if( item.closeButton_.data()->isHidden() ) item.closeButton_.data()->show();
        item.closeButton_.data()->move( position );

      }

    }

    if( alsoUpdate )
    {
      client_.widget()->setUpdatesEnabled( true );
      client_.widget()->update();
    }

  }

  //____________________________________________________________________________
  void ClientGroupItemDataList::updateBoundingRects( bool alsoUpdate )
  {

    qreal ratio( ClientGroupItemDataList::progress() );
    for( iterator iter = begin(); iter != end(); ++iter )
    {

      // left
      if( iter->endBoundingRect_.left() == iter->startBoundingRect_.left() )
      {

        iter->boundingRect_.setLeft( iter->startBoundingRect_.left() );

      } else {

        iter->boundingRect_.setLeft( (1.0-ratio)*iter->startBoundingRect_.left() + ratio*iter->endBoundingRect_.left() );

      }

      // right
      if( iter->endBoundingRect_.right() == iter->startBoundingRect_.right() )
      {

        iter->boundingRect_.setRight( iter->startBoundingRect_.right() );

      } else {

        iter->boundingRect_.setRight( (1.0-ratio)*iter->startBoundingRect_.right() + ratio*iter->endBoundingRect_.right() );

      }

    }

    // update button position
    updateButtons( alsoUpdate );

  }
}
