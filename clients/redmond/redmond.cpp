/*
 * $Id$
 *
 * Redmond KWin client
 *
 * Copyright 2001
 *   Karol Szwed <gallium@kde.org>
 *   http://gallium.n3.net/
 *
 * Based on the default KWin client.
 *
 * Updated to support toolwindows 3/2001 (KS)
 *
 */

#include "redmond.h"

#include <qlayout.h>
#include <qdrawutil.h>
#include <qdatetime.h>
#include <kpixmapeffect.h>
#include <kimageeffect.h>
#include <kdrawutil.h>
#include <klocale.h>

#include <qbitmap.h>
#include <qtooltip.h>
#include <qimage.h>
#include <qlabel.h>
#include <qapplication.h>

namespace Redmond {

static const char *kdelogo[] = {
/* columns rows colors chars-per-pixel */
"16 16 8 1",
" 	c None",
".	c #000000",
"+	c #A0A0A4",
"@	c #FFFFFF",
"#	c #585858",
"$	c #C0C0C0",
"%	c #808080",
"&	c #DCDCDC",
"                ",
"     ..    ..   ",
"    .+@.  .@#.  ",
"   .@@@. .@@@#  ",
"   .@@@..$@@$.  ",
"   .@@@.@@@$.   ",
"   .@@@%@@$.    ",
"   .@@@&@@.     ",
"   .@@@@@@.     ",
"   .@@@$@@&.    ",
"   .@@@.@@@.    ",
"   .@@@.+@@@.   ",
"   .@@@..$@@&.  ",
"   .@@%. .@@@.  ",
"   ....   ...   ",
"                "};

static unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00};

static unsigned char close_bits[] = {
  0x00, 0x00, 0x86, 0x01, 0xcc, 0x00, 0x78, 0x00, 0x30, 0x00, 0x78, 0x00,
  0xcc, 0x00, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00};

static unsigned char maximize_bits[] = {
  0xff, 0x01, 0xff, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0xff, 0x01, 0x00, 0x00};

static unsigned char minmax_bits[] = {
  0xfc, 0x00, 0xfc, 0x00, 0x84, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0xe1, 0x00,
  0x21, 0x00, 0x21, 0x00, 0x3f, 0x00, 0x00, 0x00};

static unsigned char question_bits[] = {
  0x00, 0x00, 0x3c, 0x00, 0x66, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00,
  0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00};


// Up / Down titlebar button images
static KPixmap *btnPix1;
static KPixmap *iBtnPix1;
static KPixmap *btnDownPix1;
static KPixmap *iBtnDownPix1;

static KPixmap *miniBtnPix1;
static KPixmap *iMiniBtnPix1;
static KPixmap *miniBtnDownPix1;
static KPixmap *iMiniBtnDownPix1;

static QPixmap *defaultMenuPix;
static QColor  *btnForeground;
static bool    pixmaps_created = false;

static int toolTitleHeight;
static int normalTitleHeight;
static int borderWidth;

static inline const KDecorationOptions *options()
{
   return KDecoration::options();
}

static void drawButtonFrame( KPixmap *pix, const QColorGroup &g, bool sunken )
{
    QPainter p;
    int x2 = pix->width() - 1;
    int y2 = pix->height() - 1;
    p.begin(pix);

    // titlebar button frame
    p.setPen( sunken ? g.dark().dark(155) : g.light());
    p.drawLine(0, 0, x2-1, 0);
    p.drawLine(0, 0, 0, y2-1);

    if (sunken)
    {
       p.setPen( g.mid().dark(135) );
       p.drawLine(1, 1, x2-2, 1);
       p.drawLine(1, 1, 1, y2-2);
    }

    p.setPen( sunken ? g.light() : g.mid().dark(135));
    p.drawLine(1, y2-1, x2-1, y2-1);
    p.drawLine(x2-1, 1, x2-1, y2-1);

    p.setPen( sunken ? g.light() : g.dark().dark(155));
    p.drawLine(0, y2, x2, y2);
    p.drawLine(x2, 0, x2, y2);
}


static void create_pixmaps ()
{
    if (pixmaps_created)
        return;

    pixmaps_created = true;

    bool highcolor = QPixmap::defaultDepth() > 8;

    btnPix1 = new KPixmap;
    btnDownPix1 = new KPixmap;
    iBtnPix1 = new KPixmap;
    iBtnDownPix1 = new KPixmap;
    miniBtnPix1 = new KPixmap;
    miniBtnDownPix1 = new KPixmap;
    iMiniBtnPix1 = new KPixmap;
    iMiniBtnDownPix1 = new KPixmap;
    defaultMenuPix = new QPixmap(kdelogo);

    // buttons (active/inactive, sunken/unsunken)
	QColorGroup g = options()->colorGroup(KDecoration::ColorButtonBg, true);
    QColor c = g.background();
    btnPix1->resize(normalTitleHeight, normalTitleHeight-2);
    btnDownPix1->resize(normalTitleHeight, normalTitleHeight-2);
    iBtnPix1->resize(normalTitleHeight, normalTitleHeight-2);
    iBtnDownPix1->resize(normalTitleHeight, normalTitleHeight-2);

    miniBtnPix1->resize(toolTitleHeight, toolTitleHeight);
    miniBtnDownPix1->resize(toolTitleHeight, toolTitleHeight);
    iMiniBtnPix1->resize(toolTitleHeight, toolTitleHeight);
    iMiniBtnDownPix1->resize(toolTitleHeight, toolTitleHeight);

    if (highcolor && false) {
        KPixmapEffect::gradient(*btnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*btnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);

        KPixmapEffect::gradient(*miniBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*miniBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);

        g = options()->colorGroup(KDecoration::ColorButtonBg, false);
        c = g.background();
        KPixmapEffect::gradient(*iBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iMiniBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iMiniBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
    } else {
        btnPix1->fill(c.rgb());
        btnDownPix1->fill(c.rgb());
        miniBtnPix1->fill(c.rgb());
        miniBtnDownPix1->fill(c.rgb());

        g = options()->colorGroup(KDecoration::ColorButtonBg, false);
        c = g.background();
        iBtnPix1->fill(c.rgb());
        iBtnDownPix1->fill(c.rgb());
        iMiniBtnPix1->fill(c.rgb());
        iMiniBtnDownPix1->fill(c.rgb());
    }

    g = options()->colorGroup(KDecoration::ColorButtonBg, true);
    drawButtonFrame(btnPix1, g, false);
    drawButtonFrame(btnDownPix1, g, true);
    drawButtonFrame(miniBtnPix1, g, false);
    drawButtonFrame(miniBtnDownPix1, g, true);

    g = options()->colorGroup(KDecoration::ColorButtonBg, false);
    drawButtonFrame(iBtnPix1, g, false);
    drawButtonFrame(iBtnDownPix1, g, true);
    drawButtonFrame(iMiniBtnPix1, g, false);
    drawButtonFrame(iMiniBtnDownPix1, g, true);

    // Make sure button pixmaps contrast with the current colour scheme.
    if (qGray(options()->color(KDecoration::ColorButtonBg, true).rgb()) > 127)
        btnForeground = new QColor(Qt::black);
    else
        btnForeground = new QColor(Qt::white);
}

void delete_pixmaps()
{
    delete btnPix1;
    delete btnDownPix1;
    delete iBtnPix1;
    delete iBtnDownPix1;
    delete miniBtnPix1;
    delete miniBtnDownPix1;
    delete iMiniBtnPix1;
    delete iMiniBtnDownPix1;
    delete defaultMenuPix;
    delete btnForeground;
    pixmaps_created = false;
}


RedmondButton::RedmondButton(RedmondDeco *parent, const char *name,
      const unsigned char *bitmap, bool menuButton, bool isMini, int size, const QString& tip, const int realizeBtns)
    : QButton(parent->widget(), name)
{
	// Eliminate background flicker
	setBackgroundMode( NoBackground );
        setCursor( arrowCursor );

	menuBtn = menuButton;
	miniBtn = isMini;
	client  = parent;
	this->size = size;
	realizeButtons = realizeBtns;

	// Use larger button for the menu, or mini-buttons for toolwindows.
	if ( isMini || menuButton ) {
		setFixedSize(size, size);
		resize(size, size);
	} else {
		setFixedSize(size, size-2);
		resize(size, size-2);
	}

	if ( bitmap ) {
		setBitmap(bitmap);
	}

	QToolTip::add(this, tip);
}


QSize RedmondButton::sizeHint() const
{
	if ( miniBtn || menuBtn )
		return( QSize(size, size) );
	else
		return( QSize(size, size-2) );
}


void RedmondButton::reset()
{
	repaint(false);
}


void RedmondButton::setBitmap(const unsigned char *bitmap)
{
	pix.resize(0, 0);
	deco = QBitmap(10, 10, bitmap, true);
	deco.setMask(deco);
	repaint( false );
}


void RedmondButton::setPixmap( const QPixmap &p )
{
	deco.resize(0, 0);
	pix = p;

	if (miniBtn || menuBtn)
		setMask(QRect(0, 0, size, size));
	else
		setMask(QRect(0, 0, size, size-2));

	repaint(false);
}


void RedmondButton::mousePressEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me(e->type(), e->pos(), e->globalPos(),
	               (e->button()&realizeButtons)?LeftButton:NoButton, e->state());
	QButton::mousePressEvent( &me );
}


void RedmondButton::mouseReleaseEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(),
					 (e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mouseReleaseEvent( &me );
}


void RedmondButton::drawButton(QPainter *p)
{
   if ( pix.isNull() ) {
	  if ( client->isActive() ) {
		 if ( isDown() )
			p->drawPixmap(0, 0, miniBtn ? *miniBtnDownPix1 : *btnDownPix1);
		 else
			p->drawPixmap(0, 0, miniBtn ? *miniBtnPix1 : *btnPix1);
	  } else {
		 if ( isDown() )
			p->drawPixmap(0, 0, miniBtn ? *iMiniBtnDownPix1 : *iBtnDownPix1);
		 else
			p->drawPixmap(0, 0, miniBtn ? *iMiniBtnPix1 : *iBtnPix1);
	  }

	  p->setPen( *btnForeground );
	  int xOff = (width()-10)/2;
	  int yOff = (height()-10)/2;
	  p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, deco);
   } else {
	  p->fillRect(0, 0, width(), height(),
			options()->color(KDecoration::ColorTitleBar, client->isActive()));

	  if ( menuBtn && size < 16) {
		 QPixmap tmpPix;

		 // Smooth scale the menu button pixmap
		 tmpPix.convertFromImage(
		 pix.convertToImage().smoothScale(size, size));

		 p->drawPixmap( 0, 0, tmpPix );
	  } else {
		 int xOff = (width() -pix.width() )/2;
		 int yOff = (height()-pix.height())/2;
		 p->drawPixmap(xOff, yOff, pix );
	  }
   }
}


RedmondDeco::RedmondDeco(KDecorationBridge *b, KDecorationFactory *f)
    : KDecoration(b, f)
{
}

void RedmondDeco::init()
{
	createMainWidget(WResizeNoErase);
	widget()->installEventFilter(this);

	widget()->setBackgroundMode(NoBackground);
//	bool reverse = QApplication::reverseLayout();

//	Finally, toolwindows look small
//	if ( isTool() ) {
//		titleHeight = toolTitleHeight+2;
//		smallButtons = true;
//	} else {
		titleHeight = normalTitleHeight+2;
		smallButtons = false;
//	}

	lastButtonWidth = 0;

	QGridLayout* g = new QGridLayout(widget(), 0, 0, 0);
	g->setResizeMode(QLayout::FreeResize);
	if (isPreview()) {
		g->addWidget(new QLabel(i18n("<center><b>Redmond preview</b></center>"), widget()), 3, 1);
	} else {
		g->addItem(new QSpacerItem( 0, 0 ), 3, 1); // no widget in the middle
	}

	g->addRowSpacing(0, borderWidth);       // Top grab bar
	// without the next line, unshade flickers
	g->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
	g->setRowStretch(3, 10);      // Wrapped window
	g->addRowSpacing(4, borderWidth);       // bottom handles
	g->addRowSpacing(2, 1);       // Line below title bar
	g->addColSpacing(0, borderWidth);
	g->addColSpacing(2, borderWidth);

	button[BtnMenu] = new RedmondButton(this, "menu", NULL, true, smallButtons, titleHeight-2, i18n("Menu"), LeftButton|RightButton);
	button[BtnClose] = new RedmondButton(this, "close", close_bits, false, smallButtons, titleHeight-2, i18n("Close"));
	button[BtnMin] = new RedmondButton(this, "iconify", iconify_bits, false, smallButtons, titleHeight-2, i18n("Minimize"));
	button[BtnMax] = new RedmondButton(this, "maximize", maximize_bits, false, smallButtons, titleHeight-2, i18n("Maximize"), LeftButton|MidButton|RightButton);

	// Connect required stuff together
	connect(button[BtnMenu], SIGNAL(pressed()), this, SLOT(menuButtonPressed()));
	connect(button[BtnClose], SIGNAL(clicked()), this, SLOT(closeWindow()));
	connect(button[BtnMin], SIGNAL(clicked()), this, SLOT(minimize()));
	connect(button[BtnMax], SIGNAL(clicked()), this, SLOT(slotMaximize()));

	// Pack the titleBar hbox with items
	hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0);
	hb->setResizeMode(QLayout::FreeResize);
	hb->addSpacing(2);
	hb->addWidget(button[BtnMenu]);
	titlebar = new QSpacerItem(10, titleHeight, QSizePolicy::Expanding, QSizePolicy::Minimum);
	hb->addItem(titlebar);
	hb->addSpacing(borderWidth/2);

	if ( providesContextHelp() ) {
		button[BtnHelp] = new RedmondButton(this, "help", question_bits, false, smallButtons, titleHeight-2, i18n("Help"));
		connect( button[BtnHelp], SIGNAL( clicked() ), this, SLOT( showContextHelp() ));
		hb->addWidget( button[BtnHelp] );
	} else {
		button[BtnHelp] = NULL;
	}

	hb->addWidget(button[BtnMin]);
	hb->addWidget(button[BtnMax]);
	hb->addSpacing(borderWidth/2);
	hb->addWidget(button[BtnClose]);
	hb->addSpacing(2);

	g->addLayout(hb, 1, 1);

	// Hide buttons which are not required
	// We can un-hide them if required later
	if (!isMinimizable())
		button[BtnMin]->hide();
	if (!isMaximizable())
		button[BtnMax]->hide();
	if (!isCloseable())
		button[BtnClose]->hide();

	hiddenItems = false;

	// Make sure that the menu button uses the correct mini-icon
	iconChange();
	widget()->layout()->activate();
}


void RedmondDeco::slotReset()
{
    // 0 to 3   ( 4 buttons - Help, Max, Iconify, Close )
    for(int i = RedmondDeco::BtnHelp; i <= RedmondDeco::BtnClose; i++)
        if (button[i])
            button[i]->reset();

    // The menu is reset by iconChange()

    widget()->repaint( false );
}


void RedmondDeco::iconChange()
{
	QPixmap miniIcon = icon().pixmap(QIconSet::Small, QIconSet::Normal);

	if (!miniIcon.isNull())
		button[BtnMenu]->setPixmap(miniIcon);
	else
		button[BtnMenu]->setPixmap(*defaultMenuPix);

	if (button[BtnMenu]->isVisible())
		button[BtnMenu]->repaint(false);
}


void RedmondDeco::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
       maximize( maximizeMode() ^ MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
       maximize( maximizeMode() ^ MaximizeHorizontal );
    else
       maximize( maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull );
}


void RedmondDeco::resizeEvent(QResizeEvent *)
{
    calcHiddenButtons();
/*
    if (isVisibleToTLW()) {
        update(rect());
        int dx = 0;
        int dy = 0;

	    if ( e->oldSize().width() != width() )
	       dx = 32 + QABS( e->oldSize().width() -  width() );

	    if ( e->oldSize().height() != height() )
	       dy = 8 + QABS( e->oldSize().height() -  height() );

	    if ( dy )
	       update( 0, height() - dy + 1, width(), dy );

        if ( dx )
        {
	       update( width() - dx + 1, 0, dx, height() );
	       update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	       update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
           // Titlebar needs no paint event
	       QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
	    }
    }
 */
}


void RedmondDeco::captionChange( const QString& )
{
    widget()->repaint( titlebar->geometry(), false );
}


void RedmondDeco::paintEvent( QPaintEvent* )
{
    bool hicolor = QPixmap::defaultDepth() > 8;
    int fontoffset = 1;

    QPainter p(widget());

    // Obtain widget bounds.
    QRect r(widget()->rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width()-1;
    int y2 = r.height()-1;
    int w  = r.width();
    int h  = r.height();

    // Draw part of the frame that is the frame color
    // ==============================================
    QColorGroup g = options()->colorGroup(KDecoration::ColorFrame, isActive());
    p.setPen( g.background() );
    p.drawLine( x, y, x2-1, y );
    p.drawLine( x, y, x, y2-1 );

    // Draw line under title bar
    p.drawLine( x+borderWidth, y+titleHeight+borderWidth, x2-borderWidth, y+titleHeight+borderWidth );
    // Draw a hidden line that appears during shading
    p.drawLine( x+borderWidth, y2-borderWidth, x2-borderWidth, y2-borderWidth );

    // Fill out the border edges
    for (int i = 1; i < borderWidth; i++)
        p.drawRect( x+i, y+i, w-2*i, h-2*i );

    // Draw highlights and lowlights
    p.setPen(g.light());
    for (int i = 1; i <= borderWidth/3; i++) {
        p.drawLine( x+i, y+i, x2-i-1, y+i);
        p.drawLine( x+i, y+i, x+i, y2-i-1);
    }

    p.setPen(g.mid().dark(135));
    for (int i = 1; i <= borderWidth/3; i++) {
        p.drawLine( x2-i, y+i+1, x2-i, y2-i);
        p.drawLine( x+i+1, y2-i, x2-i, y2-i);
    }

    // Draw black edges
    p.setPen( g.dark().dark(155) );
    p.drawLine(x2, y, x2, y2);
    p.drawLine(x, y2, x2, y2);

    // Draw the title bar.
    // ===================
    r = titlebar->geometry();
    QFontMetrics fm(options()->font(true));

    // Obtain blend colours.
    QColor c1 = options()->color(KDecoration::ColorTitleBar, isActive() );
    QColor c2 = options()->color(KDecoration::ColorTitleBlend, isActive() );

    // Paint without a buffer if the colours are the same to
    // improve performance, and only draw gradients on hicolor displays.
    if ((c1 != c2) && hicolor) {
        // KS - Add gradient caching if needed at a later stage.

        // Create a disposable pixmap buffer for the title blend
        KPixmap* titleBuffer = new KPixmap;
        titleBuffer->resize(w-2*borderWidth, titleHeight);

        if (titleBuffer->depth() > 16) {
            KPixmapEffect::gradient(*titleBuffer, c1, c2,
                                    KPixmapEffect::HorizontalGradient);
        } else {
            // This enables dithering on 15 and 16bit displays, preventing
            // some pretty horrible banding effects
            QImage image = KImageEffect::gradient(titleBuffer->size(), c1, c2,
                                    KImageEffect::HorizontalGradient);

            titleBuffer->convertFromImage(image, Qt::OrderedDither);
        }

        QPainter p2( titleBuffer, this );

        // Since drawing the gradient is (relatively) slow, it is best
        // to draw the title text on the pixmap.

        // Reduce the font size and weight for toolwindows.
        QFont fnt = options()->font(true);
        if ( smallButtons ) {
           fnt.setPointSize( fnt.pointSize() - 2 ); // Shrink font by 2 pt.
           fnt.setWeight( QFont::Normal );
           fontoffset = 0;
        }
        p2.setFont( fnt );
        p2.setPen( options()->color(KDecoration::ColorFont, isActive() ));
        p2.drawText( r.x(), fontoffset, r.width()-3, r.height()-1,
                     AlignLeft | AlignVCenter, caption() );
        p2.end();

        p.drawPixmap( borderWidth, borderWidth, *titleBuffer );

        delete titleBuffer;

    } else {
       // Assume lower ended hardware, so don't use buffers.
       // Don't draw a gradient either.
       p.fillRect( borderWidth, borderWidth, w-2*borderWidth, titleHeight, c1 );

       // Draw the title text.
       QFont fnt = options()->font(true);
       if ( smallButtons )
       {
          fnt.setPointSize( fnt.pointSize() - 2 ); // Shrink font by 2 pt.
          fnt.setWeight( QFont::Normal );
          fontoffset = 0;
       }
       p.setFont( fnt );
       p.setPen(options()->color(KDecoration::ColorFont, isActive() ));
       p.drawText(r.x()+4, r.y()+fontoffset, r.width()-3, r.height()-1,
                  AlignLeft | AlignVCenter, caption() );
   }

}

void RedmondDeco::showEvent(QShowEvent *)
{
    calcHiddenButtons();
    widget()->show();
}


void RedmondDeco::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
	   titlebarDblClickOperation();
}


void RedmondDeco::maximizeChange(bool m)
{
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
}

void RedmondDeco::calcHiddenButtons()
{
   // order of hiding is help, maximize, minimize, close, then menu;
   int minWidth = (2 + 4 + (providesContextHelp() ? 2 : 1 )) * normalTitleHeight;

   if (lastButtonWidth > width()) { // Shrinking
	  lastButtonWidth = width();
	  if (width() < minWidth) {
		 hiddenItems = true;

		 for(int i = RedmondDeco::BtnHelp; i <= RedmondDeco::BtnMenu; i++) {
			if (button[i]) {
			   if ( !button[i]->isHidden() ) {
				  button[i]->hide();
			   }
			   minWidth -= button[i]->sizeHint().width();
			   if (width() >= minWidth) {
				  return;
			   }
			}
		 }
	  }
   } else {
	  if ( hiddenItems ) { // Expanding
		 lastButtonWidth = width();
		 int totalSize = normalTitleHeight*3;

		 for (int i = RedmondDeco::BtnMenu; i >= RedmondDeco::BtnHelp; i--) {
			if (button[i]) {
			   if (button[i]->sizeHint().width() + totalSize <= width()) {
				  totalSize += button[i]->sizeHint().width();
				  button[i]->resize(button[i]->sizeHint());
				  button[i]->show();
			   } else {
				  return;
			   }
			}
		 }

		 // all items shown now
		 hiddenItems = false;
	  } else {
		 lastButtonWidth = width();
	  }
   }
}

RedmondDeco::Position RedmondDeco::mousePosition(const QPoint &p) const
{
	Position m = PositionCenter;

    const int range = 14 + 3*borderWidth/2;

    if ( ( p.x() > borderWidth && p.x() < width() - borderWidth )
         && ( p.y() > borderWidth && p.y() < height() - borderWidth ) )
        m = PositionCenter;
    else if ( p.y() <= range && p.x() <= range)
        m = PositionTopLeft;
    else if ( p.y() >= height()-range && p.x() >= width()-range)
        m = PositionBottomRight;
    else if ( p.y() >= height()-range && p.x() <= range)
        m = PositionBottomLeft;
    else if ( p.y() <= range && p.x() >= width()-range)
        m = PositionTopRight;
    else if ( p.y() <= borderWidth )
        m = PositionTop;
    else if ( p.y() >= height()-borderWidth )
        m = PositionBottom;
    else if ( p.x() <= borderWidth )
        m = PositionLeft;
    else if ( p.x() >= width()-borderWidth )
        m = PositionRight;
    else
        m = PositionCenter;

	return m;
}

void RedmondDeco::borders(int &l, int &r, int &t, int &b) const
{
//	bool reverse = QApplication::reverseLayout();
	l = borderWidth;
	r = borderWidth;
	t = borderWidth + titlebar->geometry().height() + 1;
	b = borderWidth;
}

void RedmondDeco::resize(const QSize &s)
{
	widget()->resize(s);
}

QSize RedmondDeco::minimumSize() const
{
	return QSize(50, 50); // what's good for the goose....
}

void RedmondDeco::activeChange()
{
    QPixmap miniIcon = icon().pixmap(QIconSet::Small, QIconSet::Normal);
    if (!miniIcon.isNull()) {
        button[BtnMenu]->setPixmap(miniIcon);
    } else {
        button[BtnMenu]->setPixmap(kdelogo);
	}

    // Reset the menu button ?
	for (int i = BtnHelp; i < BtnCount; i++) {
		if (button[i]) button[i]->reset();
	}

    widget()->repaint(false);
}

void RedmondDeco::captionChange()
{
    widget()->repaint(titlebar->geometry(), false);
}

void RedmondDeco::maximizeChange()
{
	bool m = (maximizeMode() == MaximizeFull);
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
	QToolTip::remove(button[BtnMax]);
	QToolTip::add(button[BtnMax], m ? i18n("Restore") : i18n("Maximize"));
}

void RedmondDeco::desktopChange()
{
}

void RedmondDeco::shadeChange()
{
}

void RedmondDeco::menuButtonPressed()
{
    static QTime* t = NULL;
    static RedmondDeco* lastClient = NULL;
    if (t == NULL) t = new QTime;
    bool dbl = ( lastClient == this && t->elapsed() <= QApplication::doubleClickInterval());
    lastClient = this;
    t->start();
    if (!dbl) {
		QPoint menupoint(button[BtnMenu]->rect().bottomLeft().x()-3,
		                 button[BtnMenu]->rect().bottomLeft().y()+4);
                KDecorationFactory* f = factory();
		showWindowMenu(button[BtnMenu]->mapToGlobal(menupoint));
                if( !f->exists( this )) // 'this' was destroyed
                    return;
		button[BtnMenu]->setDown(false);
    } else {
		closeWindow();
	}
}

bool RedmondDeco::eventFilter(QObject *o, QEvent *e)
{
	if (o != widget()) { return false; }
	switch (e->type()) {
		case QEvent::Resize: {
			resizeEvent(static_cast<QResizeEvent *>(e));
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
		case QEvent::MouseButtonDblClick: {
			mouseDoubleClickEvent(static_cast<QMouseEvent *>(e));
			return true;
		}
		case QEvent::MouseButtonPress: {
			processMousePressEvent(static_cast<QMouseEvent *>(e));
			return true;
		}
		default: {
			break;
		}
	}

	return false;
}

void RedmondDecoFactory::readConfig() {
	normalTitleHeight = QFontMetrics(options()->font(true)).height();
	toolTitleHeight = QFontMetrics(options()->font(true, true)).height();
	switch(options()->preferredBorderSize(this)) {
	case BorderLarge:
		borderWidth = 8;
		if (normalTitleHeight < 20) normalTitleHeight = 20;
		if (toolTitleHeight < 20) toolTitleHeight = 20;
		break;
	case BorderVeryLarge:
		borderWidth = 12;
		if (normalTitleHeight < 24) normalTitleHeight = 24;
		if (toolTitleHeight < 24) toolTitleHeight = 24;
		break;
	case BorderHuge:
		borderWidth = 18;
		if (normalTitleHeight < 28) normalTitleHeight = 28;
		if (toolTitleHeight < 28) toolTitleHeight = 28;
		break;
	case BorderVeryHuge:
		borderWidth = 27;
		if (normalTitleHeight < 33) normalTitleHeight = 33;
		if (toolTitleHeight < 33) toolTitleHeight = 33;
		break;
	case BorderOversized:
		borderWidth = 40;
		if (normalTitleHeight < 40) normalTitleHeight = 40;
		if (toolTitleHeight < 40) toolTitleHeight = 40;
		break;
	case BorderTiny:
	case BorderNormal:
	default:
		borderWidth = 4;
		if (normalTitleHeight < 16) normalTitleHeight = 16;
		if (toolTitleHeight < 16) toolTitleHeight = 16;
	}
}

RedmondDecoFactory::RedmondDecoFactory()
{
	readConfig();
	create_pixmaps();
}

RedmondDecoFactory::~RedmondDecoFactory()
{
	Redmond::delete_pixmaps();
}

KDecoration *RedmondDecoFactory::createDecoration( KDecorationBridge *b )
{
	return new RedmondDeco(b, this);
}

bool RedmondDecoFactory::reset( unsigned long changed )
{
	if ( changed & ( SettingFont | SettingBorder | SettingColors ) ) {
		delete_pixmaps();
		readConfig();
		create_pixmaps();
		resetDecorations(changed);
		return true;
	} else {
		resetDecorations(changed);
		return false;
	}
}

QValueList< RedmondDecoFactory::BorderSize > RedmondDecoFactory::borderSizes() const
{ // the list must be sorted
  return QValueList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

}

extern "C" KDecorationFactory *create_factory()
{
	return new Redmond::RedmondDecoFactory();
}


#include "redmond.moc"
// vim: ts=4
