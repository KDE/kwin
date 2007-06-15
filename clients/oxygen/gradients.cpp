/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas L�bking                             *
 *   thomas.luebking@web.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


/* ========= MAGIC NUMBERS ARE COOL ;) =================
Ok, we want to cache the gradients, but unfortunately we have no idea about
what kind of gradients will be demanded in the future
Thus creating a 2 component map (uint for color and uint for size)
would be some overhead and cause a lot of unused space in the dictionaries -
while hashing by a string is stupid slow ;)

So we store all the gradients by a uint index
Therefore we substitute the alpha component (byte << 24) of the demanded color
with the demanded size
As this would limit the size to 255/256 pixels we'll be a bit sloppy,
depending on the users resolution (e.g. use 0 to store a gradient with 2px,
usefull for demand of 1px or 2px) and shift the index
(e.g. gradients from 0 to 6 px size will hardly be needed -
maybe add statistics on this)
So the handled size is actually demandedSize + (demandedSize % sizeSloppyness),
beeing at least demanded size and the next sloppy size above at max
====================================================== */
static inline uint
hash(int size, const QColor &c, int *sloppyAdd) {
   
   uint magicNumber = 0;
   int sizeSloppyness = 1, frameBase = 0, frameSize = 20;
   while ((frameBase += frameSize) < size) {
      ++sizeSloppyness;
      frameSize += 20;
   }
      
   frameBase -=frameSize; frameSize -= 20;
   
   *sloppyAdd = size % sizeSloppyness;
   if (!*sloppyAdd)
      *sloppyAdd = sizeSloppyness;

   // first 11 bits to store the size, remaining 21 bits for the color (7bpc)
   magicNumber =  (((frameSize + (size - frameBase)/sizeSloppyness) & 0xff) << 21) |
      (((c.red() >> 1) & 0x7f) << 14) |
      (((c.green() >> 1) & 0x7f) << 7 ) |
      ((c.blue() >> 1) & 0x7f);
   
   return magicNumber;
}

static QPixmap*
newPix(int size, Qt::Orientation o, QPoint *start, QPoint *stop) {
   QPixmap *pix;
   if (o == Qt::Horizontal) {
      pix = new QPixmap(size, 32);
      *start = QPoint(0,32); *stop = QPoint(pix->width(),32);
   }
   else {
      pix = new QPixmap(32, size);
      *start = QPoint(32, 0); *stop = QPoint(32, pix->height());
   }
   return pix;
}

#define PREPARE_OXRENDER_GRADIENT QPoint start, stop; ColorArray colors; PointArray stops;\
QPixmap *pix = newPix(size, o, &start, &stop)

#define MAKE_OXRENDER_GRADIENT OXPicture grad = OXRender::gradient(start, stop, colors, stops);\
OXRender::composite (grad, X::None, *pix, 0, 0, 0, 0, 0, 0, pix->width(), pix->height());\
OXRender::freePicture(grad)

static inline QPixmap*
simpleGradient(const QColor &c, int size, Qt::Orientation o) {
   PREPARE_OXRENDER_GRADIENT;
   colors << c.light(100+config.gradientIntensity*60/100)
      << c.dark(100+config.gradientIntensity*20/100);
   MAKE_OXRENDER_GRADIENT;
   return pix;
}

static inline QPixmap *
sunkenGradient(const QColor &c, int size, Qt::Orientation o) {
   PREPARE_OXRENDER_GRADIENT;
   colors << c.dark(100+config.gradientIntensity*20/100)
      << c.light(100+config.gradientIntensity*60/100);
   MAKE_OXRENDER_GRADIENT;
   return pix;
}

static inline QPixmap *
buttonGradient(const QColor &c, int size, Qt::Orientation o) {
   
   PREPARE_OXRENDER_GRADIENT;
   
   int h,s,v, inc, dec;
   c.getHsv(&h,&s,&v);
   
   // calc difference
   inc = 15; dec = 6;
   if (v+15 > 255) {
      inc = 255-v; dec += (15-inc);
   }
   
   // make colors
   QColor ic;
   ic.setHsv(h,s,v+inc); colors << ic;
   ic.setHsv(h,s,v-dec); colors << ic;
   stops << 0 << 0.75;
   
   MAKE_OXRENDER_GRADIENT;
   return pix;
}

inline static void
gl_ssColors(const QColor &c, QColor *bb, QColor *dd, bool glass = false) {
   
   int h,s,v, ch,cs,cv, delta, add;
   
   c.getHsv(&h,&s,&v);

   // calculate the variation
   add = ((180-qGray(c.rgb()))>>1);
   if (add < 0) add = -add/2;
   if (glass)
      add = add>>4;

   // the brightest color (top)
   cv = v+27+add;
   if (cv > 255) {
      delta = cv-255; cv = 255;
      cs = s - delta; if (cs < 0) cs = 0;
      ch = h - delta/6; if (ch < 0) ch = 360+ch;
   }
   else {
      ch = h; cs = s;
   }
   bb->setHsv(ch,cs,cv);
   
   // the darkest color (lower center)
   cv = v - 14-add; if (cv < 0) cv = 0;
   cs = s*13/7; if (cs > 255) cs = 255;
   dd->setHsv(h,cs,cv);
}

static inline QPixmap *
gl_ssGradient(const QColor &c, int size, Qt::Orientation o, bool glass = false) {
   QColor bb,dd; // b = d = c;
   gl_ssColors(c, &bb, &dd, glass);
   QPoint start, stop;
   QPixmap *pix = newPix(size, o, &start, &stop);
   
#if 0
   ColorArray colors; PointArray stops;
   colors << bb << b << dd << d;
   stops << 0 <<  0.5 << 0.5 << 1;
#else
   // many xrender imps are unable to create gradients with #stops > 2
   // so we "fall back" to the qt software solution here - for the moment...
   QLinearGradient lg(start, stop);
   lg.setColorAt(0,bb); lg.setColorAt(0.5,c);
   lg.setColorAt(0.5, dd); lg.setColorAt(glass ? 1 : .90, bb);
   QPainter p(pix); p.fillRect(pix->rect(), lg); p.end();
#endif
   return pix;
}

static inline QPixmap *
rGlossGradient(const QColor &c, int size) {
   QColor bb,dd; // b = d = c;
   gl_ssColors(c, &bb, &dd);
   QPixmap *pix = new QPixmap(size, size);
   
#if 0
   ColorArray colors; PointArray stops;
   colors << bb << b << dd << d;
   stops << 0 <<  0.5 << 0.5 << 1;
#else
   // many xrender imps are unable to create gradients with #stops > 2
   // so we "fall back" to the qt software solution here - for the moment...
      QRadialGradient rg(2*pix->width()/3, pix->height(), pix->height());
      rg.setColorAt(0,c); rg.setColorAt(0.8,dd);
      rg.setColorAt(0.8, c); rg.setColorAt(1, bb);
      QPainter p(pix); p.fillRect(pix->rect(), rg); p.end();
#endif
   return pix;
}

const QPixmap&
OxygenStyle::gradient(const QColor &c, int size, Qt::Orientation o, GradientType type) const {
   // validity check
   if (size <= 0) {
      qWarning("NULL Pixmap requested, size was %d",size);
      return nullPix;
   }
   else if (size > 105883) { // this is where our dictionary reaches - should be enough for the moment ;)
      qWarning("gradient with more than 105883 steps requested, returning NULL pixmap");
      return nullPix;
   }
   
   // very dark colors won't make nice buttons =)
   QColor iC = c;
   int v = colorValue(c);
   if (v < 80) {
      int h,s;
      c.getHsv(&h,&s,&v);
      iC.setHsv(h,s,80);
   }

   // hash 
   int sloppyAdd = 1;
   uint magicNumber = hash(size, iC, &sloppyAdd);

   PixmapCache *cache =
      &(const_cast<OxygenStyle*>( this )->gradients[o == Qt::Horizontal][type]);
   QPixmap *pix = cache->object(magicNumber);
   if (pix)
      return *pix;
   
   // no cache entry found, so let's create one
   size += sloppyAdd; // rather to big than to small ;)
   switch (type) {
   case GradButton:
      pix = buttonGradient(iC, size, o);
      break;
   case GradGlass:
      pix = gl_ssGradient(iC, size, o, true);
      break;
   case GradSimple:
   default:
      pix = simpleGradient(iC, size, o);
      break;
   case GradSunken:
      pix = sunkenGradient(iC, size, o);
      break;
   case GradGloss:
      pix = gl_ssGradient(iC, size, o);
      break;
   case GradRadialGloss:
      pix = rGlossGradient(iC, size);
      break;
   }

   // cache for later
   cache->insert(magicNumber, pix, (pix->width()*pix->height()*pix->depth())>>3);
   return *pix;
}

const QPixmap &OxygenStyle::groupLight(int height) const {
   if (height <= 0) {
      qWarning("NULL Pixmap requested, height was %d",height);
      return nullPix;
   }
   QPixmap *pix = _groupLight.object(height);
   if (pix)
      return *pix;
      
   pix = new QPixmap(32, height); //golden mean relations
   pix->fill(Qt::transparent);
   QPoint start(0,0), stop(0,height);
   PointArray stops;
   ColorArray colors = ColorArray() << QColor(255,255,255,50) << QColor(255,255,255,0);
   MAKE_OXRENDER_GRADIENT;
   
   // cache for later ;)
   PixmapCache *cache = &(const_cast<OxygenStyle*>( this )->_groupLight);
   cache->insert(height, pix);
   return *pix;
}

const QPixmap &OxygenStyle::btnAmbient(int height) const {
   if (height <= 0) {
      qWarning("NULL Pixmap requested, height was %d",height);
      return nullPix;
   }
   QPixmap *pix = _btnAmbient.object(height);
   if (pix)
      return *pix;
      
   pix = new QPixmap(16*height/9,height); //golden mean relations
   pix->fill(Qt::transparent);
   ColorArray colors = ColorArray() << QColor(255,255,255,128) << QColor(255,255,255,0);
   OXPicture grad = OXRender::gradient(QPoint(pix->width(), pix->height()),
                                       QPoint(pix->width()/2,pix->height()/2), colors);
   OXRender::composite (grad, None, *pix, 0, 0, 0, 0, 0, 0, pix->width(), pix->height());
   OXRender::freePicture(grad);
   
   // cache for later ;)
   PixmapCache *cache = &(const_cast<OxygenStyle*>( this )->_btnAmbient);
   cache->insert(height, pix);
   return *pix;
}

const QPixmap &OxygenStyle::tabShadow(int height, bool bottom) const {
   if (height <= 0) {
      qWarning("NULL Pixmap requested, height was %d",height);
      return nullPix;
   }
   uint val = height + bottom*0x80000000;
   QPixmap *pix = _tabShadow.object(val);
   if (pix)
      return *pix;
      
   pix = new QPixmap(height/3,height);
   pix->fill(Qt::transparent);
   ColorArray colors = ColorArray() << QColor(0,0,0,75) << QColor(0,0,0,0);
   float hypo = sqrt(pow(pix->width(),2)+pow(pix->height(),2));
   float cosalpha = (float)(pix->height())/hypo;
   QPoint p1, p2;
   if (bottom) {
      p1 = QPoint(0, 0);
      p2 = QPoint((int)(pix->width()*pow(cosalpha, 2)),
                  (int)(pow(pix->width(), 2)*cosalpha/hypo));
   }
   else {
      p1 = QPoint(0, pix->height());
      p2 = QPoint((int)(pix->width()*pow(cosalpha, 2)),
                  (int)pix->height() - (int)(pow(pix->width(), 2)*cosalpha/hypo));
   }
   OXPicture grad = OXRender::gradient(p1, p2, colors);
   OXRender::composite (grad, None, *pix, 0, 0, 0, 0, 0, 0, pix->width(), pix->height());
   OXRender::freePicture(grad);
   
   // cache for later ;)
   PixmapCache *cache = &(const_cast<OxygenStyle*>( this )->_tabShadow);
   cache->insert(val, pix);
   return *pix;
}
