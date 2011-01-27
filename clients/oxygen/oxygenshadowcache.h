#ifndef oxygenshadowcache_h
#define oxygenshadowcache_h

//////////////////////////////////////////////////////////////////////////////
// oxygenshadowcache.h
// handles caching of TileSet objects to draw shadows
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

#include "oxygendecohelper.h"
#include "oxygenshadowconfiguration.h"

#include <cmath>
#include <QtCore/QCache>
#include <QtGui/QRadialGradient>


namespace Oxygen
{

    class ShadowCache
    {
        public:

        //! constructor
        ShadowCache( DecoHelper& helper );

        //! destructor
        virtual ~ShadowCache( void )
        {}

        //! cache size
        void setEnabled( bool enabled )
        {
            enabled_ = enabled;
            if( enabled )
            {

                shadowCache_.setMaxCost( 1<<6 );
                animatedShadowCache_.setMaxCost( maxIndex_<<6 );

            } else {

                shadowCache_.setMaxCost( 1 );
                animatedShadowCache_.setMaxCost( 1 );

            }
        }

        //! max animation index
        int maxIndex( void ) const
        { return maxIndex_; }

        //! max animation index
        void setMaxIndex( int value )
        {
            maxIndex_ = value;
            if( enabled_ )
            {

                shadowCache_.setMaxCost( 1<<6 );
                animatedShadowCache_.setMaxCost( maxIndex_<<6 );

            }

        }

        //! invalidate caches
        void invalidateCaches( void )
        {
            shadowCache_.clear();
            animatedShadowCache_.clear();
        }

        //! returns true if provided shadow configuration changes with respect to stored
        /*!
        use ShadowConfiguration::colorRole() to decide whether it should be stored
        as active or inactive
        */
        bool shadowConfigurationChanged( const ShadowConfiguration& ) const;

        //! set shadowConfiguration
        /*!
        use ShadowConfiguration::colorRole() to decide whether it should be stored
        as active or inactive
        */
        void setShadowConfiguration( const ShadowConfiguration& );

        //! shadow size
        qreal shadowSize( void ) const
        {
            qreal activeSize( activeShadowConfiguration_.isEnabled() ? activeShadowConfiguration_.shadowSize():0 );
            qreal inactiveSize( inactiveShadowConfiguration_.isEnabled() ? inactiveShadowConfiguration_.shadowSize():0 );
            qreal size( qMax( activeSize, inactiveSize ) );

            // even if shadows are disabled,
            // you need a minimum size to allow corner rendering
            return qMax( size, qreal(5.0) );
        }

        //! Key class to be used into QCache
        /*! class is entirely inline for optimization */
        class Key
        {

            public:

            //! explicit constructor
            explicit Key( void ):
                index(0),
                active(false),
                useOxygenShadows(false),
                isShade(false),
                hasBorder( true )
            {}

            //! constructor from int
            Key( int hash ):
                index( hash>>4 ),
                active( (hash>>3)&1 ),
                useOxygenShadows( (hash>>2)&1 ),
                isShade( (hash>>1)&1 ),
                hasBorder( (hash)&1 )
            {}

            //! hash function
            int hash( void ) const
            {

                // note this can be optimized because not all of the flag configurations are actually relevant
                // allocate 3 empty bits for flags
                return
                    ( index << 4 ) |
                    ( active << 3 ) |
                    (useOxygenShadows << 2 ) |
                    (isShade<<1) |
                    (hasBorder);

            }

            int index;
            bool active;
            bool useOxygenShadows;
            bool isShade;
            bool hasBorder;

        };

        //! get shadow matching client
        TileSet* tileSet( const Key& );

        //! get shadow matching client and opacity
        TileSet* tileSet( Key, qreal );

        //! simple pixmap
        QPixmap shadowPixmap( const Key& key ) const
        { return shadowPixmap( key, key.active ); }

        //! simple pixmap
        QPixmap shadowPixmap( const Key&, bool active ) const;

        protected:

        DecoHelper& helper( void ) const
        { return helper_; }

        //! square utility function
        static qreal square( qreal x )
        { return x*x; }

        //! functions used to draw shadows
        class Parabolic
        {
            public:

            //! constructor
            Parabolic( qreal amplitude, qreal width ):
                amplitude_( amplitude ),
                width_( width )
            {}

            //! destructor
            virtual ~Parabolic( void )
            {}

            //! value
            virtual qreal operator() ( qreal x ) const
            { return qMax( 0.0, amplitude_*(1.0 - square(x/width_) ) ); }

            private:

            qreal amplitude_;
            qreal width_;

        };

        //! functions used to draw shadows
        class Gaussian
        {
            public:

            //! constructor
            Gaussian( qreal amplitude, qreal width ):
                amplitude_( amplitude ),
                width_( width )
            {}

            //! destructor
            virtual ~Gaussian( void )
            {}

            //! value
            virtual qreal operator() ( qreal x ) const
            { return qMax( 0.0, amplitude_*(std::exp( -square(x/width_) -0.05 ) ) ); }

            private:

            qreal amplitude_;
            qreal width_;

        };

        private:

        //! draw gradient into rect
        /*! a separate method is used in order to properly account for corners */
        void renderGradient( QPainter&, const QRectF&, const QRadialGradient&, bool hasBorder = true ) const;

        //! helper
        DecoHelper& helper_;

        //! caching enable state
        bool enabled_;

        //! max index
        /*! it is used to set caches max cost, and calculate animation opacity */
        int maxIndex_;

        //! shadow configuration
        ShadowConfiguration activeShadowConfiguration_;

        //! shadow configuration
        ShadowConfiguration inactiveShadowConfiguration_;

        //! cache
        typedef QCache<int, TileSet> TileSetCache;

        //! shadow cache
        TileSetCache shadowCache_;

        //! animated shadow cache
        TileSetCache animatedShadowCache_;

    };

}

#endif
