/*
 *	$Id$
 *
 *	KDE2 Default KWin client
 *
 *	Copyright (C) 1999, 2001 Daniel Duley <mosfet@kde.org>
 *	Matthias Ettrich <ettrich@kde.org>
 *	Karol Szwed <gallium@kde.org>
 *
 *	Draws mini titlebars for tool windows.
 *	Many features are now customizable.
 */

#include <kconfig.h>
#include <kglobal.h>
#include <kpixmapeffect.h>
#include <kimageeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <qlayout.h>
#include <qdrawutil.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"
#include "kdedefault.h"
#include <qimage.h>

using namespace KWinInternal;

namespace Default
{

static unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x78, 0x00,
  0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char close_bits[] = {
  0x00, 0x00, 0x84, 0x00, 0xce, 0x01, 0xfc, 0x00, 0x78, 0x00, 0x78, 0x00,
  0xfc, 0x00, 0xce, 0x01, 0x84, 0x00, 0x00, 0x00};

static unsigned char maximize_bits[] = {
  0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x86, 0x01, 0x86, 0x01, 0x86, 0x01,
  0x86, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00};

static unsigned char minmax_bits[] = {
  0x7f, 0x00, 0x7f, 0x00, 0x63, 0x00, 0xfb, 0x03, 0xfb, 0x03, 0x1f, 0x03,
  0x1f, 0x03, 0x18, 0x03, 0xf8, 0x03, 0xf8, 0x03};

static unsigned char question_bits[] = {
  0x00, 0x00, 0x78, 0x00, 0xcc, 0x00, 0xc0, 0x00, 0x60, 0x00, 0x30, 0x00,
  0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00};

static unsigned char pindown_white_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x1f, 0xa0, 0x03,
  0xb0, 0x01, 0x30, 0x01, 0xf0, 0x00, 0x70, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pindown_gray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c,
  0x00, 0x0e, 0x00, 0x06, 0x00, 0x00, 0x80, 0x07, 0xc0, 0x03, 0xe0, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pindown_dgray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x10, 0x70, 0x20, 0x50, 0x20,
  0x48, 0x30, 0xc8, 0x38, 0x08, 0x1f, 0x08, 0x18, 0x10, 0x1c, 0x10, 0x0e,
  0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pindown_mask_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x1f, 0xf0, 0x3f, 0xf0, 0x3f,
  0xf8, 0x3f, 0xf8, 0x3f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf0, 0x1f, 0xf0, 0x0f,
  0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_white_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x11,
  0x3f, 0x15, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_gray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x0a, 0xbf, 0x0a, 0x80, 0x15, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_dgray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0x40, 0x31, 0x40, 0x2e,
  0x40, 0x20, 0x40, 0x20, 0x7f, 0x2a, 0x40, 0x3f, 0xc0, 0x31, 0xc0, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_mask_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0xc0, 0x31, 0xc0, 0x3f,
  0xff, 0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xc0, 0x3f, 0xc0, 0x31, 0xc0, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ===========================================================================

QPixmap* titlePix 			= NULL;
KPixmap* titleBuffer 		= NULL;
KPixmap* aUpperGradient 	= NULL;
KPixmap* iUpperGradient 	= NULL;

KPixmap* pinDownPix			= NULL;
KPixmap* pinUpPix			= NULL;
KPixmap* ipinDownPix		= NULL;
KPixmap* ipinUpPix			= NULL;

KPixmap* rightBtnUpPix		= NULL;
KPixmap* rightBtnDownPix	= NULL;
KPixmap* irightBtnUpPix		= NULL;
KPixmap* irightBtnDownPix	= NULL;

KPixmap* leftBtnUpPix		= NULL;
KPixmap* leftBtnDownPix		= NULL;
KPixmap* ileftBtnUpPix		= NULL;
KPixmap* ileftBtnDownPix	= NULL;

KDEDefaultHandler* clientHandler;
bool KDEDefault_initialized = false;
bool useGradients;
bool showGrabBar;
bool showTitleBarStipple;
bool largeToolButtons;
int	 toolTitleHeight;
int	 normalTitleHeight;


// ===========================================================================

KDEDefaultHandler::KDEDefaultHandler()
{
	readConfig();
	createPixmaps();
	KDEDefault_initialized = true;
}


KDEDefaultHandler::~KDEDefaultHandler()
{
	KDEDefault_initialized = false;
	freePixmaps();
}


void KDEDefaultHandler::reset()
{
	KDEDefault_initialized = false;
	freePixmaps();
	readConfig();
	createPixmaps();
	KDEDefault_initialized = true;
	Workspace::self()->slotResetAllClientsDelayed();
}


void KDEDefaultHandler::readConfig()
{
	KConfig* conf = KGlobal::config();
	conf->setGroup("KDEDefault");

	showGrabBar 		= conf->readBoolEntry("ShowGrabBar", true);
	showTitleBarStipple = conf->readBoolEntry("ShowTitleBarStipple", true);
	useGradients 		= conf->readBoolEntry("UseGradients", true);
	int size 			= conf->readNumEntry("TitleBarSize", 0);

	if (size < 0) size = 0;
	if (size > 2) size = 2;

	normalTitleHeight 	= 16 + (4*size);
	toolTitleHeight 	= normalTitleHeight - 4;
	largeToolButtons 	= (toolTitleHeight >= 16) ? true : false;
}


// This paints the button pixmaps upon loading the style.
void KDEDefaultHandler::createPixmaps()
{
	bool highcolor = useGradients && (QPixmap::defaultDepth() > 8);

	// Make the titlebar stipple optional
	if (showTitleBarStipple)
	{
		QPainter p;
		QPainter maskPainter;
		int i, x, y;
		titlePix = new QPixmap(132, normalTitleHeight+2);
		QBitmap mask(132, normalTitleHeight+2);
		mask.fill(Qt::color0);

		p.begin(titlePix);
		maskPainter.begin(&mask);
		maskPainter.setPen(Qt::color1);
		for(i=0, y=2; i < 6; ++i, y+=4)
			for(x=1; x <= 132; x+=3)
			{
				p.setPen(options->color(Options::TitleBar, true).light(150));
				p.drawPoint(x, y);
				maskPainter.drawPoint(x, y);
				p.setPen(options->color(Options::TitleBar, true).dark(150));
				p.drawPoint(x+1, y+1);
				maskPainter.drawPoint(x+1, y+1);
			}
		maskPainter.end();
		p.end();
		titlePix->setMask(mask);
	} else
		titlePix = NULL;

	QColor activeTitleColor1(options->color(Options::TitleBar,      true));
	QColor activeTitleColor2(options->color(Options::TitleBlend,    true));

	QColor inactiveTitleColor1(options->color(Options::TitleBar,    false));
	QColor inactiveTitleColor2(options->color(Options::TitleBlend,  false));

	// Create titlebar gradient images if required
	aUpperGradient = NULL;
	iUpperGradient = NULL;

	if(highcolor)
	{
		// Create the titlebar gradients
		if (activeTitleColor1 != activeTitleColor2)
		{
			aUpperGradient = new KPixmap;
			aUpperGradient->resize(128, normalTitleHeight+2);
			KPixmapEffect::gradient(*aUpperGradient,
				activeTitleColor1,
				activeTitleColor2,
				KPixmapEffect::VerticalGradient);
		}

		if (inactiveTitleColor1 != inactiveTitleColor2)
		{
			iUpperGradient = new KPixmap;
			iUpperGradient->resize(128, normalTitleHeight+2);

			KPixmapEffect::gradient(*iUpperGradient,
					inactiveTitleColor1,
					inactiveTitleColor2,
			KPixmapEffect::VerticalGradient);
		}
	}

	// Set the sticky pin pixmaps;
	QColorGroup g;
	QPainter p;

	// Active pins
	g = options->colorGroup( Options::ButtonBg, true );
	pinUpPix = new KPixmap();
	pinUpPix->resize(16, 16);
	p.begin( pinUpPix );
	kColorBitmaps( &p, g, 0, 0, 16, 16, true, pinup_white_bits,
		pinup_gray_bits, NULL, NULL, pinup_dgray_bits, NULL );
	p.end();
	pinUpPix->setMask( QBitmap(16, 16, pinup_mask_bits, true) );

	pinDownPix = new KPixmap();
	pinDownPix->resize(16, 16);
	p.begin( pinDownPix );
	kColorBitmaps( &p, g, 0, 0, 16, 16, true, pindown_white_bits,
		pindown_gray_bits, NULL, NULL, pindown_dgray_bits, NULL );
	p.end();
	pinDownPix->setMask( QBitmap(16, 16, pindown_mask_bits, true) );

	// Inactive pins
	g = options->colorGroup( Options::ButtonBg, false );
	ipinUpPix = new KPixmap();
	ipinUpPix->resize(16, 16);
	p.begin( ipinUpPix );
	kColorBitmaps( &p, g, 0, 0, 16, 16, true, pinup_white_bits,
		pinup_gray_bits, NULL, NULL, pinup_dgray_bits, NULL );
	p.end();
	ipinUpPix->setMask( QBitmap(16, 16, pinup_mask_bits, true) );

	ipinDownPix = new KPixmap();
	ipinDownPix->resize(16, 16);
	p.begin( ipinDownPix );
	kColorBitmaps( &p, g, 0, 0, 16, 16, true, pindown_white_bits,
		pindown_gray_bits, NULL, NULL, pindown_dgray_bits, NULL );
	p.end();
	ipinDownPix->setMask( QBitmap(16, 16, pindown_mask_bits, true) );

	// Create a title buffer for flicker-free painting
	titleBuffer = new KPixmap();

	// Cache all possible button states
	leftBtnUpPix 	= new KPixmap();
	leftBtnUpPix->resize(16, 16);
	leftBtnDownPix	= new KPixmap();
	leftBtnDownPix->resize(16, 16);
	ileftBtnUpPix	= new KPixmap();
	ileftBtnUpPix->resize(16, 16);
	ileftBtnDownPix	= new KPixmap();
	ileftBtnDownPix->resize(16, 16);

	rightBtnUpPix 	= new KPixmap();
	rightBtnUpPix->resize(16, 16);
	rightBtnDownPix = new KPixmap();
	rightBtnDownPix->resize(16, 16);
	irightBtnUpPix 	= new KPixmap();
	irightBtnUpPix->resize(16, 16);
	irightBtnDownPix = new KPixmap();
	irightBtnDownPix->resize(16, 16);

	// Draw the button state pixmaps
	g = options->colorGroup( Options::TitleBar, true );
	drawButtonBackground( leftBtnUpPix, g, false );
	drawButtonBackground( leftBtnDownPix, g, true );

	g = options->colorGroup( Options::ButtonBg, true );
	drawButtonBackground( rightBtnUpPix, g, false );
	drawButtonBackground( rightBtnDownPix, g, true );

	g = options->colorGroup( Options::TitleBar, false );
	drawButtonBackground( ileftBtnUpPix, g, false );
	drawButtonBackground( ileftBtnDownPix, g, true );

	g = options->colorGroup( Options::ButtonBg, false );
	drawButtonBackground( irightBtnUpPix, g, false );
	drawButtonBackground( irightBtnDownPix, g, true );
}


void KDEDefaultHandler::freePixmaps()
{
	// Free button pixmaps
	if (rightBtnUpPix)
		delete rightBtnUpPix;
	if(rightBtnDownPix)
		delete rightBtnDownPix;
	if (irightBtnUpPix)
		delete irightBtnUpPix;
	if (irightBtnDownPix)
		delete irightBtnDownPix;

	if (leftBtnUpPix)
		delete leftBtnUpPix;
	if(leftBtnDownPix)
		delete leftBtnDownPix;
	if (ileftBtnUpPix)
		delete ileftBtnUpPix;
	if (ileftBtnDownPix)
		delete ileftBtnDownPix;

	// Title images
	if (titleBuffer)
		delete titleBuffer;
	if (titlePix)
		delete titlePix;
	if (aUpperGradient)
		delete aUpperGradient;
	if (iUpperGradient)
		delete iUpperGradient;

	// Sticky pin images
	if (pinUpPix)
		delete pinUpPix;
	if (ipinUpPix)
		delete ipinUpPix;
	if (pinDownPix)
		delete pinDownPix;
	if (ipinDownPix)
		delete ipinDownPix;
}


void KDEDefaultHandler::drawButtonBackground(KPixmap *pix,
		const QColorGroup &g, bool sunken)
{
    QPainter p;
    int w = pix->width();
    int h = pix->height();
    int x2 = w-1;
    int y2 = h-1;

	bool highcolor = useGradients && (QPixmap::defaultDepth() > 8);
	QColor c = g.background();

	// Fill the background with a gradient if possible
	if (highcolor)
		KPixmapEffect::gradient(*pix, c.light(130), c.dark(130),
								KPixmapEffect::VerticalGradient);
	else
		pix->fill(c);

    p.begin(pix);
    // outer frame
    p.setPen(g.mid());
    p.drawLine(0, 0, x2, 0);
    p.drawLine(0, 0, 0, y2);
    p.setPen(g.light());
    p.drawLine(x2, 0, x2, y2);
    p.drawLine(0, x2, y2, x2);
    p.setPen(g.dark());
    p.drawRect(1, 1, w-2, h-2);
    p.setPen(sunken ? g.mid() : g.light());
    p.drawLine(2, 2, x2-2, 2);
    p.drawLine(2, 2, 2, y2-2);
    p.setPen(sunken ? g.light() : g.mid());
    p.drawLine(x2-2, 2, x2-2, y2-2);
    p.drawLine(2, x2-2, y2-2, x2-2);
}


// ===========================================================================

KDEDefaultButton::KDEDefaultButton(Client *parent, const char *name,
           bool largeButton, bool isLeftButton, bool isStickyButton,
           const unsigned char *bitmap, const QString& tip )
    : KWinButton(parent, name, tip)
{
    setBackgroundMode( QWidget::NoBackground );
	setToggleButton( isStickyButton );

	isMouseOver = false;
	deco 		= NULL;
    large 		= largeButton;
	isLeft 		= isLeftButton;
	isSticky 	= isStickyButton;
	client 		= parent;

    if (large)
       setFixedSize(16, 16);
    else
       setFixedSize(12, 12);

    if (bitmap)
        setBitmap(bitmap);
}


KDEDefaultButton::~KDEDefaultButton()
{
	if (deco)
		delete deco;
}


QSize KDEDefaultButton::sizeHint() const
{
   if ( large )
      return( QSize(16,16) );
   else
      return( QSize(12,12) );
}


void KDEDefaultButton::setBitmap(const unsigned char *bitmap)
{
	if (deco)
		delete deco;

    deco = new QBitmap(10, 10, bitmap, true);
    deco->setMask( *deco );
    repaint( false );
}


void KDEDefaultButton::drawButton(QPainter *p)
{
	if (!KDEDefault_initialized)
		return;

	if (deco) {
		// Fill the button background with an appropriate button image
 		KPixmap btnbg;

		if (isLeft)	{
			if (isDown())
				btnbg = client->isActive() ?
							*leftBtnDownPix : *ileftBtnDownPix;
			else
				btnbg = client->isActive() ?
							*leftBtnUpPix : *ileftBtnUpPix;
		} else {
			if (isDown())
				btnbg = client->isActive() ?
							*rightBtnDownPix : *irightBtnDownPix;
			else
				btnbg = client->isActive() ?
							*rightBtnUpPix : *irightBtnUpPix;
		}

		// Scale the background image if required
		// This is slow, but we assume this isn't done too often
		if ( !large ) {
			btnbg.detach();
			btnbg.convertFromImage(btnbg.convertToImage().smoothScale(12, 12));
		}

		p->drawPixmap( 0, 0, btnbg );

	} else if ( isLeft ) {

		// Fill the button background with an appropriate color/gradient
		// This is for sticky and menu buttons
		KPixmap* grad = client->isActive() ? aUpperGradient : iUpperGradient;
		if (!grad) {
			QColor c = options->color(Options::TitleBar, client->isActive());
			p->fillRect(0, 0, width(), height(), c );
		} else
		 	p->drawPixmap( 0, 0, *grad, 0,((normalTitleHeight-height())/2)+1,
						   16, 16 );

	} else {
		// Draw a plain background for menus or sticky buttons on RHS
   		QColor c = options->color(Options::Frame, client->isActive());
		p->fillRect(0, 0, width(), height(), c);
	}


	// If we have a decoration bitmap, then draw that
	// otherwise we paint a menu button (with mini icon), or a sticky button.
	if( deco ) {
		// Select the appropriate button decoration color
   		bool darkDeco = qGray( options->color(
				isLeft? Options::TitleBar : Options::ButtonBg,
				client->isActive()).rgb() ) > 127;

		if (isMouseOver)
			p->setPen( darkDeco ? Qt::darkGray : Qt::lightGray );
		else
			p->setPen( darkDeco ? Qt::black : Qt::white );

		int xOff = (width()-10)/2;
		int yOff = (height()-10)/2;
		p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, *deco);

	} else {
		KPixmap btnpix;

		if (isSticky) {
			if (client->isActive())
				btnpix = isOn() ? *pinDownPix : *pinUpPix;
			else
				btnpix = isOn() ? *ipinDownPix : *ipinUpPix;
		} else
			btnpix = client->miniIcon();

		// Intensify the image if required
		if (isMouseOver) {
                    btnpix = KPixmapEffect::intensity(btnpix, 0.8);
		}

		// Smooth scale the pixmap for small titlebars
		// This is slow, but we assume this isn't done too often
		if ( !large )
			btnpix.convertFromImage(btnpix.convertToImage().smoothScale(12, 12));

		p->drawPixmap( 0, 0, btnpix );
	}
}


// Make the protected member public
void KDEDefaultButton::turnOn( bool isOn )
{
	if ( isToggleButton() )
		setOn( isOn );
}


void KDEDefaultButton::enterEvent(QEvent *e)
{
	isMouseOver=true;
	repaint(false);
	KWinButton::enterEvent(e);
}


void KDEDefaultButton::leaveEvent(QEvent *e)
{
	isMouseOver=false;
	repaint(false);
	KWinButton::leaveEvent(e);
}


void KDEDefaultButton::mousePressEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					LeftButton, e->state() );
	KWinButton::mousePressEvent( &me );
}


void KDEDefaultButton::mouseReleaseEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					LeftButton, e->state() );
	KWinButton::mouseReleaseEvent( &me );
}


// ===========================================================================

KDEDefaultClient::KDEDefaultClient( Workspace *ws, WId w, QWidget *parent,
									const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WStaticContents |
								   WRepaintNoErase )
{
	// No flicker thanks
    setBackgroundMode( QWidget::NoBackground );

	// Set button pointers to NULL so we can track things
	for(int i=0; i < KDEDefaultClient::BtnCount; i++)
		button[i] = NULL;

    // Finally, toolWindows look small
    if ( isTool() ) {
		titleHeight  = toolTitleHeight;
		largeButtons = largeToolButtons;
	}
	else {
		titleHeight  = normalTitleHeight;
		largeButtons = true;
    }

    // Pack the windowWrapper() window within a grid
    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);       // Top grab bar
    g->addRowSpacing(2, 1);       // line under titlebar
    g->addWidget(windowWrapper(), 3, 1);

	// without the next line, unshade flickers
	g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed,
								 QSizePolicy::Expanding ) );
    g->setRowStretch(3, 10);      // Wrapped window

	// Determine the size of the lower grab bar
	if ( showGrabBar && (!isTool()) )
	    g->addRowSpacing(4, 8);   // bottom handles
	else
	    g->addRowSpacing(4, 4);   // bottom handles

    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    // Pack the titlebar HBox with items
    hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0 );
    hb->setResizeMode( QLayout::FreeResize );
    g->addLayout ( hb, 1, 1 );

	addClientButtons( options->titleButtonsLeft() );
    titlebar = new QSpacerItem( 10, titleHeight, QSizePolicy::Expanding,
								QSizePolicy::Minimum );
    hb->addItem(titlebar);
    hb->addSpacing(2);
	addClientButtons( options->titleButtonsRight(), false );
}


void KDEDefaultClient::addClientButtons( const QString& s, bool isLeft )
{
	if (s.length() > 0)
		for(unsigned int i = 0; i < s.length(); i++) {
		switch( s[i].latin1() )
		{
			// Menu button
			case 'M':
				if (!button[BtnMenu])
				{
   					button[BtnMenu] = new KDEDefaultButton(this, "menu",
							largeButtons, isLeft, false, NULL, i18n("Menu"));
   					connect( button[BtnMenu], SIGNAL(pressed()),
							this, SLOT(menuButtonPressed()) );
					hb->addWidget( button[BtnMenu] );
				}
				break;

			// Sticky button
			case 'S':
				if (!button[BtnSticky])
				{
   					button[BtnSticky] = new KDEDefaultButton(this, "sticky",
							largeButtons, isLeft, true, NULL, i18n("Sticky"));
					button[BtnSticky]->turnOn( isSticky() );
   					connect( button[BtnSticky], SIGNAL(clicked()),
							this, SLOT(toggleSticky()) );
					hb->addWidget( button[BtnSticky] );
				}
				break;

			// Help button
			case 'H':
   				if( providesContextHelp() && (!button[BtnHelp]) )
				{
					button[BtnHelp] = new KDEDefaultButton(this, "help",
							largeButtons, isLeft, true, question_bits,
							i18n("Help"));
					connect( button[BtnHelp], SIGNAL( clicked() ),
							this, SLOT( contextHelp() ));
					hb->addWidget( button[BtnHelp] );
				}
				break;

			// Minimize button
			case 'I':
				if ( (!button[BtnIconify]) && isMinimizable())
				{
				    button[BtnIconify] = new KDEDefaultButton(this, "iconify",
							largeButtons, isLeft, true, iconify_bits,
							i18n("Minimize"));
				    connect( button[BtnIconify], SIGNAL( clicked()),
							this, SLOT(iconify()) );
					hb->addWidget( button[BtnIconify] );
				}
				break;

			// Maximize button
			case 'A':
				if ( (!button[BtnMax]) && isMaximizable())
				{
				    button[BtnMax]  = new KDEDefaultButton(this, "maximize",
							largeButtons, isLeft, true, maximize_bits,
							i18n("Maximize"));
				    connect( button[BtnMax], SIGNAL( clicked()),
							this, SLOT(slotMaximize()) );
					hb->addWidget( button[BtnMax] );
				}
				break;

			// Close button
			case 'X':
				if (!button[BtnClose] && isCloseable())
				{
    				button[BtnClose] = new KDEDefaultButton(this, "close",
							largeButtons, isLeft, true, close_bits,
							i18n("Close"));
				    connect( button[BtnClose], SIGNAL( clicked()),
							this, SLOT(closeWindow()) );
					hb->addWidget( button[BtnClose] );
				}
				break;

			// Spacer item (only for non-tool windows)
			case '_':
    			if ( !isTool() )
	   				hb->addSpacing(2);
		}
	}
}


void KDEDefaultClient::iconChange()
{
	if (button[BtnMenu] && button[BtnMenu]->isVisible())
		button[BtnMenu]->repaint(false);
}

void KDEDefaultClient::stickyChange(bool on)
{
	if (button[BtnSticky]) {
		button[BtnSticky]->turnOn(on);
		button[BtnSticky]->repaint(false);
		button[BtnSticky]->setTipText(on ? i18n("Un-Sticky") : i18n("Sticky"));
	}
}

void KDEDefaultClient::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
	   maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
       maximize( MaximizeHorizontal );
    else
	   maximize();
}


void KDEDefaultClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
	doShape();
    calcHiddenButtons();

    if (isVisibleToTLW())
    {
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
	       update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() -
					QPoint(1,0) ) );
	       update( QRect( titlebar->geometry().topRight(), QPoint(width() - 4,
						  titlebar->geometry().bottom()) ) );
           // Titlebar needs no paint event
	       QApplication::postEvent( this, new QPaintEvent(titlebar->geometry(),
									 FALSE) );
        }
    }
}


void KDEDefaultClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


void KDEDefaultClient::paintEvent( QPaintEvent* )
{
	if (!KDEDefault_initialized)
		return;

	QColorGroup g;
	int offset;

	KPixmap* upperGradient = isActive() ? aUpperGradient : iUpperGradient;

    QPainter p(this);

    // Obtain widget bounds.
    QRect r(rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width() - 1;
    int y2 = r.height() - 1;
    int w  = r.width();
    int h  = r.height();

	// Determine where to place the extended left titlebar
	int leftFrameStart = (h > 42) ? y+titleHeight+26: y+titleHeight;

	// Determine where to make the titlebar color transition
    r = titlebar->geometry();
	int rightOffset = r.x()+r.width()+1;

    // Create a disposable pixmap buffer for the titlebar
	// very early before drawing begins so there is no lag
	// during painting pixels.
    titleBuffer->resize( rightOffset-3, titleHeight+1 );

	// Draw an outer black frame
	p.setPen(Qt::black);
	p.drawRect(x,y,w,h);

    // Draw part of the frame that is the titlebar color
	g = options->colorGroup(Options::TitleBar, isActive());
	p.setPen(g.light());
	p.drawLine(x+1, y+1, rightOffset-1, y+1);
	p.drawLine(x+1, y+1, x+1, leftFrameStart);

	// Draw titlebar colour separator line
	p.setPen(g.dark());
	p.drawLine(rightOffset-1, y+1, rightOffset-1, titleHeight+2);

	// Finish drawing the titlebar extension
	p.setPen(Qt::black);
	p.drawLine(x+1, leftFrameStart, x+2, leftFrameStart-1);
	p.setPen(g.mid());
	p.drawLine(x+2, y+titleHeight+3, x+2, leftFrameStart-2);

    // Fill out the border edges
    g = options->colorGroup(Options::Frame, isActive());
    p.setPen(g.light());
    p.drawLine(rightOffset, y+1, x2-1, y+1);
    p.drawLine(x+1, leftFrameStart+1, x+1, y2-1);
    p.setPen(g.dark());
    p.drawLine(x2-1, y+1, x2-1, y2-1);
    p.drawLine(x+1, y2-1, x2-1, y2-1);

	p.setPen(options->color(Options::Frame, isActive()));
	p.drawLine(x+2, leftFrameStart, x+2, y2-2 );
	p.drawLine(x2-2, y+titleHeight+3, x2-2, y2-2);

	// Draw the bottom handle if required
	if (showGrabBar && (!isTool()) )
	{
		if(w > 50)
		{
			qDrawShadePanel(&p, x+1, y2-6, 20, 6,
							g, false, 1, &g.brush(QColorGroup::Mid));
			qDrawShadePanel(&p, x+21, y2-6, w-42, 6,
							g, false, 1, isActive() ?
							&g.brush(QColorGroup::Background) :
							&g.brush(QColorGroup::Mid));
			qDrawShadePanel(&p, x2-20, y2-6, 20, 6,
							g, false, 1, &g.brush(QColorGroup::Mid));
		} else
			qDrawShadePanel(&p, x+1, y2-6, w-2, 6,
							g, false, 1, isActive() ?
							&g.brush(QColorGroup::Background) :
							&g.brush(QColorGroup::Mid));
		offset = 4;
	} else
		{
			p.drawLine(x+2, y2-2, x2-2, y2-2);
			offset = 0;
		}

    // Draw a frame around the wrapped widget.
    p.setPen( g.dark() );
    p.drawRect( x+3, y+titleHeight+3, w-6, h-titleHeight-offset-6 );

    // Draw the title bar.
	r = titlebar->geometry();

    // Obtain titlebar blend colours
    QColor c1 = options->color(Options::TitleBar, isActive() );
    QColor c2 = options->color(Options::Frame, isActive() );

	// Fill with frame color behind RHS buttons
	p.fillRect( rightOffset, y+2, x2-rightOffset-1, titleHeight+1, c2);

    QPainter p2( titleBuffer, this );

	// Draw the titlebar gradient
	if (upperGradient)
		p2.drawTiledPixmap(0, 0, rightOffset-3, titleHeight+1, *upperGradient);
	else
	    p2.fillRect(0, 0, rightOffset-3, titleHeight+1, c1);

    // Draw the title text on the pixmap, and with a smaller font
    // for toolwindows than the default.
    QFont fnt = options->font(true);

    if ( isTool() )
       fnt.setPointSize( fnt.pointSize()-2 );  // Shrink font by 2pt

    p2.setFont( fnt );

	// Draw the titlebar stipple if active and available
	if (isActive() && titlePix)
	{
		QFontMetrics fm(fnt);
		int captionWidth = fm.width(caption());
		if (caption().isRightToLeft())
			p2.drawTiledPixmap( r.x(), 0, r.width()-captionWidth-4,
								titleHeight+1, *titlePix );
		else
			p2.drawTiledPixmap( r.x()+captionWidth+3, 0, r.width()-captionWidth-4,
								titleHeight+1, *titlePix );
	}

    p2.setPen( options->color(Options::Font, isActive()) );
    p2.drawText(r.x(), 1, r.width()-1, r.height(),
		(caption().isRightToLeft() ? AlignRight : AlignLeft) | AlignVCenter,
		caption() );

	bitBlt( this, 2, 2, titleBuffer );

    p2.end();

	// Ensure a shaded window has no unpainted areas
	p.setPen(c2);
    p.drawLine(x+4, y+titleHeight+4, x2-4, y+titleHeight+4);
}


void KDEDefaultClient::doShape()
{
    QRegion mask(QRect(0, 0, width(), height()));
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-1, 0, 1, 1);
    mask -= QRect(0, height()-1, 1, 1);
    mask -= QRect(width()-1, height()-1, 1, 1);
    setMask(mask);
}


void KDEDefaultClient::showEvent(QShowEvent *ev)
{
    calcHiddenButtons();
	doShape();
    Client::showEvent(ev);
}


void KDEDefaultClient::mouseDoubleClickEvent( QMouseEvent * e )
{
	if (titlebar->geometry().contains( e->pos() ) )
		workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
}


void KDEDefaultClient::maximizeChange(bool m)
{
	if (button[BtnMax]) {
		button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
		button[BtnMax]->setTipText(m ? i18n("Restore") : i18n("Maximize"));
	}
}


void KDEDefaultClient::activeChange(bool)
{
	for(int i=KDEDefaultClient::BtnHelp; i < KDEDefaultClient::BtnCount; i++)
		if(button[i])
			button[i]->repaint(false);
	repaint(false);
}


// The hiding button while shrinking, show button while expanding magic
void KDEDefaultClient::calcHiddenButtons()
{
	// Hide buttons in this order:
	// Sticky, Help, Maximize, Minimize, Close, Menu.
	KDEDefaultButton* btnArray[] = { button[BtnSticky], button[BtnHelp],
			button[BtnMax], button[BtnIconify], button[BtnClose],
			button[BtnMenu] };

	int minwidth  = largeButtons ? 160 : 120; // Start hiding at this width
	int btn_width = largeButtons ? 16 : 12;
	int current_width = width();
	int count = 0;
	int i;

	// Find out how many buttons we need to hide.
	while (current_width < minwidth)
	{
		current_width += btn_width;
		count++;
	}

	// Bound the number of buttons to hide
	if (count > 6) count = 6;

	// Hide the required buttons...
	for(i = 0; i < count; i++)
	{
		if (btnArray[i] && btnArray[i]->isVisible() )
			btnArray[i]->hide();
	}

	// Show the rest of the buttons...
	for(i = count; i < 6; i++)
	{
		if (btnArray[i] && (!btnArray[i]->isVisible()) )
			btnArray[i]->show();
	}
}


Client::MousePosition KDEDefaultClient::mousePosition( const QPoint& p ) const
{
	MousePosition m = Nowhere;

	// Modify the mouse position if we are using a grab bar.
	if (showGrabBar && (!isTool()) )
		if (p.y() < (height() - 8))
			m = Client::mousePosition(p);
		else
		{
			if (p.x() >= (width() - 20))
				m = BottomRight;
			else if (p.x() <= 20)
				m = BottomLeft;
			else
			m = Bottom;
		}
	else
		m = Client::mousePosition(p);

	return m;
}


// Make sure the menu button follows double click conventions set in kcontrol
void KDEDefaultClient::menuButtonPressed()
{
	static QTime t;
	static KDEDefaultClient* lastClient = NULL;
	bool dbl = ( lastClient == this && t.elapsed() <= QApplication::doubleClickInterval());
	lastClient = this;
	t.start();

	if (dbl)
	{
		closeWindow();
		return;
	}

	QPoint menupoint ( button[BtnMenu]->rect().bottomLeft().x()-1,
					   button[BtnMenu]->rect().bottomLeft().y()+2 );
	workspace()->showWindowMenu( button[BtnMenu]->mapToGlobal( menupoint ), this );
	button[BtnMenu]->setDown(false);
}

};

// Extended KWin plugin interface
extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
        return(new Default::KDEDefaultClient(ws, w));
    }

	void init()
	{
		Default::clientHandler = new Default::KDEDefaultHandler();
	}

	void reset()
	{
		Default::clientHandler->reset();
	}

	void deinit()
	{
		delete Default::clientHandler;
	}
}

#include "kdedefault.moc"
// vim: ts=4
