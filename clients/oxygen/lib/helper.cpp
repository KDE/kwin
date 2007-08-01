/*
 * Copyright 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 * Copyright 2007 Casper Boemann <cbr@boemann.dk>
 * Copyright 2007 Fredrik Höglund <fredrik@kde.org>
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

#include <KGlobalSettings>
#include <KColorUtils>
#include <KColorScheme>

#include <QtGui/QPainter>

// alphaBlendColors Copyright 2003 Sandro Giessl <ceebx@users.sourceforge.net>
// DEPRECATED (use KColorUtils::mix to the extent we still need such a critter)
QColor alphaBlendColors(const QColor &bgColor, const QColor &fgColor, const int a)
{

    // normal button...
    QRgb rgb = bgColor.rgb();
    QRgb rgb_b = fgColor.rgb();
    int alpha = a;
    if(alpha>255) alpha = 255;
    if(alpha<0) alpha = 0;
    int inv_alpha = 255 - alpha;

    QColor result  = QColor( qRgb(qRed(rgb_b)*inv_alpha/255 + qRed(rgb)*alpha/255,
                                  qGreen(rgb_b)*inv_alpha/255 + qGreen(rgb)*alpha/255,
                                  qBlue(rgb_b)*inv_alpha/255 + qBlue(rgb)*alpha/255) );

    return result;
}

// NOTE: OxygenStyleHelper needs to use a KConfig from its own KComponentData
// Since the ctor order causes a SEGV if we try to pass in a KConfig here from
// a KComponentData constructed in the OxygenStyleHelper ctor, we'll just keep
// one here, even though the window decoration doesn't really need it.
OxygenHelper::OxygenHelper(const QByteArray &componentName)
    : _componentData(componentName, 0, KComponentData::SkipMainComponentRegistration)
{
    _config = _componentData.config();
    _contrast = KGlobalSettings::contrastF(_config);

    m_backgroundCache.setMaxCost(64);
    m_roundCache.setMaxCost(64);
}

KSharedConfigPtr OxygenHelper::config() const
{
    return _config;
}

QColor OxygenHelper::backgroundRadialColor(const QColor &color) const
{
    return KColorScheme::shade(color, KColorScheme::LightShade, _contrast);
}

QColor OxygenHelper::backgroundTopColor(const QColor &color) const
{
    return KColorScheme::shade(color, KColorScheme::MidlightShade, _contrast);
}

QColor OxygenHelper::backgroundBottomColor(const QColor &color) const
{
    return KColorScheme::shade(color, KColorScheme::MidShade, _contrast - 1.1);
}

QColor OxygenHelper::calcLightColor(const QColor &color)
{
    return KColorScheme::shade(color, KColorScheme::LightShade, _contrast);
}

QColor OxygenHelper::calcDarkColor(const QColor &color)
{
    return KColorScheme::shade(color, KColorScheme::MidShade, _contrast);
}

QPixmap OxygenHelper::verticalGradient(const QColor &color, int height)
{
    quint64 key = (quint64(color.rgba()) << 32) | height | 0x8000;
    QPixmap *pixmap = m_backgroundCache.object(key);

    if (!pixmap)
    {
        pixmap = new QPixmap(32, height);

        QLinearGradient gradient(0, 0, 0, height);
        gradient.setColorAt(0.0, backgroundTopColor(color));
        gradient.setColorAt(0.5, color);
        gradient.setColorAt(1.0, backgroundBottomColor(color));

        QPainter p(pixmap);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(pixmap->rect(), gradient);

        m_backgroundCache.insert(key, pixmap);
    }

    return *pixmap;
}

QPixmap OxygenHelper::radialGradient(const QColor &color, int width)
{
    quint64 key = (quint64(color.rgba()) << 32) | width | 0xb000;
    QPixmap *pixmap = m_backgroundCache.object(key);

    if (!pixmap)
    {
        width /= 2;
        pixmap = new QPixmap(width, 64);
        pixmap->fill(QColor(0,0,0,0));
        QColor radialColor = backgroundRadialColor(color);
        radialColor.setAlpha(255);
        QRadialGradient gradient(64, 0, 64);
        gradient.setColorAt(0, radialColor);
        radialColor.setAlpha(101);
        gradient.setColorAt(0.5, radialColor);
        radialColor.setAlpha(37);
        gradient.setColorAt(0.75, radialColor);
        radialColor.setAlpha(0);
        gradient.setColorAt(1, radialColor);

        QPainter p(pixmap);
        p.scale(width/128.0,1);
        p.fillRect(pixmap->rect(), gradient);

        m_backgroundCache.insert(key, pixmap);
    }

    return *pixmap;
}

QPixmap OxygenHelper::roundButton(const QColor &color, int size)
{
    quint64 key = (quint64(color.rgba()) << 32) | size;
    QPixmap *pixmap = m_roundCache.object(key);

    if (!pixmap)
    {
        pixmap = new QPixmap(size, size);
        pixmap->fill(QColor(0,0,0,0));

        QPainter p(pixmap);
        p.setRenderHints(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setWindow(0,0,20,20);

        // shadow
        QRadialGradient shadowGradient(10, 11, 9, 10, 12);
        shadowGradient.setColorAt(0.0, QColor(0,0,0,80));
        shadowGradient.setColorAt(1.0, QColor(0,0,0,0));
        p.setBrush(shadowGradient);
        p.drawEllipse(QRectF(0, 0, 20, 20));

        // outline
        QRadialGradient edgeGradient(10, 10, 9, 10, 10);
        edgeGradient.setColorAt(0.0, QColor(0,0,0,60));
        edgeGradient.setColorAt(0.9, QColor(0,0,0,20));
        edgeGradient.setColorAt(1.0, QColor(0,0,0,0));
        p.setBrush(edgeGradient);
        p.drawEllipse(QRectF(0, 0, 20, 20));

        // base (for anti-shadow)
        p.setBrush(color);
        p.drawEllipse(QRectF(2.4,2.4,15.2,15.2));

        // bevel
        QColor light = calcLightColor(color);
        QColor dark = calcDarkColor(color);
        qreal y = KColorUtils::luma(color);
        qreal yl = KColorUtils::luma(light);
        qreal yd = KColorUtils::luma(light);
        QLinearGradient bevelGradient(0, 0, 0, 18);
        bevelGradient.setColorAt(0.45, light);
        bevelGradient.setColorAt(0.65, dark);
        if (y < yl && y > yd) // no middle when color is very light/dark
            bevelGradient.setColorAt(0.55, color);
        p.setBrush(QBrush(bevelGradient));
        p.drawEllipse(QRectF(2.4,2.4,15.2,15.0));

        // inside mask
        QRadialGradient maskGradient(10,10,7.4,10,10);
        maskGradient.setColorAt(0.75, QColor(0,0,0,0));
        maskGradient.setColorAt(0.90, QColor(0,0,0,140));
        maskGradient.setColorAt(1.00, QColor(0,0,0,255));
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.setBrush(maskGradient);
        p.drawRect(0,0,20,20);

        // inside
        QLinearGradient innerGradient(0, 0, 0, 20);
        innerGradient.setColorAt(0.0, color);
        innerGradient.setColorAt(1.0, calcLightColor(color));
        p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        p.setBrush(innerGradient);
        p.drawEllipse(QRectF(2.5,2.5,15.0,14.8));

        m_roundCache.insert(key, pixmap);
    }

    return *pixmap;
}
