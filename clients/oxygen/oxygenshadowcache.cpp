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
#include "oxygenhelper.h"

#include <cassert>
#include <cmath>
#include <KColorUtils>
#include <QtGui/QPainter>
#include <QtCore/QTextStream>

namespace Oxygen
{

    //_______________________________________________________
    qreal sqr( qreal x )
    { return x*x; }

    //_______________________________________________________
    ShadowCache::ShadowCache( DecoHelper& helper ):
        helper_( helper ),
        activeShadowConfiguration_( ShadowConfiguration( QPalette::Active ) ),
        inactiveShadowConfiguration_( ShadowConfiguration( QPalette::Inactive ) )
    {

        setEnabled( true );
        setMaxIndex( 256 );

    }

    //_______________________________________________________
    bool ShadowCache::shadowConfigurationChanged( const ShadowConfiguration& other ) const
    {
        const ShadowConfiguration& local = (other.colorGroup() == QPalette::Active ) ? activeShadowConfiguration_:inactiveShadowConfiguration_;
        return !(local == other);
    }

    //_______________________________________________________
    void ShadowCache::setShadowConfiguration( const ShadowConfiguration& other )
    {
        ShadowConfiguration& local = (other.colorGroup() == QPalette::Active ) ? activeShadowConfiguration_:inactiveShadowConfiguration_;
        local = other;
    }

    //_______________________________________________________
    TileSet* ShadowCache::tileSet( const Client* client )
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
    TileSet* ShadowCache::tileSet( const Client* client, qreal opacity )
    {

        int index( opacity*maxIndex_ );
        assert( index <= maxIndex_ );

        // construct key
        Key key( client );
        key.index = index;

        // check if tileset already in cache
        int hash( key.hash() );
        if( animatedShadowCache_.contains(hash) ) return animatedShadowCache_.object(hash);

        // create shadow and tileset otherwise
        qreal size( shadowSize() );

        QPixmap shadow( size*2, size*2 );
        shadow.fill( Qt::transparent );
        QPainter p( &shadow );
        p.setRenderHint( QPainter::Antialiasing );

        QPixmap inactiveShadow( shadowPixmap( client, false ) );
        {
            QPainter pp( &inactiveShadow );
            pp.setRenderHint( QPainter::Antialiasing );
            pp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            pp.fillRect( inactiveShadow.rect(), QColor( 0, 0, 0, 255*(1.0-opacity ) ) );
        }

        QPixmap activeShadow( shadowPixmap( client, true ) );
        {
            QPainter pp( &activeShadow );
            pp.setRenderHint( QPainter::Antialiasing );
            pp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            pp.fillRect( activeShadow.rect(), QColor( 0, 0, 0, 255*( opacity ) ) );
        }

        p.drawPixmap( QPointF(0,0), inactiveShadow );
        p.drawPixmap( QPointF(0,0), activeShadow );
        p.end();

        TileSet* tileSet = new TileSet(shadow, size, size, 1, 1);
        animatedShadowCache_.insert( hash, tileSet );
        return tileSet;

    }

    //_________________________________________________________________
    QPixmap ShadowCache::shadowPixmap(const Client* client, bool active ) const
    {

        // get window color
        Key key( client );
        QPalette palette( client->backgroundPalette( client->widget(), client->widget()->palette() ) );
        QColor color( palette.color( client->widget()->backgroundRole() ) );
        return simpleShadowPixmap( color, key, active );

    }

    //_______________________________________________________
    QPixmap ShadowCache::simpleShadowPixmap( const QColor& color, const Key& key, bool active ) const
    {

        // local reference to relevant shadow configuration
        const ShadowConfiguration& shadowConfiguration(
            active && key.useOxygenShadows ?
            activeShadowConfiguration_:inactiveShadowConfiguration_ );

        static const qreal fixedSize = 25.5;
        qreal size( shadowSize() );
        qreal shadowSize( shadowConfiguration.isEnabled() ? shadowConfiguration.shadowSize():0 );

        QPixmap shadow = QPixmap( size*2, size*2 );
        shadow.fill( Qt::transparent );

        QPainter p( &shadow );
        p.setRenderHint( QPainter::Antialiasing );
        p.setPen( Qt::NoPen );

        // some gradients rendering are different at bottom corners if client has no border
        bool hasBorder( key.hasBorder || key.isShade );

        if( shadowSize )
        {

            if( active && key.useOxygenShadows )
            {

                {

                    // inner (shark) gradient
                    const qreal gradientSize = qMin( shadowSize, (shadowSize+fixedSize)/2 );
                    const qreal hoffset = shadowConfiguration.horizontalOffset()*gradientSize/fixedSize;
                    const qreal voffset = shadowConfiguration.verticalOffset()*gradientSize/fixedSize;

                    QRadialGradient rg = QRadialGradient( size+12.0*hoffset, size+12.0*voffset, gradientSize );
                    rg.setColorAt(1, Qt::transparent );

                    // gaussian shadow is used
                    int nPoints( (10*gradientSize)/fixedSize );
                    Gaussian f( 0.85, 0.17 );
                    QColor c = shadowConfiguration.innerColor();
                    for( int i = 0; i < nPoints; i++ )
                    {
                        qreal x = qreal(i)/nPoints;
                        c.setAlphaF( f(x) );
                        rg.setColorAt( x, c );

                    }

                    p.setBrush( rg );
                    renderGradient( p, shadow.rect(), rg, hasBorder );

                }

                {

                    // outer (spread) gradient
                    const qreal gradientSize = shadowSize;
                    const qreal hoffset = shadowConfiguration.horizontalOffset()*gradientSize/fixedSize;
                    const qreal voffset = shadowConfiguration.verticalOffset()*gradientSize/fixedSize;

                    QRadialGradient rg = QRadialGradient( size+12.0*hoffset, size+12.0*voffset, gradientSize );
                    rg.setColorAt(1, Qt::transparent );

                    // gaussian shadow is used
                    int nPoints( (10*gradientSize)/fixedSize );
                    Gaussian f( 0.46, 0.34 );
                    QColor c = shadowConfiguration.outerColor();
                    for( int i = 0; i < nPoints; i++ )
                    {
                        qreal x = qreal(i)/nPoints;
                        c.setAlphaF( f(x) );
                        rg.setColorAt( x, c );

                    }

                    p.setBrush( rg );
                    p.drawRect( shadow.rect() );

                }

            } else {

                {
                    // inner (sharp gradient)
                    const qreal gradientSize = qMin( shadowSize, fixedSize );
                    const qreal hoffset = shadowConfiguration.horizontalOffset()*gradientSize/fixedSize;
                    const qreal voffset = shadowConfiguration.verticalOffset()*gradientSize/fixedSize;

                    QRadialGradient rg = QRadialGradient( size+hoffset, size+voffset, gradientSize );
                    rg.setColorAt(1, Qt::transparent );

                    // parabolic shadow is used
                    int nPoints( (10*gradientSize)/fixedSize );
                    Parabolic f( 1.0, 0.22 );
                    QColor c = shadowConfiguration.outerColor();
                    for( int i = 0; i < nPoints; i++ )
                    {
                        qreal x = qreal(i)/nPoints;
                        c.setAlphaF( f(x) );
                        rg.setColorAt( x, c );

                    }


                    p.setBrush( rg );
                    renderGradient( p, shadow.rect(), rg, hasBorder );

                }

                {

                    // mid gradient
                    const qreal gradientSize = qMin( shadowSize, (shadowSize+2*fixedSize)/3 );
                    const qreal hoffset = shadowConfiguration.horizontalOffset()*gradientSize/fixedSize;
                    const qreal voffset = shadowConfiguration.verticalOffset()*gradientSize/fixedSize;

                    // gaussian shadow is used
                    QRadialGradient rg = QRadialGradient( size+8.0*hoffset, size+8.0*voffset, gradientSize );
                    rg.setColorAt(1, Qt::transparent );

                    int nPoints( (10*gradientSize)/fixedSize );
                    Gaussian f( 0.54, 0.21);
                    QColor c = shadowConfiguration.outerColor();
                    for( int i = 0; i < nPoints; i++ )
                    {
                        qreal x = qreal(i)/nPoints;
                        c.setAlphaF( f(x) );
                        rg.setColorAt( x, c );

                    }

                    p.setBrush( rg );
                    p.drawRect( shadow.rect() );

                }

                {

                    // outer (spread) gradient
                    const qreal gradientSize = shadowSize;
                    const qreal hoffset = shadowConfiguration.horizontalOffset()*gradientSize/fixedSize;
                    const qreal voffset = shadowConfiguration.verticalOffset()*gradientSize/fixedSize;

                    // gaussian shadow is used
                    QRadialGradient rg = QRadialGradient( size+20.0*hoffset, size+20.0*voffset, gradientSize );
                    rg.setColorAt(1, Qt::transparent );

                    int nPoints( (10*gradientSize)/fixedSize );
                    Gaussian f( 0.155, 0.445);
                    QColor c = shadowConfiguration.outerColor();
                    for( int i = 0; i < nPoints; i++ )
                    {
                        qreal x = qreal(i)/nPoints;
                        c.setAlphaF( f(x) );
                        rg.setColorAt( x, c );
                    }

                    p.setBrush( rg );
                    p.drawRect( shadow.rect() );

                }

            }

        }

        // draw the corner of the window - actually all 4 corners as one circle
        // this is all fixedSize. Does not scale with shadow size
        QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
        lg.setColorAt(0.0, helper().calcLightColor( helper().backgroundTopColor(color) ));
        lg.setColorAt(0.51, helper().backgroundBottomColor(color) );
        lg.setColorAt(1.0, helper().backgroundBottomColor(color) );


        // draw ellipse.
        p.setBrush( lg );
        p.drawEllipse( QRectF( size-4, size-4, 8, 8 ) );

        p.end();
        return shadow;

    }

    //_______________________________________________________
    void ShadowCache::renderGradient( QPainter& p, const QRectF& rect, const QRadialGradient& rg, bool hasBorder ) const
    {

        if( hasBorder )
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
                if( xx<0 )
                {
                    if( i < stops.size()-1 )
                    {
                        qreal x1( stops[i+1].first -4.0/rg.radius() );
                        c = KColorUtils::mix( c, stops[i+1].second, -xx/(x1-xx) );
                    }
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
                if( xx<0 )
                {
                    if( i < stops.size()-1 )
                    {
                        qreal x1( stops[i+1].first -4.0/rg.radius() );
                        c = KColorUtils::mix( c, stops[i+1].second, -xx/(x1-xx) );
                    }
                    xx = 0;
                }

                rg.setColorAt( xx, c );
            }

            p.setBrush( rg );
            p.drawRect( rect );

        }

    }

    //__________________________________________________________
    ShadowCache::Key::Key( const Client* client):
        index(0)
     {

        active = client->isActive() || client->isForcedActive();
        useOxygenShadows = client->configuration().useOxygenShadows();
        isShade = client->isShade();
        hasTitleOutline = client->configuration().drawTitleOutline();
        hasBorder = ( client->configuration().frameBorder() > Configuration::BorderNone );

    }


}
