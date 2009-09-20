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

#include "oxygenclient.h"
#include "oxygenshadowconfiguration.h"
#include "lib/tileset.h"

namespace Oxygen
{


  class OxygenShadowCache
  {
    public:

    //! constructor
    OxygenShadowCache( int maxIndex );

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
    { return qMax( activeShadowConfiguration_.shadowSize(), inactiveShadowConfiguration_.shadowSize() ); }

    //! get shadow matching client
    TileSet* tileSet( const OxygenClient* );

    //! get shadow matching client and animation index
    TileSet* tileSet( const OxygenClient*, int );

    //! Key class to be used into QCache
    /*! class is entirely inline for optimization */
    class Key
    {

      public:

      //! this is a shorter enumeration
      enum FrameBorder
      {
        BorderNone,
        BorderNoSide,
        BorderAny
      };

      //! explicit constructor
      explicit Key( void ):
        index(0),
        active(false),
        useOxygenShadows(false),
        isShade(false),
        hasTitleOutline(false),
        frameBorder( BorderAny )
      {}

      //! constructor from client
      Key( const OxygenClient* client ):
        index(0)
      {

        active = client->isActive();
        useOxygenShadows = client->configuration().useOxygenShadows();
        isShade = client->isShade();
        hasTitleOutline = client->configuration().drawTitleOutline();
        switch(  client->configuration().frameBorder() )
        {
          case OxygenConfiguration::BorderNone: frameBorder = Key::BorderNone; break;
          case OxygenConfiguration::BorderNoSide:  frameBorder = Key::BorderNoSide; break;
          default:  frameBorder = Key::BorderAny; break;
        }

      }

      //! constructor from int
      Key( int hash ):
        index( hash>>6 ),
        active( (hash>>5)&1 ),
        useOxygenShadows( (hash>>4)&1 ),
        isShade( (hash>>3)&1 ),
        hasTitleOutline( (hash>>2)&1 ),
        frameBorder( (FrameBorder)(hash&3) )
      {}

      //! hash function
      int hash( void ) const
      {

        // note this can be optimized because not all of the flag configurations are actually relevant
        // allocate 3 empty bits for flags
        return
          ( index << 6 ) |
          ( active << 5 ) |
          (useOxygenShadows << 4 ) |
          (isShade<<3) |
          (hasTitleOutline<<2) |
          (frameBorder<<0);

      }

      int index;
      bool active;
      bool useOxygenShadows;
      bool isShade;
      bool hasTitleOutline;

      FrameBorder frameBorder;

    };

    //! complex pixmap (when needed)
    QPixmap shadowPixmap( const OxygenClient*, bool active ) const;

    //! simple pixmap
    QPixmap simpleShadowPixmap( const QColor& color, const Key& key ) const
    { return simpleShadowPixmap( color, key, key.active ); }

    //! simple pixmap
    QPixmap simpleShadowPixmap( const QColor&, const Key&, bool active ) const;

    private:

    //! draw gradient into rect
    /*! a separate method is used in order to properly account for corners */
    void renderGradient( QPainter&, const QRectF&, const QRadialGradient&, bool noBorder = false ) const;

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
