/*
 * Copyright 2008 Long Huynh Huu <long.upcase@googlemail.com>
 * Copyright 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 * Copyright 2007 Casper Boemann <cbr@boemann.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "oxygendecohelper.h"

#include <QtGui/QPainter>
#include <KColorUtils>
#include <KDebug>

#include <cmath>

namespace Oxygen
{

    //______________________________________________________________________________
    DecoHelper::DecoHelper(const QByteArray &componentName):
        Helper(componentName),
        _debugArea( KDebug::registerArea( "Oxygen (decoration)" ) )

    {}

    //______________________________________________________________________________
    void DecoHelper::invalidateCaches( void )
    {
        // base class call
        Helper::invalidateCaches();

        // local caches
        _windecoButtonCache.clear();
        _titleBarTextColorCache.clear();
        _buttonTextColorCache.clear();

    }

    //______________________________________________________________________________
    QPixmap DecoHelper::windecoButton(const QColor &color, const QColor& glow, bool sunken, int size)
    {

        Oxygen::Cache<QPixmap>::Value* cache( _windecoButtonCache.get( color ) );

        const quint64 key( ( quint64( glow.rgba() ) << 32 ) | (sunken << 23 ) | size );
        QPixmap *pixmap = cache->object( key );

        if( !pixmap )
        {
            pixmap = new QPixmap(size, size);
            pixmap->fill(Qt::transparent);

            QPainter p( pixmap );
            p.setRenderHints(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setWindow( 0, 0, 21, 21 );

            // button shadow
            if( color.isValid() )
            {
                p.save();
                p.translate( 0, -1.4 );
                drawShadow( p, calcShadowColor( color ), 21 );
                p.restore();
            }

            // button glow
            if( glow.isValid() )
            {
                p.save();
                p.translate( 0, -1.4 );
                drawOuterGlow( p, glow, 21 );
                p.restore();
            }

            // button slab
            p.setWindow( 0, 0, 18, 18 );
            if( color.isValid() )
            {
                p.translate( 0, (0.5-0.668) );

                const QColor light( calcLightColor(color) );
                const QColor dark( calcDarkColor(color) );

                {
                    //plain background
                    QLinearGradient lg( 0, 1.665, 0, (12.33+1.665) );
                    if( sunken )
                    {
                        lg.setColorAt( 1, light );
                        lg.setColorAt( 0, dark );
                    } else {
                        lg.setColorAt( 0, light );
                        lg.setColorAt( 1, dark );
                    }

                    const QRectF r( 0.5*(18-12.33), 1.665, 12.33, 12.33 );
                    p.setBrush( lg );
                    p.drawEllipse( r );
                }

                {
                    // outline circle
                    const qreal penWidth( 0.7 );
                    QLinearGradient lg( 0, 1.665, 0, (2.0*12.33+1.665) );
                    lg.setColorAt( 0, light );
                    lg.setColorAt( 1, dark );
                    const QRectF r( 0.5*(18-12.33+penWidth), (1.665+penWidth), (12.33-penWidth), (12.33-penWidth) );
                    p.setPen( QPen( lg, penWidth ) );
                    p.setBrush( Qt::NoBrush );
                    p.drawEllipse( r );
                }

            }

            p.end();
            cache->insert( key, pixmap );
        }

        return *pixmap;
    }

    //_______________________________________________________________________
    QRegion DecoHelper::decoRoundedMask( const QRect& r, int left, int right, int top, int bottom ) const
    {
        // get rect geometry
        int x, y, w, h;
        r.getRect(&x, &y, &w, &h);

        QRegion mask(x + 3*left, y + 0*top, w-3*(left+right), h-0*(top+bottom));
        mask += QRegion(x + 0*left, y + 3*top, w-0*(left+right), h-3*(top+bottom));
        mask += QRegion(x + 1*left, y + 1*top, w-1*(left+right), h-1*(top+bottom));

        return mask;
    }

    //______________________________________________________________________________
    const QColor& DecoHelper::inactiveTitleBarTextColor( const QPalette& palette )
    {

        const quint32 key( palette.color(QPalette::Active, QPalette::Window).rgba() );
        QColor* out( _titleBarTextColorCache.object( key ) );
        if( !out )
        {

            // todo: reimplement cache
            const QColor ab = palette.color(QPalette::Active, QPalette::Window);
            const QColor af = palette.color(QPalette::Active, QPalette::WindowText);
            const QColor nb = palette.color(QPalette::Inactive, QPalette::Window);
            const QColor nf = palette.color(QPalette::Inactive, QPalette::WindowText);
            out = new QColor( reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4)))) );
            _titleBarTextColorCache.insert( key, out );
        }

        return *out;
    }


    //______________________________________________________________________________
    const QColor& DecoHelper::inactiveButtonTextColor( const QPalette& palette )
    {

        const quint32 key( palette.color(QPalette::Active, QPalette::Window).rgba() );
        QColor* out( _buttonTextColorCache.object( key ) );
        if( !out )
        {

            // todo: reimplement cache
            const QColor ab = palette.color(QPalette::Active, QPalette::Button);
            const QColor af = palette.color(QPalette::Active, QPalette::ButtonText);
            const QColor nb = palette.color(QPalette::Inactive, QPalette::Button);
            const QColor nf = palette.color(QPalette::Inactive, QPalette::ButtonText);
            out = new QColor( reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4)))) );
            _buttonTextColorCache.insert( key, out );
        }

        return *out;
    }

    //_________________________________________________________
    QColor DecoHelper::reduceContrast(const QColor &c0, const QColor &c1, double t) const
    {
        const double s( KColorUtils::contrastRatio(c0, c1) );
        if( s < t ) return c1;

        double l(0);
        double h(1.0);
        double x(s);
        double a;
        QColor r( c1 );
        for (int maxiter = 16; maxiter; --maxiter)
        {

            a = 0.5 * (l + h);
            r = KColorUtils::mix(c0, c1, a);
            x = KColorUtils::contrastRatio(c0, r);

            if ( std::abs(x - t) < 0.01) break;
            if (x > t) h = a;
            else l = a;
        }

        return r;
    }

}
