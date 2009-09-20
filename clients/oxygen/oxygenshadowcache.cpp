//////////////////////////////////////////////////////////////////////////////
// oxygenshadowcache.cpp
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

#include "oxygenshadowcache.h"
#include "oxygenclient.h"

#include <cassert>
#include <KColorUtils>
#include <KDebug>
#include <QPainter>

namespace Oxygen
{

  // referenced from definition in Oxygendclient.cpp
  OxygenHelper *oxygenHelper();

  //_______________________________________________________
  OxygenShadowCache::OxygenShadowCache( int maxIndex ):
    maxIndex_( maxIndex ),
    activeShadowConfiguration_( OxygenShadowConfiguration( QPalette::Active ) ),
    inactiveShadowConfiguration_( OxygenShadowConfiguration( QPalette::Inactive ) )
  {
    kDebug(1212) << endl;
    shadowCache_.setMaxCost( 1<<6 );
    animatedShadowCache_.setMaxCost( maxIndex_<<6 );
  }

  //_______________________________________________________
  bool OxygenShadowCache::shadowConfigurationChanged( const OxygenShadowConfiguration& other ) const
  {
    const OxygenShadowConfiguration& local = (other.colorGroup() == QPalette::Active ) ? activeShadowConfiguration_:inactiveShadowConfiguration_;
    return !(local == other);
  }

  //_______________________________________________________
  void OxygenShadowCache::setShadowConfiguration( const OxygenShadowConfiguration& other )
  {
    OxygenShadowConfiguration& local = (other.colorGroup() == QPalette::Active ) ? activeShadowConfiguration_:inactiveShadowConfiguration_;
    local = other;
  }

  //_______________________________________________________
  TileSet* OxygenShadowCache::tileSet( const OxygenClient* client )
  {

    // construct key
    Key key( client );

    // check if tileset already in cache
    int hash( key.hash() );
    if( shadowCache_.contains(hash) ) return shadowCache_.object(hash);

    // create tileset otherwise
    qreal size( shadowSize() );
    TileSet* tileSet = new TileSet( shadowPixmap( client, key.active ), size, size, 1, 1);
    shadowCache_.insert( hash, tileSet );
    return tileSet;

  }

  //_______________________________________________________
  TileSet* OxygenShadowCache::tileSet( const OxygenClient* client, int index )
  {

    assert( index <= maxIndex_ );

    // construct key
    Key key( client );
    key.index = index;

    // check if tileset already in cache
    int hash( key.hash() );
    if( animatedShadowCache_.contains(hash) ) return animatedShadowCache_.object(hash);

    // create shadow and tileset otherwise
    qreal size( shadowSize() );
    qreal opacity( qreal(index)/qreal(maxIndex_) );

    QPixmap shadow( size*2, size*2 );
    shadow.fill( Qt::transparent );
    QPainter p( &shadow );
    p.setRenderHint( QPainter::Antialiasing );

    p.setOpacity( 1.0 - opacity );
    p.drawPixmap( QPointF(0,0), shadowPixmap( client, false ) );

    p.setOpacity( opacity );
    p.drawPixmap( QPointF(0,0), shadowPixmap( client, true ) );
    p.end();

    TileSet* tileSet = new TileSet(shadow, size, size, 1, 1);
    animatedShadowCache_.insert( hash, tileSet );
    return tileSet;

  }

  //_________________________________________________________________
  QPixmap OxygenShadowCache::shadowPixmap(const OxygenClient* client, bool active ) const
  {

    // get window color
    Key key( client );
    QPalette palette( client->backgroundPalette( client->widget(), client->widget()->palette() ) );
    QColor color( palette.color( client->widget()->backgroundRole() ) );

    if( active && client->configuration().drawTitleOutline() && client->configuration().frameBorder() == OxygenConfiguration::BorderNone )
    {

      // a more complex tile set is needed for the configuration above:
      // top corners must be beveled with the "active titlebar color" while
      // bottom corners must be beveled with the "window background color".
      // this requires generating two shadow pixmaps and tiling them in the tileSet.

      qreal size( shadowSize() );
      QPixmap shadow( 2*size, 2*size );
      shadow.fill( Qt::transparent );
      QPainter p( &shadow );
      p.setRenderHint( QPainter::Antialiasing );

      QPixmap shadowTop = simpleShadowPixmap( color, key, active );
      QRect topRect( shadow.rect() );
      topRect.setBottom( int( size )-1 );
      p.setClipRect( topRect );
      p.drawPixmap( QPointF( 0, 0 ), shadowTop );

      // get window color
      palette = client->widget()->palette();
      color = palette.color( client->widget()->backgroundRole() );
      QPixmap shadowBottom = simpleShadowPixmap( color, key, active );
      QRect bottomRect( shadow.rect() );
      bottomRect.setTop( int( size ) );
      p.setClipRect( bottomRect );
      p.drawPixmap( QPointF( 0, 0 ), shadowBottom );
      p.end();

      return shadow;

    } else return simpleShadowPixmap( color, key, active );

  }

  //_______________________________________________________
  QPixmap OxygenShadowCache::simpleShadowPixmap( const QColor& color, const Key& key, bool active ) const
  {

    // local reference to relevant shadow configuration
    const OxygenShadowConfiguration& shadowConfiguration(
      active && key.useOxygenShadows ?
      activeShadowConfiguration_:inactiveShadowConfiguration_ );

    static const qreal fixedSize = 25.5;
    qreal size( shadowSize() );
    qreal shadowSize( shadowConfiguration.shadowSize() );

    QPixmap shadow = QPixmap( size*2, size*2 );
    shadow.fill( Qt::transparent );

    QPainter p( &shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );

    // offsets are scaled with the shadow size
    // so that the ratio Top-shadow/Bottom-shadow is kept constant
    // when shadow size is changed
    qreal hoffset = shadowConfiguration.horizontalOffset()*shadowSize/fixedSize;
    qreal voffset = shadowConfiguration.verticalOffset()*shadowSize/fixedSize;

    // some gradients rendering are different at bottom corners for NoBorders windows
    bool noBorder( key.frameBorder == Key::BorderNone && !key.isShade );

    if( active && key.useOxygenShadows )
    {

      {

        int nPoints = 5;
        int values[5] = {255, 220, 180, 25, 0};
        qreal x[5] = {4.4, 4.5, 5, 5.5, 6.5};

        // the first point of this gradient does not scaled
        QRadialGradient rg( size, size, shadowSize );
        QColor c = color;
        for( int i = 0; i<nPoints; i++ )
        { c.setAlpha( values[i] ); rg.setColorAt( x[0]/shadowSize+(x[i]-x[0])/fixedSize, c ); }

        renderGradient( p, shadow.rect(), rg, noBorder );

      }

      {

        // this gradient scales with shadow size
        int nPoints = 8;
        qreal values[8] = {1, 0.58, 0.43, 0.30, 0.22, 0.15, 0.08, 0 };
        qreal x[8] = {0, 4.5, 5.5, 6.5, 7.5, 8.5, 11.5, 14.4};

        // this gradient scales with shadow size
        QRadialGradient rg( size+hoffset, size+voffset, shadowSize );
        QColor c = shadowConfiguration.innerColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

    } else {

      {

        int nPoints = 9;
        qreal values[9] = { 0.17, 0.12, 0.11, 0.075, 0.06, 0.035, 0.025, 0.01, 0 };
        qreal x[9] = {0, 4.5, 6.6, 8.5, 11.5, 14.5, 17.5, 21.5, 25.5 };
        QRadialGradient rg = QRadialGradient( size+20.0*hoffset, size+20.0*voffset, shadowSize );
        QColor c = shadowConfiguration.outerColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

      {

        int nPoints = 7;
        qreal values[7] = {0.55, 0.25, 0.20, 0.1, 0.06, 0.015, 0 };
        qreal x[7] = {0, 4.5, 5.5, 7.5, 8.5, 11.5, 14.5 };
        QRadialGradient rg = QRadialGradient( size+10.0*hoffset, size+10.0*voffset, shadowSize );
        QColor c = shadowConfiguration.midColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

      {
        int nPoints = 5;
        qreal values[5] = { 1, 0.32, 0.22, 0.03, 0 };
        qreal x[5] = { 0, 4.5, 5.0, 5.5, 6.5 };
        QRadialGradient rg = QRadialGradient( size+hoffset, size+voffset, shadowSize );
        QColor c = shadowConfiguration.innerColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        renderGradient( p, shadow.rect(), rg, noBorder );

      }

    }

    // draw the corner of the window - actually all 4 corners as one circle
    // this is all fixedSize. Does not scale with shadow size
    QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
    if( key.frameBorder < Key::BorderAny )
    {

      lg.setColorAt(0.52, oxygenHelper()->backgroundTopColor(color));
      lg.setColorAt(1.0, oxygenHelper()->backgroundBottomColor(color) );

    } else {

      QColor light = oxygenHelper()->calcLightColor( oxygenHelper()->backgroundTopColor(color) );
      QColor dark = oxygenHelper()->calcDarkColor( oxygenHelper()->backgroundBottomColor(color));

      lg.setColorAt(0.52, light);
      lg.setColorAt(1.0, dark);

    }

    // draw ellipse.
    // note: special tricks are needed to cope with some rounding issues when size is not an integer.
    p.setBrush( lg );
    int sizeInt( size );
    p.drawEllipse( QRectF( sizeInt-4, sizeInt-4, 8+2*(size-sizeInt), 8+2*(size-sizeInt) ) );

    p.end();
    return shadow;

  }

  //_______________________________________________________
  void OxygenShadowCache::renderGradient( QPainter& p, const QRectF& rect, const QRadialGradient& rg, bool noBorders ) const
  {

    if( !noBorders )
    {
      p.setBrush( rg );
      p.drawRect( rect );
      return;
    }

    qreal size( rect.width()/2.0 );
    qreal hoffset( rg.center().x() - size );
    qreal voffset( rg.center().y() - size );

    // load gradient stops
    QGradientStops stops( rg.stops() );
    qreal radius( rg.radius() );

    // draw ellipse for the upper rect
    {
      QRectF rect( hoffset, voffset, 2*size-hoffset, size );
      p.setBrush( rg );
      p.drawRect( rect );
    }

    // draw square gradients for the lower rect
    {
      // vertical lines
      QRectF rect( hoffset, size+voffset, 2*size-hoffset, 4 );
      QLinearGradient lg( hoffset, 0.0, 2*size+hoffset, 0.0 );
      for( int i = 0; i<stops.size(); i++ )
      {
        QColor c( stops[i].second );
        qreal xx( stops[i].first*radius );
        lg.setColorAt( (size-xx)/(2.0*size), c );
        lg.setColorAt( (size+xx)/(2.0*size), c );
      }

      p.setBrush( lg );
      p.drawRect( rect );

    }

    {
      // horizontal line
      QRectF rect( size-4+hoffset, size+voffset, 8, size );
      QLinearGradient lg = QLinearGradient( 0, voffset, 0, 2*size+voffset );
      for( int i = 0; i<stops.size(); i++ )
      {
        QColor c( stops[i].second );
        qreal xx( stops[i].first*radius );
        lg.setColorAt( (size+xx)/(2.0*size), c );
      }

      p.setBrush( lg );
      p.drawRect( rect );
    }

    {

      // bottom-left corner
      QRectF rect( hoffset, size+4+voffset, size-4, size );
      QRadialGradient rg = QRadialGradient( size+hoffset-4, size+4+voffset, radius );
      for( int i = 0; i<stops.size(); i++ )
      {
        QColor c( stops[i].second );
        qreal xx( stops[i].first -4.0/rg.radius() );
        if( xx<0 && i < stops.size()-1 )
        {
          qreal x1( stops[i+1].first -4.0/rg.radius() );
          c = KColorUtils::mix( c, stops[i+1].second, -xx/(x1-xx) );
          xx = 0;
        }

        rg.setColorAt( xx, c );
      }

      p.setBrush( rg );
      p.drawRect( rect );

    }

    {
      // bottom-right corner
      QRectF rect( size+4+hoffset, size+4+voffset, size-4, size );
      QRadialGradient rg = QRadialGradient( size+hoffset+4, size+4+voffset, radius );
      for( int i = 0; i<stops.size(); i++ )
      {
        QColor c( stops[i].second );
        qreal xx( stops[i].first -4.0/rg.radius() );
        if( xx<0 && i < stops.size()-1 )
        {
          qreal x1( stops[i+1].first -4.0/rg.radius() );
          c = KColorUtils::mix( c, stops[i+1].second, -xx/(x1-xx) );
          xx = 0;
        }

        rg.setColorAt( xx, c );
      }

      p.setBrush( rg );
      p.drawRect( rect );

    }

  }

}
