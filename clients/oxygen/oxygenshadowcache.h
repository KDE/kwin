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

#include <QtCore/QCache>
#include <QtGui/QRadialGradient>

#include "oxygenshadowconfiguration.h"
#include "lib/tileset.h"

class OxygenHelper;

namespace Oxygen
{

  class OxygenClient;
  class OxygenShadowCache
  {
    public:

    //! constructor
    OxygenShadowCache( OxygenHelper& helper, int maxIndex );

    //! destructor
    virtual ~OxygenShadowCache( void )
    {}

    //! invalidate caches
    void invalidateCaches( void )
    {
      shadowCache_.clear();
      animatedShadowCache_.clear();
    }

    //! returns true if provided shadow configuration changes with respect to stored
    /*!
    use OxygenShadowConfiguration::colorRole() to decide whether it should be stored
    as active or inactive
    */
    bool shadowConfigurationChanged( const OxygenShadowConfiguration& ) const;

    //! set shadowConfiguration
    /*!
    use OxygenShadowConfiguration::colorRole() to decide whether it should be stored
    as active or inactive
    */
    void setShadowConfiguration( const OxygenShadowConfiguration& );

    //! shadow size
    qreal shadowSize( void ) const
    {
        qreal size( qMax( activeShadowConfiguration_.shadowSize(), inactiveShadowConfiguration_.shadowSize() ) );

        // even if shadows are disabled,
        // you need a minimum size to allow corner rendering
        return qMax( size, qreal(5.0) );
    }

    //! get shadow matching client
    TileSet* tileSet( const OxygenClient* );

    //! get shadow matching client and animation index
    TileSet* tileSet( const OxygenClient*, int );

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
        hasTitleOutline(false),
        hasBorder( true )
      {}

      //! constructor from client
      Key( const OxygenClient* );

      //! constructor from int
      Key( int hash ):
        index( hash>>5 ),
        active( (hash>>4)&1 ),
        useOxygenShadows( (hash>>3)&1 ),
        isShade( (hash>>2)&1 ),
        hasTitleOutline( (hash>>1)&1 ),
        hasBorder( hash&1 )
      {}

      //! hash function
      int hash( void ) const
      {

        // note this can be optimized because not all of the flag configurations are actually relevant
        // allocate 3 empty bits for flags
        return
          ( index << 5 ) |
          ( active << 4 ) |
          (useOxygenShadows << 3 ) |
          (isShade<<2) |
          (hasTitleOutline<<1) |
          (hasBorder<<0);

      }

      int index;
      bool active;
      bool useOxygenShadows;
      bool isShade;
      bool hasTitleOutline;
      bool hasBorder;

    };

    //! complex pixmap (when needed)
    QPixmap shadowPixmap( const OxygenClient*, bool active ) const;

    //! simple pixmap
    QPixmap simpleShadowPixmap( const QColor& color, const Key& key ) const
    { return simpleShadowPixmap( color, key, key.active ); }

    //! simple pixmap
    QPixmap simpleShadowPixmap( const QColor&, const Key&, bool active ) const;

    protected:

    OxygenHelper& helper( void ) const
    { return helper_; }

    private:

    //! draw gradient into rect
    /*! a separate method is used in order to properly account for corners */
    void renderGradient( QPainter&, const QRectF&, const QRadialGradient&, bool hasBorder = true ) const;

    //! helper
    OxygenHelper& helper_;

    //! max index
    /*! it is used to set caches max cost, and calculate animation opacity */
    int maxIndex_;

    //! shadow configuration
    OxygenShadowConfiguration activeShadowConfiguration_;

    //! shadow configuration
    OxygenShadowConfiguration inactiveShadowConfiguration_;

    //! cache
    typedef QCache<int, TileSet> TileSetCache;

    //! shadow cache
    TileSetCache shadowCache_;

    //! animated shadow cache
    TileSetCache animatedShadowCache_;

  };

}

#endif
