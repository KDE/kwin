/*
 * Copyright 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
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

#include <KColorUtils>

#include <QtGui/QPainter>

#include <math.h>

OxygenWindecoHelper::OxygenWindecoHelper(const QByteArray &componentName)
    : OxygenHelper(componentName)
{
    m_windecoButtonCache.setMaxCost(64);
}

QPixmap OxygenWindecoHelper::windecoButton(const QColor &color, int size)
{
    quint64 key = (quint64(color.rgba()) << 32) | size;
    QPixmap *pixmap = m_windecoButtonCache.object(key);

    if (!pixmap)
    {
        pixmap = new QPixmap(size, (int)ceil(double(size)*10.0/9.0));
        pixmap->fill(QColor(0,0,0,0));

        QPainter p(pixmap);
        p.setRenderHints(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setWindow(0,0,18,20);

        QColor light = calcLightColor(color);
        QColor dark = calcDarkColor(color);

        // shadow
        drawShadow(p, calcShadowColor(color), 18);

        // bevel
        qreal y = KColorUtils::luma(color);
        qreal yl = KColorUtils::luma(light);
        qreal yd = KColorUtils::luma(light);
        QLinearGradient bevelGradient(0, 0, 0, 18);
        bevelGradient.setColorAt(0.45, light);
        bevelGradient.setColorAt(0.80, dark);
        if (y < yl && y > yd) // no middle when color is very light/dark
            bevelGradient.setColorAt(0.55, color);
        p.setBrush(QBrush(bevelGradient));
        p.drawEllipse(QRectF(2.0,2.0,14.0,14.0));

        // inside mask
        QRadialGradient maskGradient(9,9,7,9,9);
        maskGradient.setColorAt(0.70, QColor(0,0,0,0));
        maskGradient.setColorAt(0.85, QColor(0,0,0,140));
        maskGradient.setColorAt(0.95, QColor(0,0,0,255));
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.setBrush(maskGradient);
        p.drawRect(0,0,20,20);

        // inside
        QLinearGradient innerGradient(0, 3, 0, 15);
        innerGradient.setColorAt(0.0, color);
        innerGradient.setColorAt(1.0, light);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        p.setBrush(innerGradient);
        p.drawEllipse(QRectF(2.0,2.0,14.0,14.0));

        // anti-shadow
        QRadialGradient highlightGradient(9,8.5,8,9,8.5);
        highlightGradient.setColorAt(0.85, alphaColor(light, 0.0));
        highlightGradient.setColorAt(1.00, light);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setBrush(highlightGradient);
        p.drawEllipse(QRectF(2.0,2.0,14.0,14.0));

        m_windecoButtonCache.insert(key, pixmap);
    }

    return *pixmap;
}
