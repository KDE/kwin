// $Id: helper.h,v 1.6 2009/05/29 14:34:07 hpereira Exp $

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

#ifndef __NITROGEN_HELPER_H
#define __NITROGEN_HELPER_H

#include <ksharedconfig.h>
#include <kcomponentdata.h>

#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtGui/QLinearGradient>
#include <QtCore/QCache>


#define _glowBias 0.9 // not likely to be configurable

// WARNING - NitrogenHelper must be a K_GLOBAL_STATIC!
class NitrogenHelper
{
  
  public:
  
  //! constructor
  explicit NitrogenHelper(const QByteArray& );
  
  //! destructor
  virtual ~NitrogenHelper() {}

  //! configuration
  KSharedConfigPtr config() const;
  
  //! reload configuration
  void reloadConfig();
  
  //! window backbround
  void renderWindowBackground(
    QPainter *p, 
    const QRect &clipRect, 
    const QWidget *widget, 
    const QPalette & pal, 
    int gradient_shift = 0
    );
  
  //! clear cache
  virtual void invalidateCaches();
   
  //!@name color calculations
  // @{
  static bool lowThreshold(const QColor &color);
  static QColor alphaColor(QColor color, double alpha);
  QColor calcLightColor(const QColor &color) const;
  QColor calcDarkColor(const QColor &color) const;
  QColor calcShadowColor(const QColor &color) const;
  QColor backgroundColor(const QColor &color, int height, int y);
  QColor backgroundRadialColor(const QColor &color) const;
  QColor backgroundTopColor(const QColor &color) const;
  QColor backgroundBottomColor(const QColor &color) const;
  //@}
  
  //!@name gradients
  //@{
  QPixmap verticalGradient(const QColor &color, int height);
  QPixmap radialGradient(const QColor &color, int width, int y_offset = 0);  
  QLinearGradient decoGradient(const QRect &r, const QColor &color);
  //@}
  
  //! buttons
  QPixmap windecoButton(const QColor &color, bool pressed, int size = 21);
  QPixmap windecoButtonGlow(const QColor &color, int size = 21);
  
  //! frame
  void drawFloatFrame(QPainter *p, const QRect r, const QColor &color, bool drawUglyShadow=true, bool isActive=false, const QColor &frameColor=QColor()) const;
  
  //! separator
  void drawSeparator(QPainter *p, const QRect &r, const QColor &color, Qt::Orientation orientation) const;
  
  protected:
  
  void drawShadow(QPainter&, const QColor&, int size) const;
  static QPixmap glow(const QColor&, int size, int rsize);
  
  static const double _shadowGain;
  
  KComponentData _componentData;
  KSharedConfigPtr _config;
  qreal _contrast;
  qreal _bgcontrast;
  
  // cache
  QCache<quint64, QPixmap> m_backgroundCache;
  QCache<quint64, QPixmap> m_windecoButtonCache;
  QCache<quint64, QPixmap> m_windecoButtonGlowCache;

};

#endif // __NITROGEN_HELPER_H
