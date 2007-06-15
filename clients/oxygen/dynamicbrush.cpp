/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas Lübking                             *
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

#include <QApplication>
#include <QDesktopWidget>
#include <QResizeEvent>
#include <QTimer>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QX11Info>
#include <QtDebug>

#define GL_GLEXT_PROTOTYPES
#include <QGLFramebufferObject>
// #include <QGLPixelBuffer>
// #include <GL/glut.h>

#include <X11/Xatom.h>
#include <cmath>

#include "oxrender.h"

#include "dynamicbrush.h"

static BgPixCache tlwbacks;
static BgSet _bgSet;

// #include "endian.h"

#define SETBACKGROUND(_pix_)\
   const_cast<QBrush*>(&QApplication::palette().brush(QPalette::Active, QPalette::Window))->setTexture(_pix_);\
   const_cast<QBrush*>(&QApplication::palette().brush(QPalette::Inactive, QPalette::Window))->setTexture(_pix_);\
   const_cast<QBrush*>(&QApplication::palette().brush(QPalette::Disabled, QPalette::Window))->setTexture(_pix_)

#ifndef CLAMP
#define CLAMP(x,l,u) (x) < (l) ? (l) :\
(x) > (u) ? (u) :\
(x)
#endif

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

static QPixmap mirror(const QPixmap &pix, Qt::Orientation o)
{
   Display *dpy = QX11Info::display();
   QPixmap mirror(pix.size());
   GC gc = XCreateGC( dpy, mirror.handle(), 0, 0 );
   if (o == Qt::Horizontal)
   {
      int x = pix.width() - 1;
      for (int i = 0; i < pix.width(); ++i)
         XCopyArea( dpy, pix.handle(), mirror.handle(), gc,
                    x-i, 0, 1, pix.height(), i, 0 );
   }
   else
   {
      int y = pix.height() - 1;
      for (int i = 0; i < pix.height(); ++i)
         XCopyArea( dpy, pix.handle(), mirror.handle(), gc,
                     0, y-i, pix.width(), 1, 0, i );
   }
   XFreeGC ( dpy , gc );
   return mirror;
}

static const Atom oxygen_isQtWindow =
XInternAtom(QX11Info::display(), "OXYGEN_IS_QT_WINDOW", False);

static GLuint _background;

const BgSet* (DynamicBrush::*bgSet)(const QSize&, bool);

BgSet::BgSet() {
   window = decoTop = decoBottom = decoLeft = decoRight = 0L;
}

BgSet::BgSet(const QPixmap* pix, int dx, int dy, int w, int h,
                      bool updateDeco)
{
   window = decoTop = decoBottom = decoLeft = decoRight = 0L;
   setPixmap(pix, dx, dy, w, h, updateDeco);
}

BgSet::BgSet(const QPixmap* pix, int d, int s, Qt::Orientation o,
                      bool updateDeco)
{
   window = decoTop = decoBottom = decoLeft = decoRight = 0L;
   setPixmap(pix, d, s, o, updateDeco);
}

void BgSet::setPixmap(const QPixmap* pix, int dx, int dy, int w, int h,
                      bool updateDeco)
{
   
   if (window) delete window;
   window = new QPixmap(w, h);
   QPainter p(window);
   p.drawPixmap(0,0, w, h, *pix, dx, dy, w, h);
   p.end();
   
   if (!updateDeco)
      return;
   
   int iW, iH;
   
#define NEW_PIX(_PIX_, _W_, _H_) if (_PIX_) delete _PIX_; \
   _PIX_ = new QPixmap(_W_, _H_); if (!_PIX_->isNull()) { p.begin(_PIX_)
   
   NEW_PIX(decoTop, w, dy);
   p.drawPixmap(0, 0, w, dy, *pix, dx, 0, w, dy); p.end(); }
   
   iH = pix->height() - (dy + h);
   NEW_PIX(decoBottom, w, iH);
   p.drawPixmap(0, 0, w, iH, *pix, dx, pix->height()-iH, w, iH); p.end(); }
   
   iH = pix->height();
   NEW_PIX(decoLeft, dx, iH);
   p.drawPixmap(0, 0, dx, iH, *pix, 0, 0, dx, iH); p.end(); }
   
   iW = pix->width() - (dx + w);
   NEW_PIX(decoRight, iW, pix->height());
   p.drawPixmap(0, 0, iW, iH, *pix, pix->width()-iW, 0, iW, iH); p.end(); }
   
}

void BgSet::setPixmap(const QPixmap* pix, int d, int s, Qt::Orientation o,
                      bool updateDeco)
{
   QPainter p; int w,h;
   if (o == Qt::Vertical) {
      w = pix->width();
      NEW_PIX(window, w, s);
      p.drawPixmap(0,0, w, s, *pix, 0, d, w, s); p.end(); }
      
      if (!updateDeco)
         return;
      
      NEW_PIX(decoTop, w, d);
      p.drawPixmap(0, 0, w, d, *pix, 0, 0, w, d); p.end(); }
      
      h = pix->height() - (d + s);
      NEW_PIX(decoBottom, w, h);
      p.drawPixmap(0, 0, w, h, *pix, 0, pix->height()-h, w, h); p.end(); }
      
      h = pix->height();
      NEW_PIX(decoLeft, w, h);
      p.drawPixmap(0, 0, w, h, *pix, 0, 0, w, h); p.end(); }

      decoRight = decoLeft;
      
   }
   else {
      
      h = pix->height();
      NEW_PIX(window, s, h);
      p.drawPixmap(0,0, s, h, *pix, d, 0, s, h); p.end(); }
      
      if (!updateDeco)
         return;
      
      NEW_PIX(decoTop, s, h);
      p.drawPixmap(0, 0, s, h, *pix, 0, 0, s, h); p.end(); }
      
      decoBottom = decoTop;
      
      NEW_PIX(decoLeft, s, h);
      p.drawPixmap(0, 0, s, h, *pix, 0, 0, s, h); p.end(); }

      w = pix->width() - (d + s);
      NEW_PIX(decoRight, w, h);
      p.drawPixmap(0, 0, w, h, *pix, pix->width()-w, 0, w, h); p.end(); }

   }
   
#undef NEW_PIX
}

void BgSet::wipe() {
   if (window) { delete window; window = 0L; }
   if (decoBottom && decoBottom != decoTop) { delete decoBottom; decoBottom = 0L; }
   if (decoTop) { delete decoTop; decoTop = 0L; }
   if (decoRight && decoRight != decoLeft) { delete decoRight; decoRight = 0L; }
   if (decoLeft) { delete decoLeft; decoLeft = 0L; }
}

BgSet::~BgSet() {
   wipe();
}

DynamicBrush::DynamicBrush(Mode mode, QObject *parent) : QObject(parent),
_glContext(0), _mode(mode), _isActiveWindow(true)
{
   for (int i = 0; i < 4; ++i) decoDim[i] = 0;
   _timerBgWipe = new QTimer(this);
   connect (_timerBgWipe, SIGNAL(timeout()), this, SLOT(wipeBackground()));
   switch (_mode)
   {
   case Tiled: bgSet = &DynamicBrush::bgSetTiled; break;
   case XRender: bgSet = &DynamicBrush::bgSetRender; break;
   case QtGradient: bgSet = &DynamicBrush::bgSetQt; break;
   case OpenGL:
      bgSet = &DynamicBrush::bgSetGL;
      _glContext = new QGLWidget( QGLFormat::defaultFormat(), 0 );
      initGL();
      break;
   case HGradient1:
   case VGradient1: bgSet = &DynamicBrush::bgSetGradient1; generateTiles(mode); break;
   case HGradient2:
   case VGradient2: bgSet = &DynamicBrush::bgSetGradient2; generateTiles(mode); break;
   case Glass: bgSet = &DynamicBrush::bgSetGlass; generateTiles(mode); break;
   }
}

DynamicBrush::DynamicBrush(OXPixmap pixmap, int bgYoffset, QObject *parent) :
QObject(parent), _pixmap(pixmap), _bgYoffset(bgYoffset), _glContext(0), _mode(Tiled)
{
   for (int i = 0; i < 4; ++i) decoDim[i] = 0;
   _timerBgWipe = new QTimer(this);
   connect (_timerBgWipe, SIGNAL(timeout()), this, SLOT(wipeBackground()));
   bgSet = &DynamicBrush::bgSetTiled;
}

DynamicBrush::~DynamicBrush() {
   glDeleteLists( _background, 1 );
//    QObject::~QObject();
}

static const Atom oxygen_decoDim =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_DIM", False);

void DynamicBrush::readDecoDim(QWidget *topLevelWidget) {
   unsigned char *data = 0;
   Atom actual;
   int format, result;
   unsigned long n, left;
   result = XGetWindowProperty(QX11Info::display(), topLevelWidget->winId(),
                               oxygen_decoDim, 0L, 1L, False, XA_CARDINAL,
                               &actual, &format, &n, &left, &data);
   if (result == Success && data != None) { // found set design
      uint tmp;
      memcpy (&tmp, data, sizeof (unsigned int));
      decoDim[0] = tmp & 0xff;
      decoDim[1] = (tmp >> 8) & 0xff;
      decoDim[2] = (tmp >> 16) & 0xff;
      decoDim[3] = (tmp >> 24) & 0xff;
   }
}

void DynamicBrush::setMode(Mode mode) {
   if (mode == _mode) return;
   if (_mode == OpenGL) {
      glDeleteLists( _background, 1 );
      delete _glContext;
      _glContext = 0L;
   }
   _mode = mode;
   switch (_mode) {
   case Tiled: bgSet = &DynamicBrush::bgSetTiled; break;
   case XRender: bgSet = &DynamicBrush::bgSetRender; break;
   case QtGradient: bgSet = &DynamicBrush::bgSetQt; break;
   case OpenGL:
      bgSet = &DynamicBrush::bgSetGL;
      if (!_glContext) {
         _glContext = new QGLWidget( QGLFormat::defaultFormat(), 0 );
         initGL();
      }
      break;
   case HGradient1:
   case VGradient1: bgSet = &DynamicBrush::bgSetGradient1; generateTiles(mode); break;
   case HGradient2:
   case VGradient2: bgSet = &DynamicBrush::bgSetGradient2; generateTiles(mode); break;
   case Glass: bgSet = &DynamicBrush::bgSetGlass; generateTiles(mode); break;
   }
}

void DynamicBrush::generateTiles(Mode mode) {
   QLinearGradient lg1, lg2;
   QPainter p;
   for (int i = 0; i < 2; ++i)
   {
      _bgC[i] = QApplication::palette().color(i?QPalette::Active:QPalette::Inactive, QPalette::Window);
      switch (mode)
      {
      case HGradient1: {
         _tile[i][0] = QPixmap(64,32);
         _tile[i][1] = QPixmap(64,32);
         QColor dark = _bgC[i].dark(105);
         lg1 = QLinearGradient(QPoint(0,0), QPoint(64, 0));
         lg1.setColorAt(0, dark); lg1.setColorAt(1, _bgC[i]);
         lg2 = QLinearGradient(QPoint(0,0), QPoint(64, 0));
         lg2.setColorAt(0, _bgC[i]); lg2.setColorAt(1, dark);
         break;
      }
      case VGradient1: {
         _tile[i][0] = QPixmap(32,64);
         _tile[i][1] = QPixmap(32,64);
         lg1 = QLinearGradient(QPoint(0,0), QPoint(0, 64));
         lg1.setColorAt(0, _bgC[i].light(105)); lg1.setColorAt(1, _bgC[i]);
         lg2 = QLinearGradient(QPoint(0,0), QPoint(0, 64));
         lg2.setColorAt(0, _bgC[i]); lg2.setColorAt(1, _bgC[i].dark(105));
         break;
      }
      case HGradient2: {
         _tile[i][0] = QPixmap(512,32);
         _tile[i][1] = QPixmap(512,32);
         QColor light = _bgC[i].light(104);
         lg1 = QLinearGradient(QPoint(0,0), QPoint(512, 0));
         lg1.setColorAt(0, _bgC[i]); lg1.setColorAt(1, light);
         lg2 = QLinearGradient(QPoint(0,0), QPoint(512, 0));
         lg2.setColorAt(0, light); lg2.setColorAt(1, _bgC[i]);
         break;
      }
      case VGradient2: {
         _tile[i][0] = QPixmap(32, 512);
         _tile[i][1] = QPixmap(32, 512);
         QColor light = _bgC[i].light(104);
         lg1 = QLinearGradient(QPoint(0,0), QPoint(0, 512));
         lg1.setColorAt(0, _bgC[i]); lg1.setColorAt(1, light);
         lg2 = QLinearGradient(QPoint(0,0), QPoint(0, 512));
         lg2.setColorAt(0, light); lg2.setColorAt(1, _bgC[i]);
         break;
      }
      default:
         _tile[i][0] = _tile[i][1] = QPixmap();
         return;
      }
      p.begin(&_tile[i][0]);
      p.fillRect(_tile[i][0].rect(), lg1);
      p.end();
      p.begin(&_tile[i][1]);
      p.fillRect(_tile[i][1].rect(), lg2);
      p.end();
   }
}

static const Atom oxygen_deco_top =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_TOP", False);
static const Atom oxygen_deco_bottom =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_BOTTOM", False);
static const Atom oxygen_deco_left =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_LEFT", False);
static const Atom oxygen_deco_right =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_RIGHT", False);

#define EXPORT_DECO_PIX(_ATOM_, _PIX_) \
if (!set->_PIX_->isNull()) { \
card = set->_PIX_->x11PictureHandle();\
XRenderChangePicture(QX11Info::display(), card, CPRepeat, &pa);\
XChangeProperty(QX11Info::display(), widget->winId(), _ATOM_, XA_CARDINAL, 32,\
PropModeReplace, (unsigned char *) &(card), 1L);\
}

bool DynamicBrush::eventFilter ( QObject * object, QEvent * ev )
{
   // Rule out non widgets (this should never happen, but who knows...)
   if (!qobject_cast<QWidget*>(object)) {
      object->removeEventFilter(this);
      return false;
   }
   
   QWidget *widget = (QWidget*)object;
   
   // we're not interested in hidden elements
   if (!widget->isVisible()) return false;
   // or in "opaque" ones - like e.g. a textinput, scrollview etc.
   if (widget->autoFillBackground()) return false;
      
   // Test for the currently demanded bg size
   QSize size;
   bool triggerUpdate = false;
   if (ev->type() == QEvent::UpdateRequest) {
      _topLevelWidget = widget->topLevelWidget();
      size = _topLevelWidget->size();
   }
   else if (widget->isWindow()) {
      _topLevelWidget = widget;
      if (ev->type() == QEvent::Resize) {
         readDecoDim(widget);
         triggerUpdate = true;
         size = ((QResizeEvent*)ev)->size();
      }
      else if ( ev->type() == QEvent::WindowActivate ||
                ev->type() == QEvent::WindowDeactivate ) {
         readDecoDim(widget);
         size = ((QWidget*)object)->size();
      }
      else if (ev->type() == QEvent::Show) {
         readDecoDim(widget);
         size = ((QWidget*)object)->size();
         triggerUpdate = true;
      }
      else if (ev->type() == QEvent::Hide)
         size = QSize();
      else
         return false;
   }
   else
      return false;
   
   // we don't need a bg pix for the moment, let's save some RAM
   if (size.isNull()) {
      wipeBackground();
      tlwbacks.remove(_topLevelWidget);
      return false;
   }
   
   // the timer will wipe the background, if it's not updated since 7.777 secs
   // (maybe the window is completely covered)
   _timerBgWipe->start(7777);
   
   // In case the demanded size differs from the one we have: make an update ;)
   bool needNewPix = true;
   if (_mode == VGradient1 || _mode == VGradient2 || _mode == Glass)
      needNewPix = (_size.height() != size.height());
   else if (_mode == HGradient1 || _mode == HGradient2)
      needNewPix = (_size.width() != size.width());
   else // complex etc.
      needNewPix = (_size != size);
   
   if (needNewPix) {
      _size = size;
      QSize fullSize = size + QSize(decoDim[2]+decoDim[3],
                                    decoDim[0]+decoDim[1]);
      const BgSet* set = (this->*bgSet)(fullSize, triggerUpdate);

      SETBACKGROUND(*(set->window));

      if (triggerUpdate) { // tell the deco about new pics
         int card;
         XRenderPictureAttributes pa;
         pa.repeat = True;
         EXPORT_DECO_PIX(oxygen_deco_top, decoTop);
         EXPORT_DECO_PIX(oxygen_deco_bottom, decoBottom);
         EXPORT_DECO_PIX(oxygen_deco_left, decoLeft);
         EXPORT_DECO_PIX(oxygen_deco_right, decoRight);
      }
   }
   
   // Maybe we better force an update
   if (triggerUpdate)
      widget->update();
   
   return false;
}

#undef EXPORT_DECO_PIX

void DynamicBrush::wipeBackground()
{
   _timerBgWipe->stop();
   _size = QSize();
   QPixmap qPix;
   SETBACKGROUND(qPix);
   tlwbacks.clear();
}

BgPixCache::iterator DynamicBrush::checkCache(bool &found) {
   found = false;
   BgPixCache::iterator i, tlw = tlwbacks.end(), sm = tlwbacks.end();
   QPixmap *pix = false;
   for (i = tlwbacks.begin(); i != tlwbacks.end(); ++i) {
      pix = i.value()->window;
      if (pix && pix->size() == _size) {
         sm = i;
         if (tlw != tlwbacks.end()) break;
      }
      if (i.key() == _topLevelWidget) {
         tlw = i;
         if (sm != tlwbacks.end()) break;
      }
   }
   if (sm != tlwbacks.end()) { // we found a match
      if (sm != tlw) { // from another window with the same size: share!
         if (tlw == tlwbacks.end()) // new top level widget -> insert
            tlw = tlwbacks.insert(_topLevelWidget, sm.value());
         else // just share
            tlw.value() = sm.value();
      }
      // anyway, we have a pixmap
      found = true;
   }
   return tlw;
}

const BgSet* DynamicBrush::bgSetTiled(const QSize &size, bool updateDeco)
{
   return &_bgSet;
#if 0
   Display *dpy = QX11Info::display();
   int ttbh = 22;
   int height = _bgYoffset - ttbh;
   int width = _size.width();
   int xb = QApplication::desktop()->width() - width;
   int x = xb/3;
   int w_2 = (width >> 1);
   /* The following is a bit tricky.
   we want a complex background including three different gradients (light sources)
   to gain this, we created a desktop sized pixmap and now extract parts from it to
   create a new window sized pixmap.
   In our particular case we want to shape away 2/3 from the sides and 1/3 from the
   center - in the upper region. below that, we take a center part
   (i.e.shape away 1/2 of the difference from each side, this could be smarter)
   */
   QPixmap qPix(_size);
   // first the left and right top part ==================
   GC gc = XCreateGC( dpy, qPix.handle(), 0, 0 );
   XCopyArea( dpy, _pixmap, qPix.handle(), gc, x, ttbh, w_2, height, 0, 0 );
   x = (x<<1) + w_2;
   XCopyArea( dpy, _pixmap, qPix.handle(), gc, x, ttbh, width-w_2, height, w_2, 0 );
   // ===================================
   // now the bottom center part ===========================
   XCopyArea( dpy, _pixmap, qPix.handle(), gc, xb>>1, _bgYoffset, width, _size.height() - height, 0, height );
   XFreeGC ( dpy , gc );
   
   /* update  the brush textures*/
   SETBACKGROUND(qPix);
#endif
}

const BgSet* DynamicBrush::bgSetGradient1(const QSize &size, bool updateDeco)
{
   QPixmap *qPix;
   if (_mode == VGradient1)
   {
      qPix = new QPixmap(32, size.height());
      qPix->fill(_bgC[_isActiveWindow]);
      QPainter p(qPix);
      int h = qMin(64,(qPix->height()+1)/2);
      p.drawTiledPixmap( 0, 0, 32, h, _tile[_isActiveWindow][0], 0, 64-h );
      h = qMin(64,(qPix->height())/2);
      p.drawTiledPixmap( 0, qPix->height()-h, 32, h, _tile[_isActiveWindow][1] );
      p.end();
      _bgSet.setPixmap(qPix, decoDim[0], _size.height(), Qt::Vertical, updateDeco);
   }
   else // horizontal
   {
      qPix = new QPixmap(size.width(), 32);
      qPix->fill(_bgC[_isActiveWindow]);
      QPainter p(qPix);
      int w = qMin(64,(qPix->width()+1)/2);
      p.drawTiledPixmap( 0, 0, w, 32, _tile[_isActiveWindow][0], 64-w );
      w = qMin(64,(qPix->width())/2);
      p.drawTiledPixmap( qPix->width()-w, 0, w, 32, _tile[_isActiveWindow][1] );
      p.end();
      _bgSet.setPixmap(qPix, decoDim[2], _size.width(), Qt::Horizontal, updateDeco);
   }
   return &_bgSet;
}

const BgSet* DynamicBrush::bgSetGradient2(const QSize &size, bool updateDeco)
{
   QPixmap *qPix;
   if (_mode == VGradient2)
   {
      qPix = new QPixmap(32, size.height());
      qPix->fill(_bgC[_isActiveWindow]);
      QPainter p(qPix);
      int h = qMin(512,(qPix->height()+1)/2);
      int h_2 = qPix->height()/2;
      p.drawTiledPixmap( 0, h_2-h, 32, h, _tile[_isActiveWindow][0], 0, (512-h)/2 );
      h = qMin(512, h_2);
      p.drawTiledPixmap( 0, h_2, 32, h, _tile[_isActiveWindow][1], 0, (512-h)/2 );
      p.end();
      _bgSet.setPixmap(qPix, decoDim[0], _size.height(), Qt::Vertical, updateDeco);
   }
   else // horizontal
   {
      qPix = new QPixmap(size.width(), 32);
      qPix->fill(_bgC[_isActiveWindow]);
      QPainter p(qPix);
      int w = qMin(512,(qPix->width()+1)/2);
      int w_2 = qPix->width()/2;
      p.drawTiledPixmap( w_2-w, 0, w, 32, _tile[_isActiveWindow][0], (512-w)/2 );
      w = qMin(512,w_2);
      p.drawTiledPixmap( w_2, 0, w, 32, _tile[_isActiveWindow][1], (512-w)/2 );
      p.end();
      _bgSet.setPixmap(qPix, decoDim[2], _size.width(), Qt::Horizontal, updateDeco);
   }
   return &_bgSet;
}

const BgSet* DynamicBrush::bgSetGlass(const QSize &size, bool updateDeco)
{
   // first check the cache
   bool found;
   BgPixCache::iterator tlw = checkCache(found);
   if (found)
      return tlw.value();
   
   // nope, create one
   QPixmap *qPix = new QPixmap(32, size.height());
   QColor c = QApplication::palette().color(QPalette::Window);
   int h,s, v;
   int ch,cs,cv, delta, add;
   QColor dd,bb;
   c.getHsv(&h,&s,&v);
   if (v < 80) { // very dark colors won't make nice backgrounds ;)
      v = 80;
      c.setHsv(h,s,80);
   }
   

   add = ((180-qGray(c.rgb()))>>1);
   if (add < 0)
      add = -add/2;
   add = add>>4;

   // bb
   cv = v+27+add;
   if (cv > 255) {
      delta = cv-255; cv = 255;
      cs = s - delta; if (cs < 0) cs = 0;
      ch = h - delta/6; if (ch < 0) ch = 360+ch;
   }
   else {
      ch = h; cs = s;
   }
   bb.setHsv(ch,cs,cv);
      
   //dd
   cv = v - 14-add; if (cv < 0) cv = 0;
   cs = s*13/7; if (cs > 255) cs = 255;
   dd.setHsv(h,cs,cv);
      
   // gradient
   QLinearGradient lg(QPoint(0,0), QPoint(0, qPix->height()));
   lg.setColorAt(0,bb); lg.setColorAt(0.5,c);
   lg.setColorAt(0.5, dd); lg.setColorAt(1, c);
   QPainter p(qPix); p.fillRect(qPix->rect(), lg); p.end();
   
   // cache
   if (tlw == tlwbacks.end())
      tlw = tlwbacks.insert(_topLevelWidget, new BgSet(qPix, decoDim[0],
         _size.height(), Qt::Vertical));
   else
      tlw.value()->setPixmap(qPix, decoDim[0], _size.height(),
                             Qt::Vertical, updateDeco);
   
   // pixmap can be deleted now
   delete qPix;
   
   return tlw.value();
}

void DynamicBrush::initGL()
{
   _glContext->makeCurrent();
   glClearColor (0.0, 0.0, 0.0, 0.0);
   glShadeModel (GL_SMOOTH);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST);
   _glContext->doneCurrent();
//    glNewList( _background, GL_COMPILE );
// 
//    glEndList();
}

QPixmap *DynamicBrush::glPixmap(const QRect &rect, const QSize &size, int darkness)
{
   // so we'll use false colors here, avoiding image swap orgies
# if __BYTE_ORDER == __BIG_ENDIAN // OpenGL talks RGBA; Qt wants ARGB, do sth. about read type, need ppc arch for testing
#define _COLORS_ &a,&r,&g,&b
   double r,g,b,a;
#else // OpenGL talks ABGR; Qt wants ARGB
#define _COLORS_ &b,&g,&r
   double r,g,b;
#endif
   if (darkness)
      QApplication::palette().color(QPalette::Window).dark(100+darkness).getRgbF(_COLORS_);
   else
      QApplication::palette().color(QPalette::Window).getRgbF(_COLORS_);
   _glContext->makeCurrent();
   int width = 128, height = 128; // texture_2d must be 2^n, we start with 2^7 to save some time here ;)
   while (width < size.width()) width = (width<<1);
   while (height < size.height()) height = (height<<1);

//    if (!QGLFramebufferObject::hasOpenGLFramebufferObjects())
//    {
//       qWarning("ARRGHHH... system doesn't support framebuffers!?!");
//       return QPixmap();
//    }
   
   QGLFramebufferObject *buffer = new QGLFramebufferObject(width, height );
   if (!buffer->isValid()) {
      qWarning("ARRGHHH... failed to create a texture buffer!");
      return new QPixmap();
   }
   if (!buffer->bind()) {
      qWarning("ARRGHHH... failed to bind gl rendering to texture buffer!");
      return new QPixmap();
   }
   
   glViewport(0, 0, (GLsizei) size.width(), (GLsizei) size.height());
   glMatrixMode (GL_PROJECTION);
   glLoadIdentity ();
//    gluOrtho2D (0.0, 30.0, 30.0, 0.0); //lrbt
   GLfloat fw = 30.0/size.width(), fh = 30.0/size.height();
   // the statement is vertically mirrored, as is the pbuffer to a qimage
   // we simply avoid a software mirror and paint "mirrored" in gl (no one will see ;)
   glOrtho( rect.x()*fw, rect.right()*fw, rect.y()*fh, rect.bottom()*fh, -1, 1 );
   glMatrixMode(GL_MODELVIEW);
      glBegin (GL_QUADS);
      glColor4f (r*0.8, g*0.8, b*0.8, 1.0);
      glVertex2f (0.0, 30.0);
      glVertex2f (30.0, 30.0);
      glColor4f (r, g, b, 1.0);
      glVertex2f (30.0, 0.0);
      glVertex2f (0.0, 0.0);
   glEnd();
   glBegin (GL_TRIANGLES);
      glColor4f (r, g, b, 0.6);
      glVertex2f (30.0, 30.0);
      glColor4f (r, g, b, 0.0);
      glVertex2f (15.0, 30.0);
      glVertex2f (30.0, 5.0);
   glEnd();
   glBegin (GL_TRIANGLE_FAN);
      r *= 1.1; g *= 1.1; b *=1.1;
      glColor4f (r, g, b, .8);
      glVertex2f (15.0, 00.0);
      glColor4f (r, g, b, 0.0);
      glVertex2f (30.0, 0.0);
      glVertex2f (26.0, 3.0);
      glVertex2f (22.0, 3.5);
      glVertex2f (17.0, 5.0);
      glVertex2f (13.0, 5.0);
      glVertex2f (8.0, 3.5);
      glVertex2f (4.0, 3.0);
      glVertex2f (0.0, 0.0);
   glEnd();
//    glCallList(_background);
   glFlush();
   
   /* ================================================================
      Hi complex...

      the qimage -> qpixmap conversion is slow,
      so we try to avoid it by using knowledge of the special
      structure of the image

      #1. the upper 5/30 mirror horizontal, mirroring a pixmap is faster than
      double conversion (at least for big images, small are not so important)

      #2. the lower left tiles horizontally, so we convert only 32xheight pixles
      and tile them together as pixmaps

      #3. only the lower right is unique - so we need to convert it completely

      the thing gets worse as we need to be able to extract parts for
      eg. the groupbox shadows... i hate my life ;P
   
      IF ANYONE EVER TOUCHES THIS CODE AND THERE IS "glExtPixmapFromTexture" OR SIMILAR:
      THIS WOULD BE BY FAR BETTER!!!
   ================================================================ */

   QPixmap *pix = new QPixmap(rect.size());
   int w_2 = (size.width()+1)/2;
   int yOff = 5*size.height()/30;
   int rlw = w_2 - rect.x();
   int rYOff = yOff-rect.y();
   int w,h;
   QPixmap tmp;
   QPainter p(pix);
   
   // top --------------------
   if (rect.y() < yOff)
   {
      w = MAX(w_2-rect.x(), rect.right()-w_2+1);
      h = yOff-rect.y();
      QImage *top = new QImage(w, h, QImage::Format_RGB32);
      glReadPixels(w_2-w, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, top->bits());
      tmp = QPixmap::fromImage(*top, Qt::ColorOnly|Qt::OrderedDither);
      p.drawPixmap(rect.x(),0, tmp, rect.x(),0,w_2-rect.x(),-1);
      tmp = mirror(tmp, Qt::Horizontal);
      p.drawPixmap(w_2,0, tmp,0,0,rect.right()-w_2+1,-1);
      delete top;
   }
   // bottom left --------------------
   if (rlw > 0)
   {
      h = rect.bottom() - yOff + 1;
      QImage *btmLeft = new QImage(32,h, QImage::Format_RGB32);
      glReadPixels(0, yOff, 32, h, GL_RGBA, GL_UNSIGNED_BYTE, btmLeft->bits());
      tmp = QPixmap::fromImage(*btmLeft, Qt::ColorOnly|Qt::OrderedDither);
      p.drawTiledPixmap(0,rYOff,rlw,h, tmp);
      delete btmLeft;
   }
   // bottom right --------------------
   w_2 = (size.width()-1)/2;
   w = rect.right() - w_2;
   if (w > 0)
   {
      h = rect.bottom() - yOff + 1;
      QImage *btmRight = new QImage(w,h, QImage::Format_RGB32);
      glReadPixels(w_2, yOff, w, h, GL_RGBA, GL_UNSIGNED_BYTE, btmRight->bits());
      tmp = QPixmap::fromImage(*btmRight, Qt::ColorOnly|Qt::OrderedDither);
      delete btmRight;
      p.drawPixmap(rlw,rYOff, tmp);
   }
   p.end();
   delete buffer;
   return pix;
}

const BgSet* DynamicBrush::bgSetGL(const QSize &size, bool updateDeco)
{
      
   // first check the cache
   bool found;
   BgPixCache::iterator tlw = checkCache(found);
   if (found)
      return tlw.value();

   QPixmap *qPix = glPixmap(QRect(QPoint(0,0), size), size);
   
   // cache
   if (tlw == tlwbacks.end())
      tlw = tlwbacks.insert(_topLevelWidget, new BgSet(qPix, decoDim[2],
         decoDim[0], _size.width(), _size.height()));
   else
      tlw.value()->setPixmap(qPix, decoDim[2], decoDim[0], _size.width(),
                            _size.height(), updateDeco);
   
   // pixmap can be deleted now
   delete qPix;
   
   return tlw.value();

}

const BgSet* DynamicBrush::bgSetRender(const QSize &size, bool updateDeco) {
   
   // first check the cache
   bool found;
   BgPixCache::iterator tlw = checkCache(found);
   if (found)
      return tlw.value();
   
   // we have no sufficient pixmap, create one and set it
   QColor c = QApplication::palette().color(QPalette::Window);
   QPixmap *qPix = new QPixmap(size);
   int w = size.width(), h = size.height();
   ColorArray ca = ColorArray() << c << c.dark(110);
   OXPicture lgp = OXRender::gradient(QPoint(0,0), QPoint(0, h), ca);
   OXRender::composite (lgp, None, *qPix, 0, 0, 0, 0, 0, 0, w, h);
   OXRender::freePicture(lgp);
   c = c.light(160);
   c.setAlpha(180); ca.replace(0, c);
   c.setAlpha(0); ca.replace(1, c);
   float f[2] = { pow(0.5, (float)h/w), pow(0.5, (float)w/h) };
   lgp = OXRender::gradient(QPoint(w, h), QPoint(w*f[0], h*f[1]), ca);
   OXRender::composite (lgp, None, *qPix, 0, 0, 0, 0, 0, 0, w, h, PictOpOver);
   OXRender::freePicture(lgp);
   h = qPix->height()/6;
   int r = -(pow(w, 2)/4 + pow(h,2)) / (-2*h);
   lgp = OXRender::gradient(QPoint(w/2, 0), 0, QPoint(w/2,h-r), r, ca);
   OXRender::composite (lgp, None, *qPix, 0, 0, 0, 0, 0, 0,
                        w, qPix->height(), PictOpOver);
   OXRender::freePicture(lgp);
   
   // cache
   if (tlw == tlwbacks.end())
      tlw = tlwbacks.insert(_topLevelWidget, new BgSet(qPix, decoDim[2],
         decoDim[0], _size.width(), _size.height()));
   else
      tlw.value()->setPixmap(qPix, decoDim[2], decoDim[0], _size.width(),
                            _size.height(), updateDeco);
   
   // pixmap can be deleted now
   delete qPix;
   
   return tlw.value();
}

const BgSet* DynamicBrush::bgSetQt(const QSize &size, bool updateDeco)
{

   // first check the cache
   bool found;
   BgPixCache::iterator tlw = checkCache(found);
   if (found)
      return tlw.value();
   
   QColor c = QApplication::palette().color(QPalette::Window);
   QPixmap *qPix = new QPixmap(_size);
   QLinearGradient lg1(QPoint(0,0), QPoint(0, size.height()));
   lg1.setColorAt(0, c);
   lg1.setColorAt(1, c.dark(110));
   QLinearGradient lg2(QPoint(size.width()/2, size.height()/2),
                       QPoint(size.width(), size.height()));
   QColor ct = c; ct.setAlpha(0);
   lg2.setColorAt(0, ct);
   ct.setAlpha(140);
   lg2.setColorAt(1, ct);
   QPainter p(qPix);
   p.setPen(Qt::NoPen);
   p.setBrush(lg1);
   p.drawRect( 0, 0, qPix->width(), qPix->height());
   QPoint points[3] = {
      QPoint(size.width(), 0),
      QPoint(size.width(), size.height()),
      QPoint(0, size.height())
   };
   p.setBrush(lg2);
   p.drawPolygon(points, 3);
   p.end();

   // cache
   if (tlw == tlwbacks.end())
      tlw = tlwbacks.insert(_topLevelWidget, new BgSet(qPix, decoDim[2],
         decoDim[0], _size.width(), _size.height()));
   else
      tlw.value()->setPixmap(qPix, decoDim[2], decoDim[0], _size.width(),
                            _size.height(), updateDeco);
   
   // pixmap can be deleted now
   delete qPix;
   
   return tlw.value();
}

// QPixmap DynamicBrush::shadow(const QRect &rect)
// {
//    switch (_mode)
//    {
//    case Tiled2:
//    {
//       QPixmap qPix(rect.size());
//       int width = _size.width();
//       int height = rect.height();
//       float factor = MIN(1.0, ((float)width)/(2*_tile[0][0].width()+2*_center[0][0].width()));
//       int wt = (int) (factor * _tile[0][0].width());
//       int wc = (int) (factor * _center[0][0].width());
//       int x = width/2 - wc;
//       QPainter p(&qPix);
//       //left
//       p.drawTiledPixmap(-rect.x(), 0, x, height, _tile[1][0], x % wt);
//       //left center
//       x -= rect.x();
//       p.drawTiledPixmap(x, 0, wc, height, _center[1][0]);
//       // right center
//       x += wc;
//       p.drawTiledPixmap(x, 0, wc, height, _center[1][1], _center[1][0].width()-wc);
//       // right
//       x += wc;
//       p.drawTiledPixmap(x, 0, rect.right()-x, height, _tile[1][1]);
//       p.end();
//       return qPix;
//    }
//    case Tiled:
//    {
//       QPixmap qPix(rect.size());
//       Drawable x11d = qPix.handle();
//       Display *dpy = QX11Info::display();
//       GC gc = XCreateGC( dpy, x11d, 0, 0 );
//       
//       int ttbh = 20;
//       int W_TopOff = _bgYoffset - ttbh;
//       int W_xBottom = QApplication::desktop()->width() - _size.width();
//       int W_xTop = W_xBottom/3;
//       int W_w_2 = (_size.width()>>1);
//       // topleft geometry : W_xTop, ttbh, W_w_2, W_TopOff
//       // topright geometry : (W_xTop<<1) + W_w_2, ttbh, windowSize.width() - W_w_2, W_TopOff
//       // bottom geometry: W_xBottom, yoff, windowSize.width(), windowSize.height() - yoff
//       
//       // top left ----------------
//       if (rect.x() < W_w_2 && rect.y() < W_TopOff)
//       {
//          XCopyArea( dpy, _shadow, x11d, gc,
//                      W_xTop + rect.x(), ttbh + rect.y(),
//                      //widget->width() - (rect.x() + rect.width() - W_w_2),
//                      W_w_2 - rect.x(),
//                      //widget->height() - (rect.y() + rect.height() - W_TopOff),
//                      W_TopOff - rect.y(),
//                      0, 0 );
//       }
//       
//       // top right ----------------
//       if (rect.right() > W_w_2 && rect.y() < W_TopOff)
//       {
//          W_xTop = (W_xTop<<1) + W_w_2;
//          int x, xOff, w;
//          if (rect.x() < W_w_2)
//          {
//             w = rect.right() - W_w_2;
//             x = W_w_2 - rect.x();
//             xOff = W_xTop;
//          }
//          else
//          {
//             w = rect.width();
//             x = 0;
//             xOff = W_xTop + (rect.x() - W_w_2);
//          }
//          XCopyArea( dpy, _shadow, x11d, gc,
//                      xOff, ttbh + rect.y(), w, W_TopOff - rect.y(),
//                      x, 0 );
//       }
//       
//       // bottom center part ----------------
//       if (rect.bottom() > W_TopOff)
//       {
//          int h, y, yOff;
//          if (rect.y() < W_TopOff)
//          {
//             h = rect.y() + rect.height() - W_TopOff;
//             y = W_TopOff - rect.y();
//             yOff = _bgYoffset;
//          }
//          else
//          {
//             h = rect.height();
//             y = 0;
//             yOff = rect.y() + ttbh;
//          }
//          XCopyArea( dpy, _shadow, x11d, gc,
//                      W_xBottom + rect.x(), yOff, rect.width(), h,
//                      0, y );
//       }
//       XFreeGC ( dpy , gc );
//       return qPix;
//    }
//    case OpenGL:
//       if (_glShadow.isNull() || _lastShadowRect != rect )
//       {
//          _lastShadowRect = rect;
//          _glShadow = glPixmap(rect, 10);
//       }
//       return _glShadow;
//    case QtGradient:
//    case XRender:
//       if (_glShadow.isNull() || _lastShadowRect != rect )
//       {
//          _glShadow = QPixmap(rect.size());
//          _lastShadowRect = rect;
//          QColor c = QApplication::palette().color(QPalette::Window).light(105);
//          ColorArray ca = ColorArray() << c << c.dark(110);
//          QPoint center(_glShadow.width()/2, _glShadow.height());
//          int r = sqrt(pow(_glShadow.width()/2,2)+pow(_glShadow.height(),2));
//          OXPicture rg = OXRender::gradient(center, 0, center, r, ca);
//          XRenderComposite (QX11Info::display(), PictOpSrc, rg, None, _glShadow.x11PictureHandle(),
//                            0, 0, 0, 0, 0, 0, _glShadow.width(), _glShadow.height());
//          XRenderFreePicture(QX11Info::display(), rg);
//       }
//       return _glShadow;
//    }
//    return QPixmap();
// }

#if 0 // this is some stored code - yet not used, but maybe later on

static bool eraseWidget(const QWidget *widget, const QPaintDevice *device, bool pseudoTransparent, const QRegion &clipRegion)
{
   const QWidget *bgWidget = (pseudoTransparent && widget->parentWidget()) ? widget->parentWidget() : widget;
   if (bgWidget->backgroundRole() == Qt::NoBackground || !widget->palette().brush( QPalette::Active, bgWidget->backgroundRole() ).texture().isNull())
      return false;
   
   OXPixmap _bgPx;
   if (bgWidget->palette().color(QPalette::Active, bgWidget->backgroundRole()) == qApp->palette().color( QPalette::Active, QPalette::Background))
      _bgPx = bgPix;
   else if (bgWidget->palette().color(QPalette::Active, bgWidget->backgroundRole()) == groupShadowColor_)
      _bgPx = _shadow;
   else
      return false;

   if (!_bgPx)
      return false;
   
   QPoint windowOffset = widget->backgroundOffset();
   int ttbh = 20;
   QSize windowSize = widget->topLevelWidget()->size();
   int W_TopOff = _bgYoffset - ttbh;
   int W_xBottom = qApp->desktop()->width() - windowSize.width();
   int W_xTop = W_xBottom/3;
   int W_w_2 = (windowSize.width()>>1);
   // topleft geometry : W_xTop, ttbh, W_w_2, W_TopOff
   // topright geometry : (W_xTop<<1) + W_w_2, ttbh, windowSize.width() - W_w_2, W_TopOff
   // bottom geometry: W_xBottom, yoff, windowSize.width(), windowSize.height() - yoff
   
   // painting
   GC gc = XCreateGC( QX11Info::display(), device->handle(), 0, 0 );
   
   if (!clipRegion.isNull())
   {
      int num;
      XRectangle *rects = (XRectangle *)qt_getClipRects( clipRegion, num );
      XSetClipRectangles( QX11Info::display(), gc, 0, 0, rects, num, YXBanded );
   }
   
   // top left
   if (windowOffset.x() < W_w_2 && windowOffset.y() < W_TopOff)
   {
      
      XCopyArea( QX11Info::display(), _bgPx, device->handle(), gc,
                 W_xTop + windowOffset.x(),
                 ttbh + windowOffset.y(),
      //widget->width() - (windowOffset.x() + widget->width() - W_w_2),
                 W_w_2 - windowOffset.x(),
      //widget->height() - (windowOffset.y() + widget->height() - W_TopOff),
                 W_TopOff - windowOffset.y(),
                 0, 0 );
   }
   
   // top right
   if (windowOffset.x() + widget->width() > W_w_2
       && windowOffset.y() < W_TopOff)
   {
      W_xTop = (W_xTop<<1) + W_w_2;
      int x, xOff, w;
      if (windowOffset.x() < W_w_2)
      {
         w = windowOffset.x() + widget->width() - W_w_2;
         x = W_w_2 - windowOffset.x();
         xOff = W_xTop;
      }
      else
      {
         w = widget->width();
         x = 0;
         xOff = W_xTop + (windowOffset.x() - W_w_2);
      }
      XCopyArea( QX11Info::display(), _bgPx, device->handle(), gc,
                 xOff,
                 ttbh + windowOffset.y(),
                 w,
                 W_TopOff - windowOffset.y(),
                 x, 0 );
   }
   
   // bottom center part
   if (windowOffset.y() + widget->height() > W_TopOff)
   {
      int h, y, yOff;
      if (windowOffset.y() < W_TopOff)
      {
         h = windowOffset.y() + widget->height() - W_TopOff;
         y = W_TopOff - windowOffset.y();
         yOff = _bgYoffset;
      }
      else
      {
         h = widget->height();
         y = 0;
         yOff = windowOffset.y() + ttbh;
      }
      XCopyArea( QX11Info::display(), _bgPx, device->handle(), gc,
                 W_xBottom + windowOffset.x(),
                 yOff,
                 widget->width(), h,
                 0, y );
   }
   
   XFreeGC ( QX11Info::display() , gc );
   return true;
}
#endif

void DynamicBrush::setXPixmap(OXPixmap pixmap)
{
   if (pixmap != -1)
      _pixmap = pixmap;
}
// cause of cmake
#include "dynamicbrush.moc"

