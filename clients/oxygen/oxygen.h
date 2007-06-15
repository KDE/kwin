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

#ifndef OXYGEN_STYLE_H
#define OXYGEN_STYLE_H

class QAbstractButton;
class QHeaderView;
class QMenuBar;
class QPushButton;
class QScrollBar;
class QTabBar;
class DynamicBrush;
class QPaintEvent;
class QFrame;
// class GradientCache;

#include <QCache>
#include <QHash>
#include <QMap>
#include <QCommonStyle>
#include <QBitmap>
#include <QRegion>
#include <QWidget>
#include <X11/Xlib.h>
#include <fixx11h.h>
#include "tileset.h"

namespace Oxygen {

enum BGMode { Plain = 0, Scanlines, Dummy, FullPix, VGradient1, HGradient1, VGradient2, HGradient2, Glass };
enum Acceleration { None = 0, QtGradient, XRender, OpenGL };
enum TabTransition {Jump = 0, CrossFade, ScanlineBlend, SlideIn, SlideOut, RollIn, RollOut, OpenVertically, CloseVertically, OpenHorizontally, CloseHorizontally };

class EventKiller : public QObject 
{
   Q_OBJECT
public:
   bool eventFilter( QObject *object, QEvent *event);
};

class OxygenStyle;

class HoverFadeInfo {
public:
   HoverFadeInfo(int s = 0, bool fI = true) {step = s; fadeIn = fI; }
   int step;
   bool fadeIn;
};

class ComplexHoverFadeInfo {
public:
   ComplexHoverFadeInfo() {
      activeSubControls = fadingInControls = fadingOutControls = QStyle::SC_None;
   }
   QStyle::SubControls activeSubControls, fadingInControls, fadingOutControls;
   QHash<QStyle::SubControl, int> steps;
};

class IndexedFadeInfo {
public:
   IndexedFadeInfo(long int index) { this->index = index; }
   long int index;
   QHash<long int, int> fadingInIndices, fadingOutIndices;
   int step(long int index);
};

class TabAnimInfo : public QObject {
   Q_OBJECT
public:
   TabAnimInfo(QObject *parent = 0, int currentTab = -1) :
      QObject(parent), lastTab(currentTab), animStep(0){}
protected:
   bool eventFilter( QObject* object, QEvent* event );
public:
   QList < QWidget* > autofillingWidgets;
   int lastTab, animStep;
   QPixmap tabPix[3];
};

enum Orientation3D {Sunken = 0, Relief, Raised};
enum GradientType {
   GradSimple = 0,
   GradSunken,
   GradGloss,
   GradGlass,
   GradRadialGloss,
   GradButton,
   NumGrads
};

typedef struct {
   int $1, $2, $3, $4, $5, $6, $7, $8, $9, $10;
   int $12, $13, $16, $32, $18, $20, $80;
   int ScrollBarExtent;
   int ScrollBarSliderMin;
   int SliderThickness;
   int SliderControl;
   int Indicator;
   int ExclusiveIndicator;
} Dpi;

typedef struct Config {
   GradientType gradient, gradBtn, gradientStrong;
   BGMode bgMode;
   Acceleration acceleration;
   int structure;
   TabTransition tabTransition;
   int gradientIntensity;
   bool aqua, showMenuIcons, glassProgress, glassMenus, menuShadow;
   double scale;
   int checkType;
   QPalette::ColorRole role_progress[2], role_tab[2],
      role_btn[2], role_btnHover[2], role_popup[2];
} Config;

class VisualFrame : public QWidget {
   Q_OBJECT
public:
   VisualFrame(QFrame *parent, int top = 0, int left = 0, int right = 0, int bottom = 0);
   bool eventFilter ( QObject * o, QEvent * ev );
   void paintEvent ( QPaintEvent * event );
protected:
//    void dragEnterEvent ( QDragEnterEvent * event ) { passDownEvent(event, event->globalPos()); }
//    void dragLeaveEvent ( QDragLeaveEvent * event ) { passDownEvent(event, event->globalPos()); }
//    void dragMoveEvent ( QDragMoveEvent * event ) { passDownEvent(event, event->globalPos()); }
//    void dropEvent ( QDropEvent * event ) { passDownEvent(event, event->globalPos()); }
//    void enterEvent ( QEvent * event ) { passDownEvent(event, event->globalPos()); }
//    void leaveEvent ( QEvent * event ) { passDownEvent(event, event->globalPos()); }
   void mouseDoubleClickEvent ( QMouseEvent * event );
   void mouseMoveEvent ( QMouseEvent * event );
   void mousePressEvent ( QMouseEvent * event );
   void mouseReleaseEvent ( QMouseEvent * event );
   void wheelEvent ( QWheelEvent * event );
private:
   void passDownEvent(QEvent *ev, const QPoint &gMousePos);
   int off[4];
};

class OxygenStyle : public QCommonStyle {
   Q_OBJECT
public:
   enum WidgetState{Basic = 0, Hovered, Focused, Active};
   
   OxygenStyle();
   ~OxygenStyle();

   //inheritance from QStyle
   void drawComplexControl ( ComplexControl control,
                             const QStyleOptionComplex * option,
                             QPainter * painter,
                             const QWidget * widget = 0 ) const;
  
   void drawControl ( ControlElement element,
                      const QStyleOption * option,
                      QPainter * painter,
                      const QWidget * widget = 0 ) const;
   
   /**what do they do?
   virtual void drawItemPixmap ( QPainter * painter, const QRect & rect, int alignment, const QPixmap & pixmap ) const;
   virtual void drawItemText ( QPainter * painter, const QRect & rect, int alignment, const QPalette & pal, bool enabled, const QString & text, QPalette::ColorRole textRole = QPalette::NoRole ) const;
   */
   
   void drawPrimitive ( PrimitiveElement elem,
                                const QStyleOption * option,
                                QPainter * painter,
                                const QWidget * widget = 0 ) const;
   
   QPixmap standardPixmap ( StandardPixmap standardPixmap,
                                    const QStyleOption * option = 0,
                                    const QWidget * widget = 0 ) const;
   
//    what do they do? ========================================
//    QPixmap generatedIconPixmap ( QIcon::Mode iconMode,
//                                  const QPixmap & pixmap,
//                                  const QStyleOption * option ) const;
//    SubControl hitTestComplexControl ( ComplexControl control,
//                                       const QStyleOptionComplex * option,
//                                       const QPoint & pos,
//                                       const QWidget * widget = 0 ) const;
//    QRect itemPixmapRect ( const QRect & rect,
//                           int alignment,
//                           const QPixmap & pixmap ) const;
//    QRect itemTextRect ( const QFontMetrics & metrics,
//                         const QRect & rect,
//                         int alignment,
//                         bool enabled,
//                         const QString & text ) const;
//=============================================================
   
   int pixelMetric ( PixelMetric metric,
                             const QStyleOption * option = 0,
                             const QWidget * widget = 0 ) const;
   
   void polish( QWidget *w );
   void polish( QApplication * );
   void polish( QPalette &pal );
   
   QSize sizeFromContents ( ContentsType type,
                            const QStyleOption * option,
                            const QSize & contentsSize,
                            const QWidget * widget = 0 ) const;
      
   int styleHint ( StyleHint hint,
                   const QStyleOption * option = 0,
                   const QWidget * widget = 0,
                   QStyleHintReturn * returnData = 0 ) const;
   
   QRect subControlRect ( ComplexControl control,
                          const QStyleOptionComplex * option,
                          SubControl subControl,
                          const QWidget * widget = 0 ) const;
   
   QRect subElementRect ( SubElement element,
                                  const QStyleOption * option,
                                  const QWidget * widget = 0 ) const;
   
   QPalette standardPalette () const;
   
   void unPolish( QWidget *w );
   void unPolish( QApplication *a );
   
   // from QObject
   bool eventFilter( QObject *object, QEvent *event );
   
signals:
   void MDIPopup(QPoint);
   
private slots:
   void fakeMouse();
   void handleIPC(int, int);
   
   // animation slots ============================
   void progressbarDestroyed(QObject*);
   void updateProgressbars();
   
   void tabChanged(int index);
   void updateTabAnimation();
   void tabDestroyed(QObject* obj);
   
   void updateFades();
   void fadeDestroyed(QObject* obj);
   
   void updateComplexFades();
   void complexFadeDestroyed(QObject* obj);
   
   void updateIndexedFades();
   void indexedFadeDestroyed(QObject* obj);
   
   //=========================================

private:
   OxygenStyle( const OxygenStyle & );
   OxygenStyle& operator=( const OxygenStyle & );
   const QPixmap &gradient(const QColor &c,
                           int size,
                           Qt::Orientation o,
                           GradientType type = GradSimple) const;
   const QPixmap &btnAmbient(int height) const;
   const QPixmap &tabShadow(int height, bool bottom = false) const;
   const QPixmap &groupLight(int height) const;
   
   void fillWithMask(QPainter *painter,
                     const QRect &rect,
                     const QBrush &brush,
                     const Tile::Mask *mask,
                     Tile::PosFlags pf = Tile::Full,
                     bool justClip = false,
                     QPoint offset = QPoint(),
                     bool inverse = false,
                     const QRect *outerRect = 0L) const;
   void fillWithMask(QPainter *painter,
                     const QPoint &xy,
                     const QBrush &brush,
                     const QPixmap &mask,
                     QPoint offset = QPoint()) const;
   
   QColor mapFadeColor(const QColor &color, int index) const;
   void fadeIn(QWidget *widget);
   void fadeOut(QWidget *widget );
   QPixmap *tint(const QImage &img, const QColor& c) const;
   const Tile::Set &glow(const QColor & c, bool round = false) const;
   void readSettings();
   void generatePixmaps();
   void initMetrics();
   void makeStructure(int num, const QColor &c);
   int hoverStep(const QWidget *widget) const;
   const ComplexHoverFadeInfo *complexHoverFadeInfo(const QWidget *widget,
      SubControls activeSubControls) const;
   const IndexedFadeInfo *indexedFadeInfo(const QWidget *widget, long int index) const;
   int progressStep(const QWidget *w) const;
   
private:
   typedef QHash<QWidget*, HoverFadeInfo> HoverFades;
   typedef QCache<uint, QPixmap> PixmapCache;
   typedef QHash<uint, Tile::Set> TileCache;
   struct {
      Tile::Mask /*rect[3], round[3], */button, tab/*, group*/;
      QPixmap radio, radioIndicator, radioGroove, notch, slider[4];
      QPixmap winClose, winMin, winMax;
      QRegion popupCorner[4];
   } masks;
   struct {
      Tile::Set button[2][8], tab, sunken, group, lineEdit[2], raised, relief;
      QPixmap radio[2][2];
      QPixmap winClose[2], winMin[2], winMax[2];
      Tile::Line line[2][3];
      QPixmap slider[4][2][2];
   } shadows;
   struct {
      Tile::Set rect[3], round[3], button[2];
   } frames;
   struct {
      Tile::Line top;
      QPixmap slider[4];
      Tile::Mask button;
   } lights;
   
   // pixmaps
   QPixmap *_scanlines[2];
   // cache
//    GradientCache *gradients[2][NumGrads];
   PixmapCache gradients[2][NumGrads];
   PixmapCache _btnAmbient, _tabShadow, _groupLight;
   TileCache glowCache;
   TileCache roundGlowCache;

   
   //anmiated progressbars
   bool animationUpdate;
   int complexStep;
   
//    QPalette tooltipPalette;
   
   // toolbar title functionality ========================
   QPoint cursorPos_;
   bool mouseButtonPressed_;
   bool internalEvent_;
   // ===========================
   DynamicBrush *_bgBrush;
   Pixmap popupPix;
   QTimer* timer;
};

} // namespace Oxygen
#endif //OXYGEN_STYLE_H
