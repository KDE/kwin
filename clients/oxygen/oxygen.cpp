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

/**================== Qt4 includes ======================*/
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QEvent>
#include <QList>
#include <QTimer>
#include <QResizeEvent>
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QGroupBox>
#include <QPixmap>
#include <QImage>
#include <QDesktopWidget>
#include <QX11Info>
#include <QStylePlugin>
#include <QProgressBar>
#include <QMenu>
#include <QMenuBar>
#include <QStyleOptionProgressBarV2>
#include <QLayout>
#include <QListWidget>
#include <QAbstractButton>
#include <QPushButton>
#include <QScrollBar>
#include <QTabBar>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSplitterHandle>
#include <QToolBar>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractScrollArea>
/**============= Qt3 support includes ======================*/
#include <Q3ScrollView>
/**========================================================*/

/**============= System includes ==========================*/
// #include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrender.h>
// #include <fixx11h.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>
/**========================================================*/

/**============= DEBUG includes ==========================*/
#undef DEBUG
#ifdef DEBUG
#define MOUSEDEBUG 1
#include <QtDebug>
#include "debug.h"
#define oDEBUG qDebug()
#include <QTime>
#define _PROFILESTART_ QTime timer; int time; timer.start();
#define _PROFILERESTART_ timer.restart();
#define _PROFILESTOP_(_STRING_) time = timer.elapsed(); qDebug("%s: %d",_STRING_,time);
#else
#define oDEBUG //
#undef MOUSEDEBUG
#endif
/**========================================================*/

/**============= Oxygen includes ==========================*/
#include "oxygen.h"
#include "dynamicbrush.h"
#include "makros.h"
/**=========================================================*/


/**============= extern C stuff ==========================*/
class OxygenStylePlugin : public QStylePlugin
{
public:
   QStringList keys() const {
      return QStringList() << "Oxygen";
   }
   
   QStyle *create(const QString &key) {
      if (key == "oxygen")
         return new Oxygen::OxygenStyle;
      return 0;
   }
};

Q_EXPORT_PLUGIN2(OxygenStyle, OxygenStylePlugin)
/**=========================================================*/
using namespace Oxygen;

/** static config object */
Config config;
Dpi dpi;

/** The event killer*/
bool EventKiller::eventFilter( QObject *, QEvent *)
{
   return true;
}
static EventKiller *eventKiller = new EventKiller();

/** Let's try if we can supply round frames that shape their content */

VisualFrame::VisualFrame(QFrame *parent, int st, int sl, int sr, int sb) : QWidget(parent) {
   if (!(parent && (st+sl+sr+sb))) {
      deleteLater(); return;
   }
   connect(parent, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
   off[0] = st; off[1] = sl; off[2] = sr; off[3] = sb;
   parent->installEventFilter(this);
   this->installEventFilter(this);
   show();
}

void VisualFrame::paintEvent ( QPaintEvent * event ) {
   if (!parent()) {
      deleteLater(); return;
   }
   QFrame *frame = static_cast<QFrame*>(parent());
   QPainter p(this);
   p.setClipRegion(event->region(), Qt::IntersectClip);
   QStyleOption opt;
   if (frame->frameShadow() == QFrame::Raised)
      opt.state |= QStyle::State_Raised;
   else if (frame->frameShadow() == QFrame::Sunken)
      opt.state |= QStyle::State_Sunken;
   if (frame->hasFocus())
      opt.state |= QStyle::State_HasFocus;
   if (frame->isEnabled())
      opt.state |= QStyle::State_Enabled;
   opt.rect = rect();
   style()->drawPrimitive(QStyle::PE_Frame, &opt, &p, this);
   p.end();
}

void VisualFrame::passDownEvent(QEvent *ev, const QPoint &gMousePos) {
   if (!parent()) {
      deleteLater(); return;
   }
   // the raised frames don't look like you could click in, we'll see if this should be chnged...
   QFrame *frame = static_cast<QFrame*>(parent());
   if (frame->frameShadow() == QFrame::Raised)
      return;
   QList<QWidget *> candidates = frame->findChildren<QWidget *>();
   QList<QWidget *>::const_iterator i = candidates.constEnd();
   QWidget *match = 0;
   while (i != candidates.constBegin()) {
      --i;
      if (*i == this)
         continue;
      if ((*i)->rect().contains((*i)->mapFromGlobal(gMousePos)))
      {
         match = *i;
         break;
      }
   }
   if (!match) match = frame;
   QCoreApplication::sendEvent( match, ev );
}

void VisualFrame::mouseDoubleClickEvent ( QMouseEvent * event ) { passDownEvent((QEvent *)event, event->globalPos()); }
void VisualFrame::mouseMoveEvent ( QMouseEvent * event ) { passDownEvent((QEvent *)event, event->globalPos()); }
void VisualFrame::mousePressEvent ( QMouseEvent * event ) { passDownEvent((QEvent *)event, event->globalPos()); }
void VisualFrame::mouseReleaseEvent ( QMouseEvent * event ) { passDownEvent((QEvent *)event, event->globalPos()); }
void VisualFrame::wheelEvent ( QWheelEvent * event ) { passDownEvent((QEvent *)event, event->globalPos()); }

bool VisualFrame::eventFilter ( QObject * o, QEvent * ev ) {
   if (o == this) {
      if (ev->type() == QEvent::ZOrderChange)
         this->raise();
      return false;
   }
   if (o != parent()) {
      o->removeEventFilter(this);
      return false;
   }
   if (ev->type() == QEvent::Resize) {
      QRect rect = static_cast<QFrame*>(o)->frameRect();
      move(rect.topLeft()); resize(rect.size());
      setMask(QRegion(rect).subtracted(rect.adjusted(off[0],off[1],-off[2],-off[3])));
      return false;
   }
   if (ev->type() == QEvent::FocusIn ||
       ev->type() == QEvent::FocusOut) {
      update();
      return false;
   }
   return false;
}

/** to be able to send nullpix refs */
static QPixmap nullPix;

/** Get some excluded code */
#include "inlinehelp.cpp"
#include "animation.cpp"
#include "gradients.cpp"

/** Internal, handles the shared bg pixmaps*/
static Pixmap bgPix = 0;
static Pixmap fgPix = 0;
Pixmap shadowPix;
int bgYoffset_;

static void loadPixmaps()
{
   QFile file("/tmp/oxygenPixIDs");
   char ignore;
   if (file.open( QIODevice::ReadOnly ))
   {
      QTextStream ts( &file );
      ts >> bgPix >> ignore >> shadowPix >> ignore >> fgPix >> ignore >> bgYoffset_;
      file.close();
//       QSettings settings;
//       settings.beginGroup("/oxygen/Style");
//       config.bgMode = (BGTileMode) settings.value("BackgroundMode", FullPix).toInt();
//       settings.endGroup();
   }
   else
   {
      fgPix = shadowPix = bgPix = 0;
   }
}

/**For painting the shared xpixmap on a qmenu*/
inline void *qt_getClipRects( const QRegion &r, int &num )
{
   return r.clipRectangles( num );
}
extern Drawable qt_x11Handle(const QPaintDevice *pd);
/**=====================================================================*/

/**Some static variables*/

// static const int windowsItemFrame	= 1; // menu item frame width
// static const int windowsSepHeight	= 2; // separator item height
// static const int windowsItemHMargin	= 3; // menu item hor text margin
// static const int windowsItemVMargin	= 1; // menu item ver text margin
// static const int windowsArrowHMargin	= 6; // arrow horizontal margin
// static const int windowsTabSpacing	= 12; // space between text and tab
// static const int windowsCheckMarkHMargin= 2; // horiz. margins of check mark
// static const int windowsRightBorder	= 12; // right border on windows
// static const int windowsCheckMarkWidth	= 14; // checkmarks width on windows

static QColor originalBgColor_;
static QColor groupShadowColor_;

static bool invColorRole(QPalette::ColorRole &from, QPalette::ColorRole &to,
                         QPalette::ColorRole defFrom = QPalette::WindowText, QPalette::ColorRole defTo = QPalette::Window)
{
   switch (from)
   {
   case QPalette::WindowText: //0
      to = QPalette::Window; break;
   case QPalette::Window: //10
      to = QPalette::WindowText; break;
   case QPalette::Base: //9
      to = QPalette::Text; break;
   case QPalette::Text: //6
      to = QPalette::Base; break;
   case QPalette::Button: //1
      to = QPalette::ButtonText; break;
   case QPalette::ButtonText: //8
      to = QPalette::Button; break;
   case QPalette::Highlight: //12
      to = QPalette::HighlightedText; break;
   case QPalette::HighlightedText: //13
      to = QPalette::Highlight; break;
   default:
      from = defFrom;
      to = defTo;
      return false;
   }
   return true;
}

void OxygenStyle::makeStructure(int num, const QColor &c)
{
   if (!_scanlines[num])
      _scanlines[num] = new QPixmap(64, 64);
   QPainter p(_scanlines[num]);
   switch (config.structure)
   {
   default:
   case 0: { // scanlines
      _scanlines[num]->fill( c.light(110).rgb() );
      p.setPen( (num == 1) ? c.light(106) : c.light(103) );
      int i;
      for ( i = 1; i < 64; i += 4 )
      {
         p.drawLine( 0, i, 63, i );
         p.drawLine( 0, i+2, 63, i+2 );
      }
      p.setPen( c );
      for ( i = 2; i < 63; i += 4 )
         p.drawLine( 0, i, 63, i );
      break;
   }
   case 1: { //checkboard
      p.setPen(Qt::NoPen);
      p.setBrush(c.light(102));
      if (num == 1) {
         p.drawRect(0,0,16,16); p.drawRect(32,0,16,16);
         p.drawRect(16,16,16,16); p.drawRect(48,16,16,16);
         p.drawRect(0,32,16,16); p.drawRect(32,32,16,16);
         p.drawRect(16,48,16,16); p.drawRect(48,48,16,16);
      }
      else {
         p.drawRect(0,0,32,32);
         p.drawRect(32,32,32,32);
      }
      p.setBrush(c.dark(102));
      if (num == 1) {
         p.drawRect(16,0,16,16); p.drawRect(48,0,16,16);
         p.drawRect(0,16,16,16); p.drawRect(32,16,16,16);
         p.drawRect(16,32,16,16); p.drawRect(48,32,16,16);
         p.drawRect(0,48,16,16); p.drawRect(32,48,16,16);
      }
      else {
         p.drawRect(32,0,32,32);
         p.drawRect(0,32,32,32);
      }
      break;
   }
   case 2: // bricks - sucks...
      p.setPen(c.dark(105));
      p.setBrush(c.light(102));
      p.drawRect(0,0,32,16); p.drawRect(32,0,32,16);
      p.drawRect(0,16,16,16); p.drawRect(16,16,32,16); p.drawRect(48,16,16,16);
      p.drawRect(0,32,32,16); p.drawRect(32,32,32,16);
      p.drawRect(0,48,16,16); p.drawRect(16,48,32,16); p.drawRect(48,48,16,16);
      break;
   }
   p.end();
}

void OxygenStyle::readSettings()
{
   QSettings settings("Oxygen", "Style");
   settings.beginGroup("Style");
   
   config.bgMode = (BGMode) settings.value("BackgroundMode", FullPix).toInt();
   config.acceleration = (Acceleration) settings.value("Acceleration", XRender).toInt();
   if (config.acceleration == None) config.bgMode = Plain;
   config.structure = settings.value("Structure", 0).toInt();
   
   config.scale = settings.value("Scale", 1.0).toDouble();
   config.checkType = settings.value("CheckType", 1).toInt();
   
   
   config.gradientIntensity = settings.value("GradientIntensity",70).toInt();
   
   config.tabTransition = (TabTransition) settings.value("TabTransition", ScanlineBlend).toInt();
   
   config.gradient = GradGlass;
   config.gradientStrong = GradGloss;
   config.aqua = settings.value("Aqua", true).toBool();
   if (!config.aqua) {
      config.gradient = GradButton;
      config.gradientStrong = GradSimple;
   }
   if (settings.value("InverseButtons", false).toBool())
      config.gradBtn = GradSunken;
   else
      config.gradBtn = config.gradient;
   
   config.showMenuIcons = settings.value("ShowMenuIcons", false).toBool();
   config.glassMenus = !settings.value("RuphysMenu", false).toBool();
   config.menuShadow = settings.value("MenuShadow", false).toBool();
   
   config.glassProgress = settings.value("GlassProgress", true).toBool();
   
   // color roles
   config.role_progress[0] =
      (QPalette::ColorRole) settings.value("role_progressGroove", QPalette::WindowText).toInt();
   invColorRole(config.role_progress[0], config.role_progress[1],
                QPalette::WindowText, QPalette::Window);
   config.role_progress[1] =
      (QPalette::ColorRole) settings.value("role_progress", config.role_progress[1]).toInt();
   config.role_tab[0] =
      (QPalette::ColorRole) settings.value("role_tab", QPalette::Button).toInt();
   invColorRole(config.role_tab[0], config.role_tab[1],
                QPalette::Button, QPalette::ButtonText);
   config.role_btn[0] =
      (QPalette::ColorRole) settings.value("role_button", QPalette::Window).toInt();
   invColorRole(config.role_btn[0], config.role_btn[1],
                QPalette::Button, QPalette::WindowText);
   config.role_btnHover[0] =
      (QPalette::ColorRole) settings.value("role_buttonHover", QPalette::Button).toInt();
   invColorRole(config.role_btnHover[0], config.role_btnHover[1],
                QPalette::Button, QPalette::ButtonText);
   config.role_popup[0] =
      (QPalette::ColorRole) settings.value("role_popup", QPalette::Window).toInt();
   invColorRole(config.role_popup[0], config.role_popup[1],
                QPalette::Window, QPalette::WindowText);

   settings.endGroup();
}

#define SCALE(_N_) lround(_N_*config.scale)

#include "genpixmaps.cpp"

void OxygenStyle::initMetrics()
{
   dpi.$1 = SCALE(1); dpi.$2 = SCALE(2);
   dpi.$3 = SCALE(3); dpi.$4 = SCALE(4);
   dpi.$5 = SCALE(5); dpi.$6 = SCALE(6);
   dpi.$7 = SCALE(7); dpi.$8 = SCALE(8);
   dpi.$9 = SCALE(9); dpi.$10 =SCALE(10);
   
   dpi.$12 = SCALE(12); dpi.$13 = SCALE(13);
   dpi.$16 = SCALE(16); dpi.$18 = SCALE(18);
   dpi.$20 = SCALE(20); dpi.$32 = SCALE(32);
   dpi.$80 = SCALE(80);
   
   dpi.ScrollBarExtent = SCALE(20);
   dpi.ScrollBarSliderMin = SCALE(40);
   dpi.SliderThickness = SCALE(24);
   dpi.SliderControl = SCALE(17);
   dpi.Indicator = SCALE(24);
   dpi.ExclusiveIndicator = SCALE(19);
}

#undef SCALE

/**THE STYLE ITSELF*/
OxygenStyle::OxygenStyle() : QCommonStyle(), animationUpdate(false),
mouseButtonPressed_(false), internalEvent_(false), _bgBrush(0L), popupPix(0L),
timer(0L) {
   _scanlines[0] = _scanlines[1] = 0L;
   readSettings();
   initMetrics();
   generatePixmaps();
   // init the caches
   for (int i = 0; i < 2; ++i)
      for (int j = 0; j < NumGrads; ++j)
         gradients[i][j].setMaxCost( 1024<<10 );
         
   //====== TOOLTIP ======================
//    tooltipPalette = qApp->palette();
//    tooltipPalette.setBrush( QPalette::Background, QColor( 255, 255, 220 ) );
//    tooltipPalette.setBrush( QPalette::Foreground, Qt::black );
   //=======================================
   
   
//    if (KApplication::kApplication())
//       connect(KApplication::kApplication(), SIGNAL(kipcMessage(int, int)), this, SLOT (handleIPC(int, int)));
   
#if 0
   // TEST!!!
   if (KApplication::kApplication())
   {
      _PROFILESTART_
      QByteArray data, replyData;
      QCString replyType;
      QDataStream arg(data, IO_WriteOnly);
      arg << "";
      if (!KApplication::kApplication()->dcopClient()->call("kwin", "OxygenInterface", "pixmap()", data, replyType, replyData))
         qDebug("there was some error using DCOP.");
      else
      {
         QDataStream reply(replyData, IO_ReadOnly);
         if (replyType == "QPixmap")
         {
            QPixmap result;
            reply >> result;
         }
         else
            qDebug("pixmap() returned an unexpected type of reply!");
      }
      _PROFILESTOP_("dcop pixmap")
   }
   //===========
#endif

   // start being animated
   timer = new QTimer( this );
//    timer->start(50);
   connect(timer, SIGNAL(timeout()), this, SLOT(updateProgressbars()));
   connect(timer, SIGNAL(timeout()), this, SLOT(updateTabAnimation()));
   connect(timer, SIGNAL(timeout()), this, SLOT(updateFades()));
   connect(timer, SIGNAL(timeout()), this, SLOT(updateComplexFades()));
   connect(timer, SIGNAL(timeout()), this, SLOT(updateIndexedFades()));
}

OxygenStyle::~OxygenStyle() {
   for (int i = 0; i < 2; i++)
      for (int j = 0; j < NumGrads; j++)
            gradients[i][j].clear();
   glowCache.clear();
   _btnAmbient.clear();
   _tabShadow.clear();
   roundGlowCache.clear();
   if (timer) {
      timer->disconnect();
      timer->stop();
//       delete timer;
   }
   progressbars.clear();

//    bfi.clear();
//    fadeColorMap.clear();

   bgPix = 0L;
   shadowPix = 0L;
   fgPix = 0L;
}

/**handles updates to the bg pixmap*/
void OxygenStyle::handleIPC(int /*id*/, int /*data*/)
{
#if 0
   if (id != 32 || data != 151616) //TODO: use custom ID as soon as kapp supports random values on this
      return;
   loadPixmaps();
   if (qApp->desktop())
      bgYoffset_ = qApp->desktop()->height()*bgYoffset_/738;
   popupPix = config.inversePopups ? fgPix : bgPix;
   // tell the deco that this it a oxygen styled qt window
   if (!bgPix)
   {
      foreach (QWidget *w, QApplication::topLevelWidgets())
      {
         if (!(qobject_cast<QMenuBar*>(w) ||
           qobject_cast<QMenu*>(w) ||
            (w->windowType() == Qt::Desktop) ||
           w->inherits("QTipLabel") ||
           qobject_cast<QAbstractScrollArea*>(w)))
           w->setPalette(QPalette()); //TODO: make sure this is a bgPix'd widget!
      }
   }
   else
   {
      foreach (QWidget *w, QApplication::topLevelWidgets())
      {
         if (!(qobject_cast<QMenuBar*>(w) ||
               qobject_cast<QMenu*>(w) ||
               (w->windowType() == Qt::Desktop) ||
               w->inherits("QTipLabel") ||
               qobject_cast<QAbstractScrollArea*>(w)))
         {
            // faking resize to make sure the window's bg is updated
            QResizeEvent* rev = new QResizeEvent(w->size(), QSize(0,0));
            backgroundHandler_->eventFilter( w, rev );
         }
      }
   }
#endif
}

/** for creating the highlight glow pixmps*/
QPixmap *OxygenStyle::tint(const QImage &img, const QColor& oc) const
{
   QImage *dest = new QImage( img.width(), img.height(), QImage::Format_ARGB32/*_Premultiplied*/ );
//    dest->fill(0);
   unsigned int *data = ( unsigned int * ) img.bits();
   unsigned int *destData = ( unsigned int* ) dest->bits();
   int total = img.width() * img.height();
   int red, green, blue;
   int current;
   oc.getHsv(&red,&green,&blue); // not really ;) green is saturation
   current = green/60;
   green = CLAMP(green*(1+current)/current,0,255);
   QColor c; c.setHsv(red,green,blue);
   
   int srcR = c.red() - 128;
   int srcG = c.green() - 128;
   int srcB = c.blue() - 128;
   int destR, destG, destB;

   // grain/merge from the gimp. TODO: use mmx/sse here
   for ( current = 0 ; current < total ; ++current )
   {
      red = srcR + qRed( data[ current ] );
      green = srcG + qGreen( data[ current ] );
      blue = srcB + qBlue( data[ current ] );
      destR = CLAMP(red, 0, 255);
      destG = CLAMP(green, 0, 255);
      destB = CLAMP(blue, 0, 255);
      destData[ current ] = qRgba( destR, destG, destB, qAlpha(data[ current ]) );
   }
   QPixmap *pix = new QPixmap(QPixmap::fromImage(*dest, 0));
   delete dest;
   return pix;
}

const Tile::Set &OxygenStyle::glow(const QColor & c, bool round) const
{
   TileCache *cache = const_cast<TileCache*>(round ? &roundGlowCache : &glowCache);
   TileCache::const_iterator it = cache->find(c.rgb());
   if (it != cache->end())
      return it.value();
   
   // need to create a new one
   QPixmap *pix;
   Tile::Set frame;
   
   if (round)
   {
      pix = tint(QImage(":/round-glow"), c);
      frame = Tile::Set(*pix,10,10,7,6);
   }
   else
   {
      pix = tint(QImage(":/glow"), c);
      frame = Tile::Set(*pix,15,12,6,4);
   }

   delete pix;
   if (cache->size() == cache->capacity())
      cache->clear();
   it = cache->insert(c.rgb(), frame);
   return it.value();
}
/**=========================*/

void OxygenStyle::fillWithMask(QPainter *painter, const QPoint &xy, const QBrush &brush, const QPixmap &mask, QPoint offset) const
{
   QPixmap qPix(mask.size());
   if (brush.texture().isNull())
      qPix.fill(brush.color());
   else {
      QPainter p(&qPix);
      p.drawTiledPixmap(mask.rect(),brush.texture(),offset);
      p.end();
   }
   qPix = OXRender::applyAlpha(qPix, mask);
   painter->drawPixmap(xy, qPix);
}

void OxygenStyle::fillWithMask(QPainter *painter, const QRect &rect, const QBrush &brush, const Tile::Mask *mask, Tile::PosFlags pf, bool justClip, QPoint offset, bool inverse, const QRect *outerRect) const
{
   // TODO: get rid of this?! - masks now render themselves!
   bool pixmode = !brush.texture().isNull();
   if (!mask) {
      if (pixmode)
         painter->drawTiledPixmap(rect, brush.texture(), offset);
      else
         painter->fillRect(rect, brush.color());
      return;
   }
   mask->render(rect, painter, brush, pf, justClip, offset, inverse, outerRect);
}

/**======================================*/

/**QStyle reimplementation ========================================= */

void OxygenStyle::polish ( QApplication * app ) {
//    if (timer && !timer->isActive())
//       timer->start(50);
   loadPixmaps();
   if (app->desktop())
      bgYoffset_ = app->desktop()->height()*bgYoffset_/738;
   popupPix = bgPix;
   QPalette pal = app->palette();
   polish(pal);
   app->setPalette(pal);
   if (!_bgBrush && config.bgMode > Dummy) {
      if (config.bgMode > FullPix)
         _bgBrush = new DynamicBrush((DynamicBrush::Mode)config.bgMode, this);
      else {
      if (config.acceleration > None)
         _bgBrush = new DynamicBrush((DynamicBrush::Mode)config.acceleration, this);
      else
         _bgBrush = new DynamicBrush(bgPix, bgYoffset_, this);
      }
   }
   app->installEventFilter(this);
}

#define _SHIFTCOLOR_(clr) clr = QColor(CLAMP(clr.red()-10,0,255),CLAMP(clr.green()-10,0,255),CLAMP(clr.blue()-10,0,255))

void OxygenStyle::polish( QPalette &pal )
{
   if (config.bgMode == Scanlines) {
      QColor c = pal.color(QPalette::Active, QPalette::Background);
      makeStructure(0, c);
      QBrush brush( c, *_scanlines[0] );
      pal.setBrush( QPalette::Background, brush );
   }
   else if (config.bgMode != Plain) {
      int h,s,v;
      QColor c = pal.color(QPalette::Active, QPalette::Background);
      c.getHsv(&h,&s,&v);
      originalBgColor_ = c;
      if (v < 70) // very dark colors won't make nice backgrounds ;)
         c.setHsv(h,s,70);
      pal.setColor( QPalette::Window, c );
   }
   
   int highlightGray = qGray(pal.color(QPalette::Active, QPalette::Highlight).rgb());
   pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(highlightGray,highlightGray,highlightGray));
   
   //inactive palette
   pal.setColor(QPalette::Inactive, QPalette::WindowText, midColor(pal.color(QPalette::Active, QPalette::Window),pal.color(QPalette::Active, QPalette::WindowText)));
   pal.setColor(QPalette::Inactive, QPalette::Text, midColor(pal.color(QPalette::Active, QPalette::Base),pal.color(QPalette::Active, QPalette::Text),1,3));
   pal.setColor(QPalette::Inactive, QPalette::Highlight, midColor(pal.color(QPalette::Active, QPalette::Highlight),pal.color(QPalette::Active, QPalette::HighlightedText),3,1));
   pal.setColor(QPalette::Inactive, QPalette::AlternateBase, midColor(pal.color(QPalette::Active, QPalette::AlternateBase),pal.color(QPalette::Active, QPalette::Base),3,1));
   
//    loadPixmaps();
//    if (qApp->desktop())
//       bgYoffset_ = qApp->desktop()->height()*bgYoffset_/738;
   
   popupPix = bgPix;
}

#include <QtDebug>

static Atom winTypePopup = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
static Atom winType = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE", False);

void OxygenStyle::polish( QWidget * widget) {
   
   // installs dynamic brush to all widgets, taking care of a correct bg pixmap size
   //TODO maybe we can exclude some more widgets here... (currently only popup menus)
   
   if (_bgBrush && !(
         qobject_cast<QMenu*>(widget) ||
         widget->inherits("QAlphaWidget") ||
         widget->inherits("QComboBoxListView") ||
         widget->inherits("QComboBoxPrivateContainer") ||
         // Kwin stuff ===========================
         widget->topLevelWidget()->objectName() == "decoration widget" ||
         widget->topLevelWidget()->inherits("QDesktopWidget") ||
         widget->topLevelWidget()->objectName() == "desktop_widget"
        ))
      widget->installEventFilter(_bgBrush);

#ifdef MOUSEDEBUG
   widget->installEventFilter(this);
#endif
   
   if (false
//         qobject_cast<QPushButton *>(widget)
// #ifndef QT_NO_COMBOBOX
//        || qobject_cast<QComboBox *>(widget)
// #endif
#ifndef QT_NO_SPINBOX
       || qobject_cast<QAbstractSpinBox *>(widget)
#endif
//        || qobject_cast<QCheckBox *>(widget)
       || qobject_cast<QScrollBar *>(widget)
       || widget->inherits("QHeaderView")
//        || qobject_cast<QRadioButton *>(widget)
#ifndef QT_NO_SPLITTER
       || qobject_cast<QSplitterHandle *>(widget)
#endif
#ifndef QT_NO_TABBAR
       || qobject_cast<QTabBar *>(widget)
#endif
       || widget->inherits("QWorkspaceTitleBar")
       || widget->inherits("QToolButton")
       || widget->inherits("QDockWidget")
       || widget->inherits("QToolBar")
       || widget->inherits("QToolBarHandle")
       || widget->inherits("QDockSeparator")
       || widget->inherits("QToolBoxButton")
       || widget->inherits("QAbstractSlider")
       || widget->inherits("QDockWidgetSeparator")
       || widget->inherits("Q3DockWindowResizeHandle")
      )
         widget->setAttribute(Qt::WA_Hover);
   
   if (qobject_cast<QAbstractButton*>(widget)) {
      if (widget->inherits("QToolBoxButton"))
         widget->setForegroundRole ( QPalette::WindowText );
      else {
         widget->setBackgroundRole ( config.role_btn[0] );
         widget->setForegroundRole ( config.role_btn[1] );
         widget->installEventFilter(this);
      }
   }
   if (qobject_cast<QComboBox *>(widget)) {
      widget->setBackgroundRole ( QPalette::Base );
      widget->setForegroundRole ( QPalette::Text );
      widget->installEventFilter(this);
   }
   if (qobject_cast<QAbstractSlider *>(widget)) {
      widget->installEventFilter(this);
      if (qobject_cast<QScrollBar *>(widget) &&
         !(widget->parentWidget() &&
            widget->parentWidget()->parentWidget() &&
            widget->parentWidget()->parentWidget()->inherits("QComboBoxListView")))
         widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
   }
   
   if (qobject_cast<QProgressBar*>(widget)) {
      widget->setBackgroundRole ( config.role_progress[0] );
      widget->setForegroundRole ( config.role_progress[1] );
      widget->installEventFilter(this);
      if (!timer->isActive()) timer->start(50);
      connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(progressbarDestroyed(QObject*)));
   }
   
   if (qobject_cast<QTabWidget*>(widget)) {
      connect((QTabWidget*)widget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
      connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(tabDestroyed(QObject*)));
   }
   if (qobject_cast<QTabBar *>(widget)) {
      widget->setBackgroundRole ( config.role_tab[0] );
      widget->setForegroundRole ( config.role_tab[1] );
      widget->installEventFilter(this);
   }
   
   
   if (qobject_cast<QAbstractScrollArea*>(widget) || qobject_cast<Q3ScrollView*>(widget) ||
       widget->inherits("QWorkspaceTitleBar"))
      widget->installEventFilter(this);
   
   if (widget->inherits("QWorkspace"))
      connect(this, SIGNAL(MDIPopup(QPoint)), widget, SLOT(_q_popupOperationMenu(QPoint)));

   
   if (false // to simplify the #ifdefs
#ifndef QT_NO_MENUBAR
       || qobject_cast<QMenuBar *>(widget)
#endif
#ifdef QT3_SUPPORT
       || widget->inherits("Q3ToolBar")
#endif
#ifndef QT_NO_TOOLBAR
       || qobject_cast<QToolBar *>(widget)
       || (widget && qobject_cast<QToolBar *>(widget->parent()))
#endif
      ) {
      widget->setBackgroundRole(QPalette::Window);
      if (config.bgMode == Scanlines) {
         widget->setAutoFillBackground ( true );
         QPalette pal = widget->palette();
         QColor c = pal.color(QPalette::Active, QPalette::Window);
         
         if (!_scanlines[1])
            makeStructure(1, c);
         QBrush brush( c, *_scanlines[1] );
         pal.setBrush( QPalette::Window, brush );
         widget->setPalette(pal);
      }
   }
   
   if (!widget->isWindow())
   if (QFrame *frame = qobject_cast<QFrame *>(widget)) {
      // kill ugly winblows frames...
      if (frame->frameShape() == QFrame::Box ||
          frame->frameShape() == QFrame::Panel ||
          frame->frameShape() == QFrame::WinPanel)
         frame->setFrameShape(QFrame::StyledPanel);
      
      // overwrite ugly lines
      if (frame->frameShape() == QFrame::HLine ||
          frame->frameShape() == QFrame::VLine)
         widget->installEventFilter(this);
      
      // toolbox handling - a shame they look that crap by default!
      else if (widget->inherits("QToolBox")) {
         widget->setBackgroundRole(QPalette::Window);
         widget->setForegroundRole(QPalette::WindowText);
         if (widget->layout()) {
            widget->layout()->setMargin ( 0 );
            widget->layout()->setSpacing ( 0 );
         }
      }

      else if (widget->inherits("QListView")) {
        // don't do anything as it breaks drag and drop 
      }
//        && !(
//          widget->inherits("QComboBoxListView") ||
//          widget->inherits("QComboBoxPrivateContainer"))
      else if (frame->frameShape() == QFrame::StyledPanel) {
         if (widget->inherits("QTextEdit") && frame->lineWidth() == 1)
            frame->setLineWidth(dpi.$4);
         else {
            QList<VisualFrame*> vfs = frame->findChildren<VisualFrame*>();
            bool addVF = true;
            foreach (VisualFrame* vf, vfs)
               if (vf->parent() == frame) addVF = false;
            if (addVF)
               new VisualFrame(frame, dpi.$4, dpi.$4, dpi.$4, frame->frameShadow() == QFrame::Sunken ? dpi.$2 : dpi.$4);
         }
      }
   }
   
   if (widget->autoFillBackground() &&
       // dad
       widget->parentWidget() &&
       ( widget->parentWidget()->objectName() == "qt_scrollarea_viewport" ) &&
       //grampa
       widget->parentWidget()->parentWidget() &&
       qobject_cast<QAbstractScrollArea*>(widget->parentWidget()->parentWidget()) &&
       // grangrampa
       widget->parentWidget()->parentWidget()->parentWidget() &&
       widget->parentWidget()->parentWidget()->parentWidget()->inherits("QToolBox")
      ) {
      widget->parentWidget()->setAutoFillBackground(false);
      widget->setAutoFillBackground(false);
   }
   
   // swap qmenu colors
   if (qobject_cast<QMenu *>(widget)) {
      // this should tell beryl et. al this is a popup - doesn't work... yet
      XChangeProperty(QX11Info::display(), widget->winId(), winType,
                      XA_CARDINAL, 32, PropModeReplace, (const unsigned char*)&winTypePopup, 1L);
      // WARNING: compmgrs like e.g. beryl deny to shadow shaped windows,
      // if we cannot find a way to get ARGB menus independent from the app settings, the compmgr must handle the round corners here
      widget->installEventFilter(this); // for the round corners
      widget->setAutoFillBackground (true);
      widget->setBackgroundRole ( config.role_popup[0] );
      widget->setForegroundRole ( config.role_popup[1] );
      if (qGray(widget->palette().color(QPalette::Active, widget->backgroundRole()).rgb()) < 100) {
         QFont tmpFont = widget->font();
         tmpFont.setBold(true);
         widget->setFont(tmpFont);
      }
   }
   
   //========================

}

bool OxygenStyle::eventFilter( QObject *object, QEvent *ev ) {
   switch (ev->type()) {
   case QEvent::Paint: {
      if (QFrame *frame = qobject_cast<QFrame*>(object)) {
         if (frame->frameShape() == QFrame::HLine ||
             frame->frameShape() == QFrame::VLine) {
            QPainter p(frame);
            Orientation3D o3D = (frame->frameShadow() == QFrame::Sunken) ? Sunken:
               (frame->frameShadow() == QFrame::Raised) ? Raised : Relief;
            shadows.line[frame->frameShape() == QFrame::VLine][o3D].render(frame->rect(), &p);
            p.end();
            return true;
         }
      }
      else if (QTabBar *tabBar = qobject_cast<QTabBar*>(object)) {
         if (tabBar->parentWidget() &&
             qobject_cast<QTabWidget*>(tabBar->parentWidget()))
            return false; // no extra tabbar here please...
         QPainter p(tabBar);
         QStyleOptionTabBarBase opt;
         opt.initFrom(tabBar);
         drawPrimitive ( PE_FrameTabBarBase, &opt, &p);
         p.end();
      }
      return false;
   }
   case QEvent::Resize: {
      if (QMenu *menu = qobject_cast<QMenu*>(object)) {
/*         QPalette::ColorRole role = menu->backgroundRole();
         QColor c = menu->palette().color(QPalette::Active, role);
         int size = ((QResizeEvent*)ev)->size().height();
         QBrush brush;
         
         if (config.glassMenus) {
            const QPixmap &glass = gradient(c, size, Qt::Vertical, config.gradient);
            brush = QBrush(c, glass);
         }
         else {
            int v = colorValue(c);
            if (v < 80) {
               int h,s; c.getHsv(&h,&s,&v); v = 80; c.setHsv(h,s,v);
            }
            QPixmap pix(32,size);
            QLinearGradient lg(QPoint(0, 0), QPoint(0, size));
            QColor dark = c.dark(100+3000/v);
            lg.setColorAt(0, c); lg.setColorAt(0.2, dark);
            lg.setColorAt(0.8, c);
            QPainter p(&pix); p.fillRect(pix.rect(), lg); p.end();
            brush = QBrush(c, pix);
         }
         
         QPalette pal = menu->palette();
         pal.setBrush(role, brush);
         menu->setPalette(pal);
*/         return false;
      }
//          QResizeEvent *rev = (QResizeEvent*)ev;
//          int w = ((QResizeEvent*)ev)->size().width(),
//             h = ((QResizeEvent*)ev)->size().height();
//          QRegion mask(0,0,w,h);
//          mask -= masks.popupCorner[0]; // tl
//          QRect br = masks.popupCorner[1].boundingRect();
//          mask -= masks.popupCorner[1].translated(w-br.width(), 0); // tr
//          br = masks.popupCorner[2].boundingRect();
//          mask -= masks.popupCorner[2].translated(0, h-br.height()); // bl
//          br = masks.popupCorner[3].boundingRect();
//          mask -= masks.popupCorner[3].translated(w-br.width(), h-br.height()); // br
//          menu->setMask(mask);
//       }
//    }
      return false;
   }
   case QEvent::MouseButtonPress: {
      QMouseEvent *mev = (QMouseEvent*)ev;
#ifdef MOUSEDEBUG
      qDebug() << object;
#endif
      if (( mev->button() == Qt::LeftButton) &&
          object->inherits("QWorkspaceTitleBar")) {
         //TODO this is a hack to get the popupmenu to the right side. bug TT to query the position with a SH
         QWidget *widget = (QWidget*)object;
         // check for menu button
         QWidget *MDI = qobject_cast<QWidget*>(widget->parent()); if (!MDI) return false; //this is elsewhat...
         /// this does not work as TT keeps the flag in a private to the titlebar (for no reason?)
//             if (!(widget->windowFlags() & Qt::WindowSystemMenuHint)) return false;
         // check if we clicked it..
         if (mev->x() < widget->width()-widget->height()-2) return false;
         // find popup
         MDI = qobject_cast<QWidget*>(MDI->parent()); if (!MDI) return false; //this is elsewhat...
         MDI = MDI->findChild<QMenu *>("qt_internal_mdi_popup");
         if (!MDI) {
            qWarning("MDI popup not found, unable to calc menu position");
            return false;
         }
         // calc menu position
         emit MDIPopup(widget->mapToGlobal( QPoint(widget->width() - MDI->sizeHint().width(), widget->height())));
         return true;
      }
      return false;
   }
   case QEvent::Show: {
      if (qobject_cast<QProgressBar*>(object) &&
          ((QWidget*)object)->isEnabled()) {
         progressbars[(QWidget*)object] = 0;
         return false;
      }
      if (qobject_cast<QTabWidget*>(object)) {
         tabwidgets[(QTabWidget*)object] =
            new TabAnimInfo((QTabWidget*)object, ((QTabWidget*)object)->currentIndex());
         return false;
      }
      return false;
   }
      
#define HANDLE_SCROLL_AREA_EVENT \
         if (area->horizontalScrollBar()->isVisible())\
            fadeIn(area->horizontalScrollBar());\
         if (area->verticalScrollBar()->isVisible())\
            fadeIn(area->verticalScrollBar());
   case QEvent::Enter:
      if (qobject_cast<QAbstractButton*>(object) ||
          qobject_cast<QComboBox*>(object)) {
         QWidget *widget = (QWidget*)object;
         if (!widget->isEnabled())
            return false;
         if (widget->hasFocus()) {
            widget->update(); return false;
         }
         fadeIn(widget);
         return false;
      }
      else if (QAbstractScrollArea* area =
          qobject_cast<QAbstractScrollArea*>(object)) {
         if (!area->isEnabled()) return false;
         HANDLE_SCROLL_AREA_EVENT
         return false;
      }
      else if (Q3ScrollView* area =
               qobject_cast<Q3ScrollView*>(object)) {
         if (!area->isEnabled()) return false;
         HANDLE_SCROLL_AREA_EVENT
         return false;
      }
      return false;

#undef HANDLE_SCROLL_AREA_EVENT
#define HANDLE_SCROLL_AREA_EVENT \
         if (area->horizontalScrollBar()->isVisible())\
            fadeOut(area->horizontalScrollBar());\
         if (area->verticalScrollBar()->isVisible())\
            fadeOut(area->verticalScrollBar());
   case QEvent::Leave:
      if (qobject_cast<QAbstractButton*>(object) || 
          qobject_cast<QComboBox*>(object)) {
         QWidget *widget = (QWidget*)object;
         if (!widget->isEnabled())
            return false;
         if (widget->hasFocus()) {
            widget->update(); return false;
         }
         fadeOut(widget);
         return false;
      }
      else if (QAbstractScrollArea* area =
          qobject_cast<QAbstractScrollArea*>(object)) {
         if (!area->isEnabled()) return false;
         HANDLE_SCROLL_AREA_EVENT
         return false;
      }
      else if (Q3ScrollView* area =
               qobject_cast<Q3ScrollView*>(object)) {
         HANDLE_SCROLL_AREA_EVENT
         return false;
      }
      return false;
#undef HANDLE_SCROLL_AREA_EVENT
      
   case QEvent::FocusIn:
      if (qobject_cast<QAbstractButton*>(object) ||
          qobject_cast<QComboBox*>(object)) {
         QWidget *widget = (QWidget*)object;
         if (!widget->isEnabled()) return false;
         if (widget->testAttribute(Qt::WA_UnderMouse))
            widget->repaint();
         else
            fadeIn(widget);
         return false;
      }
      return false;
   case QEvent::FocusOut:
      if (qobject_cast<QAbstractButton*>(object) || 
          qobject_cast<QComboBox*>(object)) {
         QWidget *widget = (QWidget*)object;
         if (!widget->isEnabled()) return false;
         if (widget->testAttribute(Qt::WA_UnderMouse))
            widget->repaint();
         else
            fadeOut((QWidget*)(object));
         return false;
      }
      return false;
   case QEvent::EnabledChange:
      if (qobject_cast<QProgressBar*>(object)) {
         if (((QWidget*)object)->isEnabled())
            progressbars[(QWidget*)object] = 0;
         else
            progressbars.remove((QWidget*)object);
         return false;
      }
      return false;
   default:
      return false;
   }
}

void OxygenStyle::unPolish( QApplication */*app */)
{
   if (timer)
      timer->stop();
//    inExitPolish = true;
//    QPalette pal( app->palette() );
//    // rQWidgeteset bg Tileset
//    for (int i = QPalette::Disabled; i < QPalette::NColorGroups; i++)
//    {
//       if ( !pal.brush( (QPalette::ColorGroup)i, QPalette::Background ).texture().isNull() )
//          pal.setBrush( (QPalette::ColorGroup)i, QPalette::Background, originalBgColor_ );
//       if ( !pal.brush( (QPalette::ColorGroup)i, QPalette::Button ).texture().isNull() )
//          pal.setBrush( (QPalette::ColorGroup)i, QPalette::Button, originalBgColor_ );
//    }
//    app->setPalette( pal );
//    inExitPolish = false;
}

void OxygenStyle::unPolish( QWidget *widget )
{
   if (qobject_cast<QProgressBar*>(widget)) {
      widget->removeEventFilter(this);
      disconnect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(progressbarDestroyed(QObject*)));
   }
   if (qobject_cast<QAbstractScrollArea*>(widget) || qobject_cast<Q3ScrollView*>(widget))
      widget->removeEventFilter(this);
   if (_bgBrush)
      widget->removeEventFilter(this);
   if (qobject_cast<VisualFrame*>(widget))
      widget->deleteLater();
//    w->removeEventFilter(this);
//    if (w->isTopLevel() || qobject_cast<QGroupBox*>(w) || w->inherits("KActiveLabel"))
//       w->setPalette(QPalette());
}

QPalette OxygenStyle::standardPalette () const
{
   QPalette pal ( Qt::black, QColor(30,31,32), // windowText, button
                     Qt::white, QColor(200,201,202), QColor(221,222,223), //light, dark, mid
                     Qt::black, Qt::white, //text, bright_text
                     QColor(251,254,255), QColor(234,236,238) ); //base, window
   pal.setColor(QPalette::ButtonText, QColor(234,236,238));
   return pal;
}

/** eventcontrol slots*/

void OxygenStyle::fakeMouse()
{
   if (mouseButtonPressed_) // delayed mousepress for move event
   {
      QCursor::setPos ( cursorPos_ );
      XTestFakeButtonEvent(QX11Info::display(),1, false, 0);
      XTestFakeKeyEvent(QX11Info::display(),XKeysymToKeycode(QX11Info::display(), XK_Alt_L), true, 0);
      XTestFakeButtonEvent(QX11Info::display(),1, true, 0);
      XTestFakeKeyEvent(QX11Info::display(),XKeysymToKeycode(QX11Info::display(), XK_Alt_L), false, 0);
      XFlush(QX11Info::display());
   }
}

// cause of cmake
#include "oxygen.moc"

