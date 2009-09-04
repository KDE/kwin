/*
* Copyright 2008 Long Huynh Huu <long.upcase@googlemail.com>
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

#include <KGlobalSettings>
#include <KColorUtils>
#include <KColorScheme>
#include <QWidget>
#include <QPainter>
#include <math.h>

#include "helper.h"


const double NitrogenHelper::_shadowGain = 1.5;

//_______________________________________________________________________________________________________________
NitrogenHelper::NitrogenHelper(const QByteArray &componentName): 
  _componentData(componentName, 0, KComponentData::SkipMainComponentRegistration)
{
  _config = _componentData.config();
  _contrast = KGlobalSettings::contrastF(_config);
  _bgcontrast = 0.3;
  
  m_backgroundCache.setMaxCost(64);
  m_windecoButtonCache.setMaxCost(64);
  m_windecoButtonGlowCache.setMaxCost(64);

}

//_______________________________________________________________________________________________________________
KSharedConfigPtr NitrogenHelper::config() const
{ return _config; }

//_______________________________________________________________________________________________________________
void NitrogenHelper::reloadConfig()
{
  double old_contrast = _contrast;
  
  _config->reparseConfiguration();
  _contrast = KGlobalSettings::contrastF(_config);
  
  // contrast changed, invalidate our caches
  if (_contrast != old_contrast) invalidateCaches(); 
  
}

//_______________________________________________________________________________________________________________
void NitrogenHelper::renderWindowBackground(
  QPainter *p, 
  const QRect &clipRect, 
  const QWidget *widget, 
  const QPalette & pal, 
  int gradient_shift )
{
    
  if (clipRect.isValid()) {
    p->save();
    p->setClipRegion(clipRect,Qt::IntersectClip);
  }
      
  QRect r = widget->window()->rect();
  
  // use an additional pixmap for buffering
  // to avoid flicker observe when painting large (wide) windows
  QPixmap pixmap( r.size() );
  {
    
    QPainter p( &pixmap );
    if (clipRect.isValid()) p.setClipRegion(clipRect);
    
    // get coordinates relative to the client area
    const QWidget* w = widget;
    int x = 0;
    int y = 0;
    
    // get window
    while (!w->isWindow()) 
    {
      x += w->geometry().x();
      y += w->geometry().y();
      w = w->window();
    }
    
    QColor color = pal.color( w->backgroundRole() );
    int splitY = qMin(300, 3*r.height()/4);
    
    QRect upperRect = QRect(-x, -y, r.width(), splitY);
    QPixmap tile = verticalGradient(color, splitY);
    p.drawTiledPixmap(upperRect, tile);
    
    QRect lowerRect = QRect(-x, splitY-y, r.width(), r.height() - splitY);
    p.fillRect(lowerRect, backgroundBottomColor(color));    
    int radialW = qMin(600, r.width());
    
    // on first paint the frame may not have been done yet, so just fixate it
    QRect radialRect = QRect((r.width() - radialW)/2 - x, -y, radialW, 64+gradient_shift);
    if (clipRect.intersects(radialRect))
    {
      tile = radialGradient(color, radialW, gradient_shift );
      p.drawPixmap(radialRect, tile);
    }
  
  }
  
  p->drawPixmap( r, pixmap );
  if (clipRect.isValid()) p->restore();
    
}

//_______________________________________________________________________________________________________________
void NitrogenHelper::invalidateCaches()
{
  m_backgroundCache.clear();
  m_windecoButtonCache.clear();
  m_windecoButtonGlowCache.clear();
}

//_______________________________________________________________________________________________________________
bool NitrogenHelper::lowThreshold(const QColor &color)
{
  QColor darker = KColorScheme::shade(color, KColorScheme::MidShade, 0.5);
  return KColorUtils::luma(darker) > KColorUtils::luma(color);
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::alphaColor(QColor color, double alpha)
{
  if (alpha < 1.0) color.setAlphaF( qMax(0.0, alpha) * color.alphaF() );
  return color;
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::backgroundRadialColor(const QColor &color) const
{
  return lowThreshold(color) ? 
    KColorScheme::shade(color, KColorScheme::LightShade, 0.0):
    KColorScheme::shade(color, KColorScheme::LightShade, _bgcontrast);
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::backgroundTopColor(const QColor &color) const
{
  return lowThreshold(color) ? 
    KColorScheme::shade(color, KColorScheme::MidlightShade, 0.0):
    KColorScheme::shade(color, KColorScheme::MidlightShade, _bgcontrast);
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::backgroundBottomColor(const QColor &color) const
{
  QColor midColor = KColorScheme::shade(color, KColorScheme::MidShade, 0.0);
  if (lowThreshold(color)) return midColor;
  
  double by = KColorUtils::luma(color), my = KColorUtils::luma(midColor);
  return KColorUtils::shade(color, (my - by) * _bgcontrast);
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::calcLightColor(const QColor &color) const
{ return KColorScheme::shade(color, KColorScheme::LightShade, _contrast); }

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::calcDarkColor(const QColor &color) const
{
  return lowThreshold(color) ? 
    KColorUtils::mix(calcLightColor(color), color, 0.2 + 0.8 * _contrast):
    KColorScheme::shade(color, KColorScheme::MidShade, _contrast);
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::calcShadowColor(const QColor &color) const
{
  return 
    KColorScheme::shade(KColorUtils::mix(QColor(255,255,255),color, color.alpha()*(1/255.0)),
    KColorScheme::ShadowShade, _contrast);
  
}

//_______________________________________________________________________________________________________________
QColor NitrogenHelper::backgroundColor(const QColor &color, int height, int y)
{
  double h = height * 0.5;
  if (y > height>>1) {
    
    double a = double(y) / h;
    return KColorUtils::mix(backgroundTopColor(color), color, a);
    
  } else {
    
    double a = (double(y) - h) / h;
    return KColorUtils::mix(color, backgroundBottomColor(color), a);
    
  }
}

//_______________________________________________________________________________________________________________
QPixmap NitrogenHelper::verticalGradient(const QColor &color, int height)
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

//_______________________________________________________________________________________________________________
QPixmap NitrogenHelper::radialGradient(const QColor &color, int width, int y_offset )
{
  quint64 key = (quint64(color.rgba()) << 32) | width | 0xb000;
  QPixmap *pixmap = m_backgroundCache.object(key);
  
  if (!pixmap)
  {
    pixmap = new QPixmap(width, 64+y_offset);
    pixmap->fill(Qt::transparent);
    QColor radialColor = backgroundRadialColor(color);

    // create gradient
    QRadialGradient gradient(64, 0, 64);
    radialColor.setAlpha(255); gradient.setColorAt(0, radialColor);
    radialColor.setAlpha(101); gradient.setColorAt(0.5, radialColor);
    radialColor.setAlpha(37);  gradient.setColorAt(0.75, radialColor);
    radialColor.setAlpha(0);   gradient.setColorAt(1, radialColor);
    
    // need some tr
    QPainter p(pixmap);
    p.translate( 0, y_offset );
    p.scale(width/128.0,1);
    p.fillRect(QRect(0,0,128,64+y_offset).translated(0, -y_offset ), gradient);
    
    m_backgroundCache.insert(key, pixmap);
  }
  
  return *pixmap;
}

//_______________________________________________________________________________________________________________
void NitrogenHelper::drawShadow(QPainter &p, const QColor &color, int size) const
{
  double m = double(size-2)*0.5;
  
  const double offset = 0.8;
  double k0 = (m-4.0) / m;
  QRadialGradient shadowGradient(m+1.0, m+offset+1.0, m);
  for (int i = 0; i < 8; i++) { // sinusoidal gradient
    double k1 = (k0 * double(8 - i) + double(i)) * 0.125;
    double a = (cos(3.14159 * i * 0.125) + 1.0) * 0.25;
    shadowGradient.setColorAt(k1, alphaColor(color, a * _shadowGain));
  }
  shadowGradient.setColorAt(1.0, alphaColor(color, 0.0));
  p.setBrush(shadowGradient);
  p.drawEllipse(QRectF(0, 0, size, size));
}

//_______________________________________________________________________________________________________________
QLinearGradient NitrogenHelper::decoGradient(const QRect &r, const QColor &color)
{

  QColor light = KColorScheme::shade(color, KColorScheme::LightShade, _contrast * 0.7);
  QColor dark = KColorScheme::shade(color, KColorScheme::DarkShade, _contrast * 0.7);
  
  double y = KColorUtils::luma(color);
  double yd = KColorUtils::luma(dark);
  double yl = KColorUtils::luma(light);
  
  QLinearGradient gradient(r.topLeft(), r.bottomLeft());
  if (yd > y)
  {
    gradient.setColorAt(0.2, color);
    gradient.setColorAt(0.8, dark);
  }
  else if (yl < y)
  {
    gradient.setColorAt(0.2, light);
    gradient.setColorAt(0.8, color);
  }
  else
  {
    gradient.setColorAt(0.2, dark);
    gradient.setColorAt(0.5, color);
    gradient.setColorAt(0.8, light);
  }
  
  return gradient;
}

//_______________________________________________________________________________________________________________
QPixmap NitrogenHelper::windecoButton(const QColor &color, bool pressed, int size)
{
  
  quint64 key = (quint64(color.rgba()) << 32) | (size << 1) | pressed;
  QPixmap *pixmap = m_windecoButtonCache.object(key);
  
  if (!pixmap)
  {
    pixmap = new QPixmap(size, size);
    pixmap->fill(Qt::transparent);
    
    QPainter p(pixmap);
    p.setRenderHints(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    
    double u = double(size)/22.0;
    QColor light  = calcLightColor(color);
    QColor dark   = calcDarkColor(color);
    QColor shadow = calcShadowColor(color);
    
    QRectF rect(0.0, 0.0, size, size);
    QRectF buttonRect = rect.adjusted(2*u,2*u,-2*u,-2*u);
    
    QLinearGradient fill(QPointF(0.0, 0.0*u), QPointF(0.0, 21.0*u));
    fill.setColorAt(0.0, alphaColor(light, 0.7));
    fill.setColorAt(1.0, alphaColor(dark, 0.7));
    
    p.setBrush(fill);
    p.drawEllipse(buttonRect);
    p.setBrush(Qt::NoBrush);
    
    // shadow
    if (pressed)
    {
      p.setPen(alphaColor(dark, 0.4));
      p.drawEllipse(buttonRect.adjusted(0.9, 0.6, -0.7, -0.7).adjusted(1.7,1.7,-1.7,-1.7));
      p.setPen(alphaColor(dark, 0.8));
      p.drawEllipse(buttonRect.adjusted(0.9, 0.6, -0.7, -0.7).adjusted(1.2,1.2,-1.2,-1.2));
    }
    p.setPen(QPen(KColorUtils::mix(dark, shadow, 0.12), 2.0));
    p.drawEllipse(buttonRect.adjusted(0.9, 0.6, -0.7, -0.7).adjusted(0,0.1,0,-0.1));
    p.setPen(QPen(KColorUtils::mix(dark, shadow, 0.6), 1.2));
    p.drawEllipse(buttonRect.adjusted(1.0, 1.4, -0.8, -0.8));
    
    // reflection
    QLinearGradient lightgr(QPointF(0.0, 0.0*u), QPointF(0.0, 21.0*u));
    lightgr.setColorAt(0.0, Qt::transparent);
    lightgr.setColorAt(1.0, light);
    p.setPen(QPen(lightgr, 1.7));
    p.drawEllipse(buttonRect.adjusted(0.0, -0.5, -0.1, 0.0));
    
    m_windecoButtonCache.insert(key, pixmap);
      
  }
  
  return *pixmap;
}

//_______________________________________________________________________________________________________________
QPixmap NitrogenHelper::windecoButtonGlow(const QColor &color, int size)
{
  quint64 key = (quint64(color.rgba()) << 32) | size;
  QPixmap *pixmap = m_windecoButtonGlowCache.object(key);
  
  if (!pixmap)
  {
    pixmap = new QPixmap(size, size);
    pixmap->fill(Qt::transparent);
    
    QPainter p(pixmap);
    p.setRenderHints(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    
    double u = size/21.0;
    QRectF rect(0.0, 0.0, size, size);
    QRectF buttonRect = rect.adjusted(2*u,2*u,-2*u,-2*u);
    
    //mask
    p.setBrush(QColor(0,0,0,125));
    p.drawEllipse(rect.adjusted(u, 0, -u, -2*u));
    
    //glow
    QColor dark  = calcDarkColor(color);
    QColor light = calcLightColor(color);
    
    QRadialGradient glow(QPointF(size/2.0, size/2.0 + 0.25), size/2.0);
    glow.setColorAt(12.0 / 21.0, Qt::transparent);
    glow.setColorAt(16.0 / 21.0, dark);
    glow.setColorAt(18.0 / 21.0, alphaColor(light, 0.25));
    glow.setColorAt(20.0 / 21.0, Qt::transparent);
    
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.setBrush(glow);
    p.drawEllipse(rect);
    
    m_windecoButtonGlowCache.insert(key, pixmap);
  }
  
  return *pixmap;

}
//_______________________________________________________________________________________________________________
QPixmap NitrogenHelper::glow(const QColor &color, int size, int rsize)
{
  QPixmap pixmap(rsize, rsize);
  pixmap.fill(QColor(0,0,0,0));
  
  QPainter p(&pixmap);
  p.setRenderHints(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setWindow(0,0,size,size);
  
  QRectF r(0, 0, size, size);
  double m = double(size)*0.5;
  
  const double width = 3.0;
  const double bias = _glowBias * double(size) / double(rsize);
  double k0 = (m-width+bias) / m;
  QRadialGradient glowGradient(m, m, m);
  
  for (int i = 0; i < 8; i++) 
  { 
  
    // inverse parabolic gradient
    double k1 = (k0 * double(8 - i) + double(i)) * 0.125;
    double a = 1.0 - sqrt(i * 0.125);
    glowGradient.setColorAt(k1, alphaColor(color, a));
  
  }
  
  glowGradient.setColorAt(1.0, alphaColor(color, 0.0));
  
  // glow
  p.setBrush(glowGradient);
  p.drawEllipse(r);
  
  // mask
  p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
  p.setBrush(QBrush(Qt::black));
  p.drawEllipse(r.adjusted(width, width, -width, -width));
  
  p.end();
  
  return pixmap;
}

//_______________________________________________________________________________________________________________
void NitrogenHelper::drawFloatFrame(QPainter *p, const QRect r, const QColor &color, bool drawUglyShadow, bool isActive, const QColor &frameColor) const
{
  p->save();
  p->setRenderHint(QPainter::Antialiasing);
  QRect frame = r;
  frame.adjust(1,1,-1,-1);
  int x,y,w,h;
  frame.getRect(&x, &y, &w, &h);
  
  QColor light = calcLightColor(backgroundTopColor(color));
  QColor dark = calcLightColor(backgroundBottomColor(color));
  QColor glow = KColorUtils::mix(QColor(128,128,128),frameColor,0.7);
  
  p->setBrush(Qt::NoBrush);
  
  if (drawUglyShadow) {
    if(isActive) { //window active - it's a glow - not a shadow
      p->setPen(glow);
      p->drawLine(QPointF(x+4, y-0.5), QPointF(x+w-4, y-0.5));
      p->drawArc(QRectF(x-0.5, y-0.5, 11, 11),90*16, 90*16);
      p->drawArc(QRectF(x+w-11+0.5, y-0.5, 11, 11), 0, 90*16);
      p->drawLine(QPointF(x-0.5, y+4), QPointF(x-0.5, y+h-4));
      p->drawLine(QPointF(x+w+0.5, y+4), QPointF(x+w+0.5, y+h-4));
      p->drawArc(QRectF(x-0.5, y+h-11+0.5, 11, 11),180*16, 90*16);
      p->drawArc(QRectF(x+w-11+0.5, y+h-11+0.5, 11, 11),270*16, 90*16);
      p->drawLine(QPointF(x+4, y+h+0.5), QPointF(x+w-4, y+h+0.5));
      light = KColorUtils::mix(light, frameColor);
      dark = KColorUtils::mix(dark, frameColor);
    }
    else { //window inactive - draw something resembling shadow
      QColor shadow = KColorUtils::darken(color, 0.0, 0.0); // fully desaturate
      p->setPen(KColorUtils::darken(shadow, 0.2));
      p->drawLine(QPointF(x+4, y-0.5), QPointF(x+w-4, y-0.5));
      p->drawArc(QRectF(x-0.5, y-0.5, 11, 11),90*16, 90*16);
      p->drawArc(QRectF(x+w-11+0.5, y-0.5, 11, 11), 0, 90*16);
      p->setPen(KColorUtils::darken(shadow, 0.35));
      p->drawLine(QPointF(x-0.5, y+4), QPointF(x-0.5, y+h-4));
      p->drawLine(QPointF(x+w+0.5, y+4), QPointF(x+w+0.5, y+h-4));
      p->setPen(KColorUtils::darken(shadow, 0.45));
      p->drawArc(QRectF(x-0.5, y+h-11+0.5, 11, 11),180*16, 90*16);
      p->drawArc(QRectF(x+w-11+0.5, y+h-11+0.5, 11, 11),270*16, 90*16);
      p->setPen(KColorUtils::darken(shadow, 0.6));
      p->drawLine(QPointF(x+4, y+h+0.5), QPointF(x+w-4, y+h+0.5));
    }
  }
  
  p->setPen(QPen(light, 0.8));
  p->drawLine(QPointF(x+4, y+0.6), QPointF(x+w-4, y+0.6));
  QLinearGradient lg = QLinearGradient(0.0, 1.5, 0.0, 4.5);
  lg.setColorAt(0, light);
  lg.setColorAt(1, dark);
  p->setPen(QPen(lg, 0.8));
  p->drawArc(QRectF(x+0.6, y+0.6, 9, 9),90*16, 90*16);
  p->drawArc(QRectF(x+w-9-0.6, y+0.6, 9, 9), 0, 90*16);
  p->drawLine(QPointF(x+0.6, y+4), QPointF(x+0.6, y+h-4));
  p->drawLine(QPointF(x+w-0.6, y+4), QPointF(x+w-0.6, y+h-4));
  p->restore();
}

//_______________________________________________________________________________________________________________
void NitrogenHelper::drawSeparator(QPainter *p, const QRect &rect, const QColor &color, Qt::Orientation orientation) const
{
  QColor light = calcLightColor(color);
  QColor dark = calcDarkColor(color);
  
  bool antialias = p->testRenderHint(QPainter::Antialiasing);
  p->setRenderHint(QPainter::Antialiasing,false);
  
  QPoint start,end,offset;
  
  if (orientation == Qt::Horizontal) {
    
    start = QPoint(rect.x(),rect.y()+rect.height()/2-1);
    end = QPoint(rect.right(),rect.y()+rect.height()/2-1);
    offset = QPoint(0,1);
    
  } else {
    
    start = QPoint(rect.x()+rect.width()/2-1,rect.y());
    end = QPoint(rect.x()+rect.width()/2-1,rect.bottom());
    offset = QPoint(1,0);
    light.setAlpha(150);
    
  }
  
  QLinearGradient lg(start,end);
  lg.setColorAt(0.3, dark);
  lg.setColorAt(0.7, dark);
  dark.setAlpha(0);
  lg.setColorAt(0.0, dark);
  lg.setColorAt(1.0, dark);
  p->setPen(QPen(lg,1));
  
  if (orientation == Qt::Horizontal) p->drawLine(start,end);
  else p->drawLine(start+offset,end+offset);
  
  lg = QLinearGradient(start,end);
  lg.setColorAt(0.3, light);
  lg.setColorAt(0.7, light);
  light.setAlpha(0);
  lg.setColorAt(0.0, light);
  lg.setColorAt(1.0, light);
  p->setPen(QPen(lg,1));
  
  
  if (orientation == Qt::Horizontal) p->drawLine(start+offset,end+offset);
  else  {
    
    p->drawLine(start,end);
    p->drawLine(start+offset*2,end+offset*2);
    
  }
  
  p->setRenderHint(QPainter::Antialiasing, antialias);
}

