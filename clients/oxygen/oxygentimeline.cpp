
//////////////////////////////////////////////////////////////////////////////
// OxygenTimeLine.h
// derive from QTimeLine and is used to perform smooth transitions
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

#include "oxygentimeline.h"
#include "oxygentimeline.moc"

#include <cassert>
#include <KDebug>

namespace Oxygen
{

  //______________________________________________________
  OxygenTimeLine::OxygenTimeLine( int duration, QObject* parent ):
    QTimeLine( duration, parent ),
    direction_( Forward ),
    currentIndex_( 0 ),
    maxIndex_( 255 ),
    lastFrame_( 0 )
  {

    // connections
    connect( this, SIGNAL( frameChanged( int ) ), SLOT( updateCurrentIndex( int ) ) );

  }

  //______________________________________________________
  void OxygenTimeLine::start( void )
  {
    assert( state() == NotRunning );
    lastFrame_ = 0;
    QTimeLine::start();
  }

  //______________________________________________________
  void OxygenTimeLine::updateCurrentIndex( int currentFrame )
  {

    // check interval
    if( currentFrame <= lastFrame_ ) lastFrame_ = 0;

    // calculate step
    int step = (maxIndex()*( currentFrame - lastFrame_ ))/endFrame();
    if( direction_ == Backward ) step *= -1;

    // update lastFrame
    lastFrame_ = currentFrame;

    // check end conditions
    if( currentIndex_ + step <= 0 )
    {

      currentIndex_ = 0;
      emit currentIndexChanged( currentIndex_ );
      stop();

    } else if( currentIndex_ + step >= maxIndex_ ) {

      currentIndex_ = maxIndex_;
      emit currentIndexChanged( currentIndex_ );
      stop();

    } else {
      currentIndex_ += step;
      emit currentIndexChanged( currentIndex_ );
    }

  }

}
