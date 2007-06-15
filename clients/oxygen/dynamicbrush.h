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

#ifndef DYNAMICBRUSH_H
#define DYNAMICBRUSH_H

class QTimer;
class QImage;
class QRect;
class QGLWidget;

#include <QMap>
#include <QObject>
#include <QSize>
#include <QRect>
#include <QPixmap>
#include <X11/Xlib.h>
#include <fixx11h.h>

class BgSet {
public:
   BgSet();
   BgSet(const QPixmap* pix, int dx, int dy, int w, int h,
         bool updateDeco = true);
   BgSet(const QPixmap* pix, int d, int s, Qt::Orientation o,
         bool updateDeco = true);
   void setPixmap(const QPixmap* pix, int dx, int dy, int w, int h,
                  bool updateDeco = false);
   void setPixmap(const QPixmap* pix, int d, int s, Qt::Orientation o,
                  bool updateDeco = false);
   void wipe();
   ~BgSet();
   QPixmap *window,
      *decoTop, *decoBottom, *decoLeft, *decoRight;
};

typedef QMap<QWidget*, BgSet*> BgPixCache;

class DynamicBrush : public QObject
{
   Q_OBJECT
public:
   enum Mode {Tiled = 0, QtGradient, XRender, OpenGL,
         VGradient1, HGradient1, VGradient2, HGradient2, Glass};
   DynamicBrush(Mode mode, QObject *parent = 0);
   DynamicBrush(Pixmap pixmap = -1, int bgYoffset = 0, QObject *parent = 0);
   ~DynamicBrush();
//    QPixmap shadow(const QRect &rect);
   void setMode(Mode);
   void setXPixmap(Pixmap pixmap = -1);
protected:
   virtual bool eventFilter ( QObject * watched, QEvent * event );
private:
   // bg pixmap creating
   const BgSet* bgSetTiled(const QSize &size, bool updateDeco);
   const BgSet* bgSetGL(const QSize &size, bool updateDeco);
   const BgSet* bgSetRender(const QSize &size, bool updateDeco);
   const BgSet* bgSetGradient1(const QSize &size, bool updateDeco);
   const BgSet* bgSetGradient2(const QSize &size, bool updateDeco);
   const BgSet* bgSetQt(const QSize &size, bool updateDeco);
   const BgSet* bgSetGlass(const QSize &size, bool updateDeco);
   
   // get the deco dimensions from the deco
   void readDecoDim(QWidget *topLevelWidget);
   
   // tiling
   Pixmap _pixmap;
   int _bgYoffset;
   void generateTiles(Mode mode);
   
   // complex (gl/qt/render)
   BgPixCache::iterator checkCache(bool &found);
   
   // openGL
   void initGL();
   QGLWidget *_glContext;
   QPixmap *glPixmap(const QRect &rect, const QSize &size, int darkness = 0);
   
   // gradients
   QPixmap _tile[2][2];
   QColor _bgC[2];
   
   // states
   QSize _size;
   Mode _mode;
   bool _isActiveWindow;
   int decoDim[4];
   
   // bender
   QWidget *_topLevelWidget;
   
   // wiping
   QTimer *_timerBgWipe;
private slots:
   void wipeBackground();
};

#endif //DYNAMICBRUSH_H
