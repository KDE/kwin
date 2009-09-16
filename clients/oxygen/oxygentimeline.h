#ifndef oxygentimeline_h
#define oxygentimeline_h

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

#include <QTimeLine>

namespace Oxygen
{

  class OxygenTimeLine: public QTimeLine
  {

    Q_OBJECT

    public:

    //! constructor
    OxygenTimeLine( int duration, QObject* parent );

    //! max number of Index
    void setMaxIndex( int value )
    { maxIndex_ = value; }

    //! max number of Index
    int maxIndex( void ) const
    { return maxIndex_; }

    //! current Index
    int currentIndex( void ) const
    { return currentIndex_; }

    //! direction used to update Index
    void setDirection( QTimeLine::Direction value )
    { direction_ = value; }

    //! start
    void start( void );

    signals:

    //! emitted whenever the current Index changed
    void currentIndexChanged( int );

    protected slots:

    //! connected to timeLine frameChanged
    void updateCurrentIndex( int );

    private:

    //! direction
    Direction direction_;

    //! current Index
    int currentIndex_;

    //! max Index
    int maxIndex_;

    //! frame at last update
    int lastFrame_;

  };


}

#endif
