/* Copyright (C) 2003 by Sandro Giessl
 * based on the nice CVS KDesktop KShadowEngine class. thanks!
 * looking forward to see KShadowEngine in kdefx somewhen btw.. :)
 * ------------------------------------------------------------------------
 * these are the original copyright notes:
 * This file is proposed to be part of the KDE libraries.
 * Copyright (C) 2003 Laur Ivan <laurivan@eircom.net>
 *
 * Many thanks to:
 *  - Bernardo Hung <deciare@gta.igs.net> for the enhanced shadow
 *    algorithm (currently used)
 *  - Tim Jansen <tim@tjansen.de> for the API updates and fixes.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "shadow.h"
#include <qcolor.h>

ShadowEngine::ShadowEngine()
{
    thickness_ = 1;
    multiplicationFactor_ = 10.0;
}

ShadowEngine::~ShadowEngine()
{
}

QImage ShadowEngine::makeShadow(const QPixmap& textPixmap, const QColor &bgColor)
{
  QImage result;

  // create a new image for for the shaddow
  int w = textPixmap.width();
  int h = textPixmap.height();

  // avoid calling these methods for every pixel
  int bgRed = bgColor.red();
  int bgGreen = bgColor.green();
  int bgBlue = bgColor.blue();

  double alphaShadow;

  /*
   *	This is the source pixmap
   */
  QImage img = textPixmap.convertToImage().convertDepth(32);

  /*
   *	Resize the image if necessary
   */
  if ((result.width() != w) || (result.height() != h))
  {
    result.create(w, h, 32);
  }

  result.fill(0); // all black
  result.setAlphaBuffer(true);

  for (int i = thickness_; i < w - thickness_; i++)
  {
    for (int j = thickness_; j < h - thickness_; j++)
    {
        alphaShadow = decay(img, i, j);
        alphaShadow = (alphaShadow > 180.0) ? 180.0 : alphaShadow;
      // update the shadow's i,j pixel.
      result.setPixel(i,j, qRgba(bgRed, bgGreen , bgBlue, (int) alphaShadow));
    }
  }
  return result;
}

double ShadowEngine::decay(QImage& source, int i, int j)
{
  // create a new image for the shadow
  int w = source.width();
  int h = source.height();
  int sx, sy;

  double alphaShadow = 0;
  double opacity = 0;
  for (int k = 1; k <= thickness_; k++) {
    /* Generate a shadow thickness_ pixels thicker
     * on either side than the text image. Ensure
     * that i +/- k and j +/- k are within the
     * bounds of the text pixmap.
     */
    opacity = 0;
    for (int l = -k; l <= k; l++) {
      if (i < k)
	sx = 0;
      else if (i >= w - k)
	sx = w - 1;
      else
	sx = i + l;

      for (int m = -k; m <= k; m++) {
	if (j < k)
	  sy = 0;
	else if (j >= h - k)
	  sy = h - 1;
	else
	  sy = j + m;

	opacity += qGray(source.pixel(sx, sy));
      }
    }
    alphaShadow += opacity / multiplicationFactor_;
  }
  return alphaShadow;
}
