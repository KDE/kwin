//////////////////////////////////////////////////////////////////////////////
// oxygenclient.cpp
// -------------------
// Oxygen window decoration for KDE
// -------------------
// Copyright (c) 2003, 2004 David Johnson
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
//
// Please see the header file for copyright and license information.
//////////////////////////////////////////////////////////////////////////////
// #ifndef OXYGENCLIENT_H
// #define OXYGENCLIENT_H

#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kdebug.h>

#include <qbitmap.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qtooltip.h>
//Added by qt3to4:
#include <QBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QShowEvent>
#include <QPaintEvent>
#include <QPainterPath>
#include <QTimer>
#include <QCache>

#include "oxygenclient.h"
#include "oxygenclient.moc"
#include "oxygenbutton.h"
#include "oxygen.h"

#include "definitions.cpp"

namespace Oxygen
{

// global constants

// static const int BUTTONSIZE      = 18;
// static const int DECOSIZE        = 8;
// static const int TITLESIZE       = 18;
// static const int TFRAMESIZE       = 8;
// static const int BFRAMESIZE       = 7;
// static const int LFRAMESIZE       = 8;
// static const int RFRAMESIZE       = 7;BUTTONSIZE     
// static const int FRAMEBUTTONSPACE       = 67;

class TileCache
{
    private:
        TileCache();

    public:
        ~TileCache() {}
        static TileCache *instance();

        QPixmap verticalGradient(const QColor &color, int height);
        QPixmap radialGradient(const QColor &color, int width);

    private:
        static TileCache *s_instance;
        QCache<quint64, QPixmap> m_cache;
};

TileCache *TileCache::s_instance = 0;

TileCache::TileCache()
{
    m_cache.setMaxCost(64);
}

TileCache *TileCache::instance()
{
    if (!s_instance)
        s_instance = new TileCache;

    return s_instance;
}

QPixmap TileCache::verticalGradient(const QColor &color, int height)
{
    quint64 key = (quint64(color.rgba()) << 32) | height | 0x8000;
    QPixmap *pixmap = m_cache.object(key);

    if (!pixmap)
    {
        pixmap = new QPixmap(32, height);

        QLinearGradient gradient(0, 0, 0, height);
        gradient.setColorAt(0, color.lighter(115));
        gradient.setColorAt(1, color);

        QPainter p(pixmap);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(pixmap->rect(), gradient);

        m_cache.insert(key, pixmap);
    }

    return *pixmap;
}

QPixmap TileCache::radialGradient(const QColor &color, int width)
{
    quint64 key = (quint64(color.rgba()) << 32) | width | 0xb000;
    QPixmap *pixmap = m_cache.object(key);

    if (!pixmap)
    {
        width /= 2;
        pixmap = new QPixmap(width, 64);
        pixmap->fill(QColor(0,0,0,0));
        QColor radialColor = color.lighter(140);
        radialColor.setAlpha(255);
        QRadialGradient gradient(64, TITLESIZE + TFRAMESIZE, 64);
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

        m_cache.insert(key, pixmap);
    }

    return *pixmap;
}


void renderDot(QPainter *p, const QPointF &point, qreal diameter)
{
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
}

// window button decorations
static const unsigned char close_bits[] = {
    0xc3, 0x66, 0x7e, 0x3c, 0x3c, 0x7e, 0x66, 0xc3};

static const unsigned char help_bits[] = {
    0x7e, 0x7e, 0x60, 0x78, 0x78, 0x00, 0x18, 0x18};

static const unsigned char max_bits[] = {
    0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00};

static const unsigned char min_bits[] = {
//    0x00, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00};
    0x00, 0x00, 0x38, 0x38, 0x38, 0x00, 0x00, 0x00};

static const unsigned char minmax_bits[] = {
    0x00, 0x02, 0x06, 0x0e, 0x1e, 0x3e, 0x7e, 0x00};

static const unsigned char stickydown_bits[] = {
    0x00, 0x18, 0x18, 0x7e, 0x7e, 0x18, 0x18, 0x00};

static const unsigned char sticky_bits[] = {
    0x00, 0x00, 0x00, 0x7e, 0x7e, 0x00, 0x00, 0x00};

//////////////////////////////////////////////////////////////////////////////
// OxygenClient Class                                                      //
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// OxygenClient()
// ---------------
// Constructor

OxygenClient::OxygenClient(KDecorationBridge *b, KDecorationFactory *f)
    : KDecoration(b, f) { ; }

OxygenClient::~OxygenClient()
{
    for (int n=0; n<ButtonTypeCount; n++) {
        if (button[n]) delete button[n];
    }
}

//////////////////////////////////////////////////////////////////////////////
// init()
// ------
// Actual initializer for class

static QTimer updateTimer;

void OxygenClient::init()
{
    createMainWidget(); //PORT  Qt::WResizeNoErase | Qt::WNoAutoErase);
    widget()->setAutoFillBackground(false);
    widget()->setAttribute(Qt::WA_OpaquePaintEvent);
    widget()->setAttribute( Qt::WA_PaintOnScreen, false);
    widget()->installEventFilter(this);

    // setup layout
    QGridLayout *mainlayout = new QGridLayout(widget());
    QHBoxLayout *titlelayout = new QHBoxLayout();
    titlebar_ = new QSpacerItem(1, TITLESIZE, QSizePolicy::Expanding,
                                QSizePolicy::Fixed);

    mainlayout->addItem(new QSpacerItem(LFRAMESIZE, TFRAMESIZE), 0, 0);
    mainlayout->addItem(new QSpacerItem(0, BFRAMESIZE), 3, 0);
    mainlayout->addItem(new QSpacerItem(RFRAMESIZE, 0), 0, 2);

    mainlayout->addLayout(titlelayout, 1, 1);
    if (isPreview()) {
        mainlayout->addWidget(
        new QLabel(i18n("<b><center>Oxygen preview! =)</center></b>"),
        widget()), 2, 1);
    } else {
        mainlayout->addItem(new QSpacerItem(0, 0), 2, 1);
    }

    mainlayout->setRowStretch(2, 10);
    mainlayout->setColumnStretch(1, 10);

    // setup titlebar buttons
    for (int n=0; n<ButtonTypeCount; n++) {
        button[n] = 0;
    }

    addButtons(titlelayout, options()->titleButtonsLeft());
    titlelayout->addItem(titlebar_);
    addButtons(titlelayout, options()->titleButtonsRight());
    titlelayout->addSpacing(2);

    titlelayout->setSpacing(0);
    titlelayout->setMargin(0);
    mainlayout->setSpacing(0);
    mainlayout->setMargin(0);
}

//////////////////////////////////////////////////////////////////////////////
// addButtons()
// ------------
// Add buttons to title layout

void OxygenClient::addButtons(QHBoxLayout *layout, const QString& s)
{
    const unsigned char *bitmap;
    QString tip;

    if (s.length() > 0) {
        for (int n=0; n < s.length(); n++) {
            if(n>0)
                layout->addSpacing(3);
            switch (s[n].toLatin1()) {
              case 'M': // Menu button
                  if (!button[ButtonMenu]) {
                      button[ButtonMenu] =
                          new OxygenButton(this, i18n("Menu"), ButtonMenu, 0);
                      connect(button[ButtonMenu], SIGNAL(pressed()),
                              this, SLOT(menuButtonPressed()));
                      layout->addWidget(button[ButtonMenu]);
                  }
                  break;

              case 'S': // Sticky button
                  if (!button[ButtonSticky]) {
              if (isOnAllDesktops()) {
              bitmap = stickydown_bits;
              tip = i18n("Un-Sticky");
              } else {
              bitmap = sticky_bits;
              tip = i18n("Sticky");
              }
                      button[ButtonSticky] =
                          new OxygenButton(this, tip, ButtonSticky, bitmap);
                      connect(button[ButtonSticky], SIGNAL(clicked()),
                              this, SLOT(toggleOnAllDesktops()));
                      layout->addWidget(button[ButtonSticky]);
                  }
                  break;

              case 'H': // Help button
                  if ((!button[ButtonHelp]) && providesContextHelp()) {
                      button[ButtonHelp] =
                          new OxygenButton(this, i18n("Help"), ButtonHelp, help_bits);
                      connect(button[ButtonHelp], SIGNAL(clicked()),
                              this, SLOT(showContextHelp()));
                      layout->addWidget(button[ButtonHelp]);
                  }
                  break;

              case 'I': // Minimize button
                  if ((!button[ButtonMin]) && isMinimizable())  {
                      button[ButtonMin] =
                          new OxygenButton(this, i18n("Minimize"), ButtonMin, min_bits);
                      connect(button[ButtonMin], SIGNAL(clicked()),
                              this, SLOT(minimize()));
                      layout->addWidget(button[ButtonMin]);
                  }
                  break;

              case 'A': // Maximize button
                  if ((!button[ButtonMax]) && isMaximizable()) {
              if (maximizeMode() == MaximizeFull) {
              bitmap = minmax_bits;
              tip = i18n("Restore");
              } else {
              bitmap = max_bits;
              tip = i18n("Maximize");
              }
                      button[ButtonMax]  =
                          new OxygenButton(this, tip, ButtonMax, bitmap);
                      connect(button[ButtonMax], SIGNAL(clicked()),
                              this, SLOT(maxButtonPressed()));
                      layout->addWidget(button[ButtonMax]);
                  }
                  break;

              case 'X': // Close button
                  if ((!button[ButtonClose]) && isCloseable()) {
                      button[ButtonClose] =
                          new OxygenButton(this, i18n("Close"), ButtonClose, close_bits);
                      connect(button[ButtonClose], SIGNAL(clicked()),
                              this, SLOT(closeWindow()));
                      layout->addWidget(button[ButtonClose]);
                  }
                  break;

              case '_': // Spacer item
                  layout->addSpacing(FRAMEBUTTONSPACE);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// activeChange()
// --------------
// window active state has changed

void OxygenClient::activeChange()
{
    for (int n=0; n<ButtonTypeCount; n++)
        if (button[n]) button[n]->reset();
    widget()->repaint();
}

//////////////////////////////////////////////////////////////////////////////
// captionChange()
// ---------------
// The title has changed

void OxygenClient::captionChange()
{
    widget()->repaint(titlebar_->geometry());
}

//////////////////////////////////////////////////////////////////////////////
// desktopChange()
// ---------------
// Called when desktop/sticky changes

void OxygenClient::desktopChange()
{
    bool d = isOnAllDesktops();
    if (button[ButtonSticky]) {
        button[ButtonSticky]->setToolTip( d ? i18n("Un-Sticky") : i18n("Sticky"));
    }
}

//////////////////////////////////////////////////////////////////////////////
// iconChange()
// ------------
// The title has changed

void OxygenClient::iconChange()
{
    if (button[ButtonMenu]) {
        button[ButtonMenu]->repaint();
    }
}

//////////////////////////////////////////////////////////////////////////////
// maximizeChange()
// ----------------
// Maximized state has changed

void OxygenClient::maximizeChange()
{
    bool m = (maximizeMode() == MaximizeFull);
    if (button[ButtonMax]) {
        button[ButtonMax]->setToolTip(m ? i18n("Restore") : i18n("Maximize"));
    }
}

//////////////////////////////////////////////////////////////////////////////
// shadeChange()
// -------------
// Called when window shading changes

void OxygenClient::shadeChange()
{ ; }

//////////////////////////////////////////////////////////////////////////////
// borders()
// ----------
// Get the size of the borders

void OxygenClient::borders(int &l, int &r, int &t, int &b) const
{
    l = LFRAMESIZE;
    r = RFRAMESIZE;
    t = TITLESIZE + TFRAMESIZE;
    b = BFRAMESIZE;
}

//////////////////////////////////////////////////////////////////////////////
// resize()
// --------
// Called to resize the window

void OxygenClient::resize(const QSize &size)
{
    widget()->resize(size);
}

//////////////////////////////////////////////////////////////////////////////
// minimumSize()
// -------------
// Return the minimum allowable size for this window

QSize OxygenClient::minimumSize() const
{
    return widget()->minimumSize();
}

//////////////////////////////////////////////////////////////////////////////
// mousePosition()
// ---------------
// Return logical mouse position

KDecoration::Position OxygenClient::mousePosition(const QPoint &point) const
{
    const int corner = 24;
    Position pos;

    if (point.y() <= TFRAMESIZE) {
        // inside top frame
        if (point.x() <= corner)                 pos = PositionTopLeft;
        else if (point.x() >= (width()-corner))  pos = PositionTopRight;
        else                                     pos = PositionTop;
    } else if (point.y() >= (height()-BFRAMESIZE)) {
        // inside handle
        if (point.x() <= corner)                 pos = PositionBottomLeft;
        else if (point.x() >= (width()-corner))  pos = PositionBottomRight;
        else                                     pos = PositionBottom;
    } else if (point.x() <= LFRAMESIZE) {
        // on left frame
        if (point.y() <= corner)                 pos = PositionTopLeft;
        else if (point.y() >= (height()-corner)) pos = PositionBottomLeft;
        else                                     pos = PositionLeft;
    } else if (point.x() >= width()-RFRAMESIZE) {
        // on right frame
        if (point.y() <= corner)                 pos = PositionTopRight;
        else if (point.y() >= (height()-corner)) pos = PositionBottomRight;
        else                                     pos = PositionRight;
    } else {
        // inside the frame
        pos = PositionCenter;
    }
    return pos;
}

//////////////////////////////////////////////////////////////////////////////
// eventFilter()
// -------------
// Event filter

bool OxygenClient::eventFilter(QObject *obj, QEvent *e)
{
    if (obj != widget()) return false;

    switch (e->type()) {
      case QEvent::MouseButtonDblClick: {
          mouseDoubleClickEvent(static_cast<QMouseEvent *>(e));
          return true;
      }
      case QEvent::MouseButtonPress: {
          processMousePressEvent(static_cast<QMouseEvent *>(e));
          return true;
      }
      case QEvent::Paint: {
          paintEvent(static_cast<QPaintEvent *>(e));
          return true;
      }
      case QEvent::Show: {
          showEvent(static_cast<QShowEvent *>(e));
          return true;
      }
      default: {
          return false;
      }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// mouseDoubleClickEvent()
// -----------------------
// Doubleclick on title

void OxygenClient::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (titlebar_->geometry().contains(e->pos())) titlebarDblClickOperation();
}

//////////////////////////////////////////////////////////////////////////////
// paintEvent()
// ------------
// Repaint the window

void OxygenClient::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    if (!OxygenFactory::initialized()) return;

    doShape();

    QPalette palette = widget()->palette();
    QPainter painter(widget());

    QRect title(titlebar_->geometry());

    int x,y,w,h;
    QRect frame = widget()->frameGeometry();
    QColor color = palette.window();
//color = QColor(Qt::red);
/*

    QLinearGradient gradient(0, 0, 0, frame.height());
    gradient.setColorAt(0, color.lighter(110));
    gradient.setColorAt(1, color.darker(110));
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(QRect(0, 0, frame.width(), TITLESIZE + TFRAMESIZE), gradient);
    painter.fillRect(QRect(0, 0, LFRAMESIZE, frame.height()), gradient);
    painter.fillRect(QRect(0, frame.height() - BFRAMESIZE -1,
                                                        frame.width(), BFRAMESIZE), gradient);
    painter.fillRect(QRect(frame.width()-RFRAMESIZE, 0,
                                                        RFRAMESIZE, frame.height()), gradient);

*/

    int splitY = qMin(300, 3*frame.height()/4);

    QPixmap tile = TileCache::instance()->verticalGradient(color, splitY);
    painter.drawTiledPixmap(QRect(0, 0, frame.width(), TITLESIZE + TFRAMESIZE), tile);

    painter.drawTiledPixmap(QRect(0, 0, LFRAMESIZE, splitY), tile);
    painter.fillRect(0, splitY, LFRAMESIZE, frame.height() - splitY, color);

    painter.drawTiledPixmap(QRect(frame.width()-RFRAMESIZE, 0,
                                                        RFRAMESIZE, splitY), tile,
                                                        QPoint(frame.width()-RFRAMESIZE, 0));
    painter.fillRect(frame.width()-RFRAMESIZE, splitY, RFRAMESIZE, frame.height() - splitY, color);

    painter.fillRect(0, frame.height() - BFRAMESIZE, frame.width(), BFRAMESIZE, color);

    int radialW = qMin(600, frame.width());
    tile = TileCache::instance()->radialGradient(color, radialW);
    QRect radialRect = QRect((frame.width() - radialW) / 2, 0, radialW, 64);
    painter.drawPixmap(radialRect, tile);

    //painter.setRenderHint(QPainter::Antialiasing,true);

    // draw title text
    painter.setFont(options()->font(isActive(), false));
    painter.setBrush(palette.windowText());
    painter.drawText(title.x(), title.y(), title.width(), title.height(),
              OxygenFactory::titleAlign() | Qt::AlignVCenter, caption());


    painter.setRenderHint(QPainter::Antialiasing);

    // shadows of the frame
    frame = widget()->rect();
    frame.getRect(&x, &y, &w, &h);

    painter.setBrush(Qt::NoBrush);
    QLinearGradient lg(0, 0, 0, 10);
    QGradientStops stops;
    stops << QGradientStop( 0, QColor(255,255,255, 110) )
           << QGradientStop( 1, QColor(128,128,128, 60) );
    lg.setStops(stops);
    painter.setPen(QPen(QBrush(lg),1));
    painter.drawLine(QPointF(6.3, 0.5), QPointF(w-6.3, 0.5));
    painter.drawArc(QRectF(0.5, 0.5, 9.5, 9.5),90*16, 90*16);
    painter.drawArc(QRectF(w-9.5-0.5, 0.5, 9.5, 9.5), 0, 90*16);

    painter.setPen(QColor(128,128,128, 60));
    painter.drawLine(QPointF(0.5, 6.3), QPointF(0.5, h-6.3));
    painter.drawLine(QPointF(w-0.5, 6.3), QPointF(w-0.5, h-6.3));

    lg = QLinearGradient(0, h-10, 0, h);
    stops.clear();
    stops << QGradientStop( 0, QColor(128,128,128, 60) )
           << QGradientStop( 1, QColor(0,0,0, 50) );
    lg.setStops(stops);
    painter.setPen(QPen(QBrush(lg),1));
    painter.drawArc(QRectF(0.5, h-9.5-0.5, 9.5, 9.5),180*16, 90*16);
    painter.drawArc(QRectF(w-9.5-0.5, h-9.5-0.5, 9.5, 9.5), 270*16, 90*16);
    painter.drawLine(QPointF(6.3, h-0.5), QPointF(w-6.3, h-0.5));

    qreal cenY = frame.height() / 2 + 0.5;
    qreal posX = frame.width() - 2.5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 66));
    renderDot(&painter, QPointF(posX, cenY - 5), 2.5);
    renderDot(&painter, QPointF(posX, cenY), 2.5);
    renderDot(&painter, QPointF(posX, cenY + 5), 2.5);

    painter.translate(frame.width()-9, frame.height()-9);
    renderDot(&painter, QPointF(0.5, 6.5), 2.5);
    renderDot(&painter, QPointF(5.5, 5.5), 2.5);
    renderDot(&painter, QPointF(6.5, 0.5), 2.5);
}

void OxygenClient::doShape()
{
  int r=widget()->width();
  int b=widget()->height();
QRegion mask(0,0,r,b);
    // Remove top-left corner.
    mask -= QRegion(0, 0, 3, 1);
    mask -= QRegion(0, 1, 2, 1);
    mask -= QRegion(0, 2, 1, 1);

    // Remove top-right corner.
    mask -= QRegion(r - 3, 0, 3, 1);
    mask -= QRegion(r - 2, 1, 2, 1);
    mask -= QRegion(r - 1, 2, 1, 1);

    // Remove bottom-left corner.
    mask -= QRegion(0, b-1-0, 3, b-1-1);
    mask -= QRegion(0, b-1-1, 2, b-1-1);
    mask -= QRegion(0, b-1-2, 1, b-1-1);

    // Remove bottom-right corner.
    mask -= QRegion(r - 3, b-1-0, 3, b-1-1);
    mask -= QRegion(r - 2, b-1-1, 2, b-1-1);
    mask -= QRegion(r - 1, b-1-2, 1, b-1-1);

    setMask(mask);
}

//////////////////////////////////////////////////////////////////////////////
// showEvent()
// -----------
// Window is being shown

void OxygenClient::showEvent(QShowEvent *)
{
   widget()->repaint();
}

//////////////////////////////////////////////////////////////////////////////
// maxButtonPressed()
// -----------------
// Max button was pressed

void OxygenClient::maxButtonPressed()
{
    if (button[ButtonMax]) {
        switch (button[ButtonMax]->lastMousePress()) {
          case Qt::MidButton:
              maximize(maximizeMode() ^ MaximizeVertical);
              break;
          case Qt::RightButton:
              maximize(maximizeMode() ^ MaximizeHorizontal);
              break;
          default:
              (maximizeMode() == MaximizeFull) ? maximize(MaximizeRestore)
                  : maximize(MaximizeFull);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// menuButtonPressed()
// -------------------
// Menu button was pressed (popup the menu)

void OxygenClient::menuButtonPressed()
{
    if (button[ButtonMenu]) {
        QPoint p(button[ButtonMenu]->rect().bottomLeft().x(),
                 button[ButtonMenu]->rect().bottomLeft().y());
        KDecorationFactory* f = factory();
        showWindowMenu(button[ButtonMenu]->mapToGlobal(p));
        if (!f->exists(this)) return; // decoration was destroyed
        button[ButtonMenu]->setDown(false);
    }
}

} //namespace Oxygen

//#include "oxygenclient.moc"

// #endif
