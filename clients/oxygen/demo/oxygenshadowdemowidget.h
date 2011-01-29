#ifndef oxygenshadowdemowidget_h
#define oxygenshadowdemowidget_h

//////////////////////////////////////////////////////////////////////////////
// oxygenshadowdemowidget.h
// shadow demo widget
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

#include "oxygentileset.h"
#include "oxygenhelper.h"

#include <QtGui/QWidget>
#include <QtGui/QPaintEvent>
#include <QtGui/QPixmap>
#include <cassert>

namespace Oxygen
{

    //! shadow demo widget
    class ShadowDemoWidget: public QWidget
    {

        Q_OBJECT

        public:

        //! constructor
        explicit ShadowDemoWidget( QWidget* parent = 0 ):
            QWidget( parent ),
            _drawBackground( true ),
            _square( false ),
            _helper( 0L )
            {}

        //! destructor
        virtual ~ShadowDemoWidget( void )
        {}

        //! helper
        void setHelper( Helper& helper )
        { _helper = &helper; }

        //! set tileSet
        void setTileSet( const TileSet& tileSet )
        {
            _tileSet = tileSet;
            if( isVisible() ) update();
        }

        //! tileset
        const TileSet& tileSet( void ) const
        { return _tileSet; }

        //! set shadow size
        void setShadowSize( int size )
        {
            _shadowSize = size;
            setMinimumSize( _shadowSize*2 + 100, _shadowSize*2 + 60 );
        }

        //! square
        void setSquare( bool value )
        {
            _square = value;
             if( isVisible() ) update();
        }

        protected slots:

        //! toggle background drawing
        void toggleBackground( bool value )
        {
            if( value == _drawBackground ) return;
            _drawBackground = value;
            update();
        }

        protected:

        //! paint event
        virtual void paintEvent( QPaintEvent* );

        //! render window to pixmap
        void updateBackgroundPixmap( void );

        Helper& helper( void ) const
        {
            assert( _helper );
            return *_helper;
        }

        private:

        //! shadow size
        int _shadowSize;

        //! draw window background
        bool _drawBackground;

        //! true if (bottom) corners are square
        bool _square;

        //! helper
        Helper* _helper;

        //! dummy widget
        QWidget _dummy;

        //! window pixmap
        QPixmap _backgroundPixmap;

        //! tileSet
        TileSet _tileSet;

    };
}

#endif
