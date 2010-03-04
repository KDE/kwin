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

#include "helper.h"

#include <QtGui/QPainter>
//______________________________________________________________________________
OxygenDecoHelper::OxygenDecoHelper(const QByteArray &componentName):
OxygenHelper(componentName)
{}

//______________________________________________________________________________
QPixmap OxygenDecoHelper::windecoButton(const QColor &color, bool pressed, int size)
{
    quint64 key = (quint64(color.rgba()) << 32) | (size << 1) | (int)pressed;
    QPixmap *pixmap = m_windecoButtonCache.object(key);

    if (!pixmap)
    {
        pixmap = new QPixmap(size, size);
        pixmap->fill(Qt::transparent);

        QColor light  = calcLightColor(color);
        QColor dark   = calcDarkColor(color);

        QPainter p(pixmap);
        p.setRenderHints(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        qreal u = size/18.0;
        p.translate( 0.5*u, (0.5-0.668)*u );

        {
            //plain background
            QLinearGradient lg( 0, u*1.665, 0, u*(12.33+1.665) );
            if( pressed )
            {
                lg.setColorAt( 1, light );
                lg.setColorAt( 0, dark );
            } else {
                lg.setColorAt( 0, light );
                lg.setColorAt( 1, dark );
            }

            QRectF r( u*0.5*(17-12.33), u*1.665, u*12.33, u*12.33 );
            p.setBrush( lg );
            p.drawEllipse( r );
        }

        {
            // outline circle
            qreal penWidth = 0.7;
            QLinearGradient lg( 0, u*1.665, 0, u*(2.0*12.33+1.665) );
            lg.setColorAt( 0, light );
            lg.setColorAt( 1, dark );
            QRectF r( u*0.5*(17-12.33+penWidth), u*(1.665+penWidth), u*(12.33-penWidth), u*(12.33-penWidth) );
            p.setPen( QPen( lg, penWidth*u ) );
            p.drawEllipse( r );
            p.end();
        }

        m_windecoButtonCache.insert(key, pixmap);
    }

    return *pixmap;
}

//_______________________________________________________________________
QPixmap OxygenDecoHelper::windecoButtonGlow(const QColor &color, int size)
{
    quint64 key = (quint64(color.rgba()) << 32) | size;
    QPixmap *pixmap = m_windecoButtonGlowCache.object(key);

    if (!pixmap)
    {
        pixmap = new QPixmap(size, size);
        pixmap->fill(Qt::transparent);

        // right now the same color is used for the two shadows
        QColor light = color;
        QColor dark = color;

        QPainter p(pixmap);
        p.setRenderHints(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        qreal u = size/18.0;
        p.translate( 0.5*u, (0.5-0.668)*u );

        {

            // outer shadow
            QRadialGradient rg( u*8.5, u*8.5, u*8.5 );

            int nPoints = 5;
            qreal x[5] = { 0.61, 0.72, 0.81, 0.9, 1};
            qreal values[5] = { 255-172, 255-178, 255-210, 255-250, 0 };
            QColor c = dark;
            for( int i = 0; i<nPoints; i++ )
            { c.setAlpha( values[i] ); rg.setColorAt( x[i], c ); }

            QRectF r( pixmap->rect() );
            p.setBrush( rg );
            p.drawRect( r );
        }

        {
            // inner shadow
            QRadialGradient rg( u*8.5, u*8.5, u*8.5 );

            static int nPoints = 6;
            qreal x[6] = { 0.61, 0.67, 0.7, 0.74, 0.78, 1 };
            qreal values[6] = { 255-92, 255-100, 255-135, 255-205, 255-250, 0 };
            QColor c = light;
            for( int i = 0; i<nPoints; i++ )
            { c.setAlpha( values[i] ); rg.setColorAt( x[i], c ); }

            QRectF r( pixmap->rect() );
            p.setBrush( rg );
            p.drawRect( r );
        }

        m_windecoButtonGlowCache.insert(key, pixmap);

    }

    return *pixmap;
}
