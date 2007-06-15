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
#include <QX11Info>
#include <X11/Xatom.h>

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
    widget()->setAutoFillBackground(true);
    widget()->installEventFilter(this);

    // setup layout
    QGridLayout *mainlayout = new QGridLayout(widget());
    QHBoxLayout *titlelayout = new QHBoxLayout();
    titlebar_ = new QSpacerItem(1, TITLESIZE, QSizePolicy::Expanding,
                                QSizePolicy::Fixed);

    mainlayout->addItem(new QSpacerItem(LFRAMESIZE, TFRAMESIZE), 0, 0);
//     mainlayout->addItem(new QSpacerItem(RFRAMESIZE, BFRAMESIZE), 2, 2);
    mainlayout->addItem(new QSpacerItem(0, TFRAMESIZE), 3, 0);
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

    titlelayout->addSpacing(5);
    addButtons(titlelayout, options()->titleButtonsLeft());
    titlelayout->addSpacing(15);
    titlelayout->addItem(titlebar_);
    titlelayout->addSpacing(15);
    addButtons(titlelayout, options()->titleButtonsRight());
    titlelayout->addSpacing(5);

    titlelayout->setSpacing(0);
    titlelayout->setMargin(0);
    mainlayout->setSpacing(0);
    mainlayout->setMargin(0);
   
    setDecoDim(TITLESIZE + TFRAMESIZE, BFRAMESIZE, LFRAMESIZE, RFRAMESIZE);
    fallbackDeco = false;
    xPic[0] = xPic[1] = xPic[2] = xPic[3] = 0L;
    connect (&updateTimer, SIGNAL(timeout()), this, SLOT(updateDecoPics()));
    if (!updateTimer.isActive())
       updateTimer.start(250);

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
        button[ButtonSticky]->setBitmap(d ? stickydown_bits : sticky_bits);
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
        button[ButtonMenu]->setBitmap(0);
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
        button[ButtonMax]->setBitmap(m ? minmax_bits : max_bits);
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
      case QEvent::Resize: {
          resizeEvent(static_cast<QResizeEvent *>(e));
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
    if (!OxygenFactory::initialized()) return;

    doShape();

    QPalette palette = widget()->palette();
    QPainter painter(widget());
    painter.setRenderHint(QPainter::Antialiasing,true);

    QRect title(titlebar_->geometry());

    // the background

    updateDecoPics();

    int x,y,w,h;

    if (fallbackDeco)
        painter.fillRect(widget()->rect(), Qt::red);
    else {
      // explanation if you don't know render syntax:
      //------------------------------------------------
      // XRenderComposite (dpy, op,
      // src.x11PictureHandle(), mask, dst.x11PictureHandle(),
      // sx, sy, mx, my, dx, dy, w, h);
      //=====================================================
        QRect winRect = widget()->rect();
        winRect.getRect(&x,&y,&w,&h);
        x = e->rect().x();
        y = e->rect().y();
        int updW = e->rect().width();
        int updH = e->rect().height();

        Picture window = widget()->x11PictureHandle();
        // left
        if (x < LFRAMESIZE)
            XRenderComposite (QX11Info::display(), PictOpSrc, xPic[2], 0, window,
                        x, y, 0, 0, x, y, qMin(LFRAMESIZE - x, updW), qMin(h - y, updH));
        // right
        if (x+updW >= w - RFRAMESIZE - 1) {
            int paintW = updW;
            if(x < w-RFRAMESIZE-1)
                paintW = updW - (w - RFRAMESIZE - 1) + x;
            XRenderComposite (QX11Info::display(), PictOpSrc, xPic[3], 0, window,
                        qMax(x-(w-RFRAMESIZE),0), y, 0, 0,
                        qMax(x, w-RFRAMESIZE-1), y, paintW, qMin(h - y, updH));
        }

        if (x < LFRAMESIZE) {   // don't paint left frame again
           updW -= (LFRAMESIZE - x);
           x = LFRAMESIZE;
        }
        if (x + updW > w - RFRAMESIZE) // dont paint right frame again
            updW = w - RFRAMESIZE - x;

        // top
        if (y < TITLESIZE + TFRAMESIZE)
            XRenderComposite (QX11Info::display(), PictOpSrc, xPic[0], 0, window,
                        x-LFRAMESIZE, y, 0, 0, x, y, updW, qMin(TITLESIZE + TFRAMESIZE - y, updH));
        // bottom
        if (y + updH >= h - BFRAMESIZE - 1) {
            int paintH = updH;
            if(y < h-BFRAMESIZE-1)
                paintH = updH - (h - BFRAMESIZE - 1) + y;
            XRenderComposite (QX11Info::display(), PictOpSrc, xPic[1], 0, window,
                        x-LFRAMESIZE, qMax(y-(h-BFRAMESIZE),0), 0, 0,
                        x, qMax(y, h-BFRAMESIZE-1), updW, paintH);
        }

        XFlush(QX11Info::display()); // must be?! - the pictures will change soon
    }

    // draw title text
    painter.setFont(options()->font(isActive(), false));
    painter.setBrush(palette.windowText());
    painter.drawText(title.x(), title.y(), title.width(), title.height(),
              OxygenFactory::titleAlign() | Qt::AlignVCenter, caption());
#if 0
    // draw frame
    QRect frame(0, 0, width(), TFRAMESIZE);
    painter.fillRect(frame, palette.window());
    frame.setRect(0, 0, LFRAMESIZE, height());
    painter.fillRect(frame, palette.window());
    frame.setRect(0, height() - BFRAMESIZE, width(), BFRAMESIZE);
    painter.fillRect(frame, palette.window());
    frame.setRect(width()-RFRAMESIZE, 0, RFRAMESIZE, height());
    painter.fillRect(frame, palette.window());
#endif
    
    // Draw depression lines where the buttons are
    QLinearGradient grad1(LFRAMESIZE, TFRAMESIZE + title.height()/2, title.x(), TFRAMESIZE + title.height()/2);
    grad1.setColorAt(0.0, QColor(0,0,0,64));
    grad1.setColorAt(1.0, QColor(0,0,0,5));
    QBrush brush1(grad1);
    painter.fillRect(LFRAMESIZE, TFRAMESIZE + title.height()/2-1, title.x(), 1, brush1);
    QLinearGradient grad2(LFRAMESIZE, TFRAMESIZE + title.height()/2, title.x(), TFRAMESIZE + title.height()/2);
    grad2.setColorAt(0.0, QColor(255,255,255,128));
    grad2.setColorAt(1.0, QColor(255,255,255,5));
    QBrush brush2(grad2);
    painter.fillRect(LFRAMESIZE, TFRAMESIZE + title.height()/2, title.x(), 1, brush2);
    QLinearGradient grad3(width()-RFRAMESIZE, TFRAMESIZE + title.height()/2, title.right(), TFRAMESIZE + title.height()/2);
    grad3.setColorAt(0.0, QColor(0,0,0,64));
    grad3.setColorAt(1.0, QColor(0,0,0,5));
    QBrush brush3(grad3);
    painter.fillRect(title.right(), TFRAMESIZE + title.height()/2-1, width() - title.right()-RFRAMESIZE, 1, brush3);
    QLinearGradient grad4(width()-RFRAMESIZE, TFRAMESIZE + title.height()/2, title.right(), TFRAMESIZE + title.height()/2);
    grad4.setColorAt(0.0, QColor(255,255,255,128));
    grad4.setColorAt(1.0, QColor(255,255,255,5));
    QBrush brush4(grad4);
    painter.fillRect(title.right(), TFRAMESIZE + title.height()/2, width() -title.right()-RFRAMESIZE, 1, brush4);

    painter.setRenderHint(QPainter::Antialiasing);

    // shadows of the frame
    QRect frame = widget()->rect();
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

    qreal cenX = frame.width() / 2 + 0.5;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 66));
    renderDot(&painter, QPointF(cenX-10, 2.5), 1);
    renderDot(&painter, QPointF(cenX-5, 2.5), 1.5);
    renderDot(&painter, QPointF(cenX, 2.5), 2);
    renderDot(&painter, QPointF(cenX+5, 2.5), 1.5);
    renderDot(&painter, QPointF(cenX+10, 2.5), 1);

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
// resizeEvent()
// -------------
// Window is being resized

void OxygenClient::resizeEvent(QResizeEvent *)
{
    if (widget()->isVisible()) {
        QRegion region = widget()->rect();
        region = region.subtract(titlebar_->geometry());
    widget()->update();
    }
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

//////////////////////////////////////////////////////////////////////////////
// updateDecoPics()
// -----------------
// The function queries the X server for the location of the bg pixmaps
// (XRender pictures), provided by the style
// as these pictures can get lost or updated anytime (e.g. if the user
// deselects the oxygen style), it's a slot and bound to a 250ms timer, i.e.
// every quarter second we check if we're still using the proper and if not,
// trigger a repaint

void OxygenClient::updateDecoPics()
{
   if (readDecoPics())
      widget()->update();
}

static const Atom oxygen_deco_top =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_TOP", False);
static const Atom oxygen_deco_bottom =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_BOTTOM", False);
static const Atom oxygen_deco_left =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_LEFT", False);
static const Atom oxygen_deco_right =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_RIGHT", False);

#define READ_PIC(_ATOM_, _CARD_)\
   result = XGetWindowProperty(QX11Info::display(), windowId(), \
                               _ATOM_, 0L, 1L, False, XA_CARDINAL, \
                               &actual, &format, &n, &left, &data); \
   if (result == Success && data != None) \
      memcpy (&_CARD_, data, sizeof (unsigned int)); \
   else {\
      _CARD_ = 0; \
      fallbackDeco = true;\
   }//

// this function reads out the Render Pictures the style should have stacked
// onto the server
//notice that on any read failure, we'll fall back to a simple deco

bool OxygenClient::readDecoPics()
{
   fallbackDeco = false;
   
   unsigned char *data = 0;
   Atom actual;
   int format, result;
   unsigned long n, left;

// DON'T change the order atom -> pixmap
   Picture oldPic = xPic[0];
   READ_PIC(oxygen_deco_top, xPic[0]);
   if (xPic[0] == oldPic) // there has been NO update
      return false;
   READ_PIC(oxygen_deco_bottom, xPic[1]);
   READ_PIC(oxygen_deco_left, xPic[2]);
   READ_PIC(oxygen_deco_right, xPic[3]);
   
   return true; // has been an update
}

static const Atom oxygen_decoDim =
XInternAtom(QX11Info::display(), "OXYGEN_DECO_DIM", False);

// this function tells the deco about how much oversize it should create for
// the bg pixmap (each dim must be < 256)
// it's called in init() and should be recalled whenever the sizes get changed
// for some reason
void OxygenClient::setDecoDim(int top, int bottom, int left, int right) {
// DON'T change the order
   int tmp = (top & 0xff) |
             ((bottom & 0xff) << 8) |
             ((left & 0xff) << 16) |
             ((right & 0xff) <<24 );
   XChangeProperty(QX11Info::display(), windowId(), oxygen_decoDim, XA_CARDINAL,
   32, PropModeReplace, (unsigned char *) &(tmp), 1L);
}

} //namespace Oxygen

//#include "oxygenclient.moc"

// #endif
