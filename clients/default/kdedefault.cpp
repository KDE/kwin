/*
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

#include "kdedefault.h"

#include <kconfig.h>
#include <kglobal.h>
#include <kpixmapeffect.h>
#include <kimageeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <qlayout.h>
#include <qdrawutil.h>
#include <qbitmap.h>
#include <qimage.h>
#include <qtooltip.h>
#include <qapplication.h>
#include <qlabel.h>
#include <kdebug.h>

namespace Default
{

static const unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x78, 0x00,
  0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char close_bits[] = {
  0x00, 0x00, 0x84, 0x00, 0xce, 0x01, 0xfc, 0x00, 0x78, 0x00, 0x78, 0x00,
  0xfc, 0x00, 0xce, 0x01, 0x84, 0x00, 0x00, 0x00};

static const unsigned char maximize_bits[] = {
  0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x86, 0x01, 0x86, 0x01, 0x86, 0x01,
  0x86, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00};

static const unsigned char minmax_bits[] = {
  0x7f, 0x00, 0x7f, 0x00, 0x63, 0x00, 0xfb, 0x03, 0xfb, 0x03, 0x1f, 0x03,
  0x1f, 0x03, 0x18, 0x03, 0xf8, 0x03, 0xf8, 0x03};

static const unsigned char question_bits[] = {
  0x00, 0x00, 0x78, 0x00, 0xcc, 0x00, 0xc0, 0x00, 0x60, 0x00, 0x30, 0x00,
  0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00};

static const unsigned char above_on_bits[] = {
   0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x30, 0x00, 0xfc, 0x00, 0x78, 0x00,
   0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char above_off_bits[] = {
   0x30, 0x00, 0x78, 0x00, 0xfc, 0x00, 0x30, 0x00, 0xfe, 0x01, 0xfe, 0x01,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char below_on_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x78, 0x00, 0xfc, 0x00,
   0x30, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00 };

static const unsigned char below_off_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01,
   0x30, 0x00, 0xfc, 0x00, 0x78, 0x00, 0x30, 0x00 };

static const unsigned char shade_on_bits[] = {
   0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x02, 0x01, 0x02, 0x01,
   0x02, 0x01, 0x02, 0x01, 0xfe, 0x01, 0x00, 0x00 };

static const unsigned char shade_off_bits[] = {
   0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char pindown_white_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x1f, 0xa0, 0x03,
  0xb0, 0x01, 0x30, 0x01, 0xf0, 0x00, 0x70, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pindown_gray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c,
  0x00, 0x0e, 0x00, 0x06, 0x00, 0x00, 0x80, 0x07, 0xc0, 0x03, 0xe0, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pindown_dgray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x10, 0x70, 0x20, 0x50, 0x20,
  0x48, 0x30, 0xc8, 0x38, 0x08, 0x1f, 0x08, 0x18, 0x10, 0x1c, 0x10, 0x0e,
  0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pindown_mask_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x1f, 0xf0, 0x3f, 0xf0, 0x3f,
  0xf8, 0x3f, 0xf8, 0x3f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf0, 0x1f, 0xf0, 0x0f,
  0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pinup_white_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x11,
  0x3f, 0x15, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pinup_gray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x0a, 0xbf, 0x0a, 0x80, 0x15, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pinup_dgray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0x40, 0x31, 0x40, 0x2e,
  0x40, 0x20, 0x40, 0x20, 0x7f, 0x2a, 0x40, 0x3f, 0xc0, 0x31, 0xc0, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char pinup_mask_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0xc0, 0x31, 0xc0, 0x3f,
  0xff, 0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xc0, 0x3f, 0xc0, 0x31, 0xc0, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ===========================================================================

static QPixmap* titlePix;
static KPixmap* titleBuffer;
static KPixmap* aUpperGradient;
static KPixmap* iUpperGradient;

static KPixmap* pinDownPix;
static KPixmap* pinUpPix;
static KPixmap* ipinDownPix;
static KPixmap* ipinUpPix;

static KPixmap* rightBtnUpPix[2];
static KPixmap* rightBtnDownPix[2];
static KPixmap* irightBtnUpPix[2];
static KPixmap* irightBtnDownPix[2];

static KPixmap* leftBtnUpPix[2];
static KPixmap* leftBtnDownPix[2];
static KPixmap* ileftBtnUpPix[2];
static KPixmap* ileftBtnDownPix[2];

static KDEDefaultHandler* clientHandler;
static int	toolTitleHeight;
static int	normalTitleHeight;
static int borderWidth;
static int grabBorderWidth;
static bool KDEDefault_initialized = false;
static bool useGradients;
static bool showGrabBar;
static bool showTitleBarStipple;


// ===========================================================================

KDEDefaultHandler::KDEDefaultHandler()
{
        clientHandler = this;
	readConfig( false );
	createPixmaps();
	KDEDefault_initialized = true;
}


KDEDefaultHandler::~KDEDefaultHandler()
{
	KDEDefault_initialized = false;
	freePixmaps();
        clientHandler = NULL;
}

KDecoration* KDEDefaultHandler::createDecoration( KDecorationBridge* b )
{
        return new KDEDefaultClient( b, this );
}

bool KDEDefaultHandler::reset( unsigned long changed )
{
	KDEDefault_initialized = false;
        changed |= readConfig( true );
        if( changed & SettingColors )
        { // pixmaps need to be recreated
	        freePixmaps();
                createPixmaps();
        }
	KDEDefault_initialized = true;
        bool need_recreate = ( changed & ( SettingDecoration | SettingFont | SettingButtons | SettingBorder )) != 0;
        if( need_recreate )  // something else than colors changed
            return true;
        resetDecorations( changed );
        return false;
}


unsigned long KDEDefaultHandler::readConfig( bool update )
{
        unsigned long changed = 0;
	KConfig* conf = KGlobal::config();
	conf->setGroup("KDEDefault");

        bool new_showGrabBar 		= conf->readBoolEntry("ShowGrabBar", true);
	bool new_showTitleBarStipple = conf->readBoolEntry("ShowTitleBarStipple", true);
	bool new_useGradients 		= conf->readBoolEntry("UseGradients", true);
	int  new_titleHeight 		= QFontMetrics(options()->font(true)).height();
	int  new_toolTitleHeight 	= QFontMetrics(options()->font(true, true)).height()-2;

	int new_borderWidth;
	switch(options()->preferredBorderSize(this)) {
	case BorderLarge:
		new_borderWidth = 8;
		break;
	case BorderVeryLarge:
		new_borderWidth = 12;
		break;
	case BorderHuge:
		new_borderWidth = 18;
		break;
	case BorderVeryHuge:
		new_borderWidth = 27;
		break;
	case BorderOversized:
		new_borderWidth = 40;
		break;
	case BorderTiny:
	case BorderNormal:
	default:
		new_borderWidth = 4;
	}

	if (new_titleHeight < 16)              new_titleHeight = 16;
	if (new_titleHeight < new_borderWidth) new_titleHeight = new_borderWidth;
	if (new_toolTitleHeight < 12)              new_toolTitleHeight = 12;
	if (new_toolTitleHeight < new_borderWidth) new_toolTitleHeight = new_borderWidth;

        if( update )
        {
                if( new_showGrabBar != showGrabBar
                    || new_titleHeight != normalTitleHeight
                    || new_toolTitleHeight != toolTitleHeight
                    || new_borderWidth != borderWidth )
                        changed |= SettingDecoration; // need recreating the decoration
                if( new_showTitleBarStipple != showTitleBarStipple
                    || new_useGradients != useGradients
                    || new_titleHeight != normalTitleHeight
                    || new_toolTitleHeight != toolTitleHeight )
                        changed |= SettingColors; // just recreate the pixmaps and repaint
        }

        showGrabBar             = new_showGrabBar;
        showTitleBarStipple     = new_showTitleBarStipple;
        useGradients            = new_useGradients;
        normalTitleHeight       = new_titleHeight;
        toolTitleHeight         = new_toolTitleHeight;
        borderWidth             = new_borderWidth;
        grabBorderWidth         = (borderWidth > 15) ? borderWidth + 15 : 2*borderWidth;
        return changed;
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
		for(i=0, y=2; i < 9; ++i, y+=4)
			for(x=1; x <= 132; x+=3)
			{
				p.setPen(options()->color(ColorTitleBar, true).light(150));
				p.drawPoint(x, y);
				maskPainter.drawPoint(x, y);
				p.setPen(options()->color(ColorTitleBar, true).dark(150));
				p.drawPoint(x+1, y+1);
				maskPainter.drawPoint(x+1, y+1);
			}
		maskPainter.end();
		p.end();
		titlePix->setMask(mask);
	} else
		titlePix = NULL;

	QColor activeTitleColor1(options()->color(ColorTitleBar,      true));
	QColor activeTitleColor2(options()->color(ColorTitleBlend,    true));

	QColor inactiveTitleColor1(options()->color(ColorTitleBar,    false));
	QColor inactiveTitleColor2(options()->color(ColorTitleBlend,  false));

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
	g = options()->colorGroup( ColorButtonBg, true );
	pinUpPix  = new KPixmap();
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
	g = options()->colorGroup( ColorButtonBg, false );
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
	leftBtnUpPix[true] 	= new KPixmap();
	leftBtnUpPix[true]->resize(normalTitleHeight, normalTitleHeight);
	leftBtnDownPix[true]	= new KPixmap();
	leftBtnDownPix[true]->resize(normalTitleHeight, normalTitleHeight);
	ileftBtnUpPix[true]	= new KPixmap();
	ileftBtnUpPix[true]->resize(normalTitleHeight, normalTitleHeight);
	ileftBtnDownPix[true]	= new KPixmap();
	ileftBtnDownPix[true]->resize(normalTitleHeight, normalTitleHeight);

	rightBtnUpPix[true] 	= new KPixmap();
	rightBtnUpPix[true]->resize(normalTitleHeight, normalTitleHeight);
	rightBtnDownPix[true] = new KPixmap();
	rightBtnDownPix[true]->resize(normalTitleHeight, normalTitleHeight);
	irightBtnUpPix[true] 	= new KPixmap();
	irightBtnUpPix[true]->resize(normalTitleHeight, normalTitleHeight);
	irightBtnDownPix[true] = new KPixmap();
	irightBtnDownPix[true]->resize(normalTitleHeight, normalTitleHeight);

	leftBtnUpPix[false] 	= new KPixmap();
	leftBtnUpPix[false]->resize(toolTitleHeight, normalTitleHeight);
	leftBtnDownPix[false]	= new KPixmap();
	leftBtnDownPix[false]->resize(toolTitleHeight, normalTitleHeight);
	ileftBtnUpPix[false]	= new KPixmap();
	ileftBtnUpPix[false]->resize(normalTitleHeight, normalTitleHeight);
	ileftBtnDownPix[false]	= new KPixmap();
	ileftBtnDownPix[false]->resize(normalTitleHeight, normalTitleHeight);

	rightBtnUpPix[false] 	= new KPixmap();
	rightBtnUpPix[false]->resize(toolTitleHeight, toolTitleHeight);
	rightBtnDownPix[false] = new KPixmap();
	rightBtnDownPix[false]->resize(toolTitleHeight, toolTitleHeight);
	irightBtnUpPix[false] 	= new KPixmap();
	irightBtnUpPix[false]->resize(toolTitleHeight, toolTitleHeight);
	irightBtnDownPix[false] = new KPixmap();
	irightBtnDownPix[false]->resize(toolTitleHeight, toolTitleHeight);

	// Draw the button state pixmaps
	g = options()->colorGroup( ColorTitleBar, true );
	drawButtonBackground( leftBtnUpPix[true], g, false );
	drawButtonBackground( leftBtnDownPix[true], g, true );
	drawButtonBackground( leftBtnUpPix[false], g, false );
	drawButtonBackground( leftBtnDownPix[false], g, true );

	g = options()->colorGroup( ColorButtonBg, true );
	drawButtonBackground( rightBtnUpPix[true], g, false );
	drawButtonBackground( rightBtnDownPix[true], g, true );
	drawButtonBackground( rightBtnUpPix[false], g, false );
	drawButtonBackground( rightBtnDownPix[false], g, true );

	g = options()->colorGroup( ColorTitleBar, false );
	drawButtonBackground( ileftBtnUpPix[true], g, false );
	drawButtonBackground( ileftBtnDownPix[true], g, true );
	drawButtonBackground( ileftBtnUpPix[false], g, false );
	drawButtonBackground( ileftBtnDownPix[false], g, true );

	g = options()->colorGroup( ColorButtonBg, false );
	drawButtonBackground( irightBtnUpPix[true], g, false );
	drawButtonBackground( irightBtnDownPix[true], g, true );
	drawButtonBackground( irightBtnUpPix[false], g, false );
	drawButtonBackground( irightBtnDownPix[false], g, true );
}


void KDEDefaultHandler::freePixmaps()
{
	// Free button pixmaps
	if (rightBtnUpPix[true])
		delete rightBtnUpPix[true];
	if(rightBtnDownPix[true])
		delete rightBtnDownPix[true];
	if (irightBtnUpPix[true])
		delete irightBtnUpPix[true];
	if (irightBtnDownPix[true])
		delete irightBtnDownPix[true];

	if (leftBtnUpPix[true])
		delete leftBtnUpPix[true];
	if(leftBtnDownPix[true])
		delete leftBtnDownPix[true];
	if (ileftBtnUpPix[true])
		delete ileftBtnUpPix[true];
	if (ileftBtnDownPix[true])
		delete ileftBtnDownPix[true];

	if (rightBtnUpPix[false])
		delete rightBtnUpPix[false];
	if(rightBtnDownPix[false])
		delete rightBtnDownPix[false];
	if (irightBtnUpPix[false])
		delete irightBtnUpPix[false];
	if (irightBtnDownPix[false])
		delete irightBtnDownPix[false];

	if (leftBtnUpPix[false])
		delete leftBtnUpPix[false];
	if(leftBtnDownPix[false])
		delete leftBtnDownPix[false];
	if (ileftBtnUpPix[false])
		delete ileftBtnUpPix[false];
	if (ileftBtnDownPix[false])
		delete ileftBtnDownPix[false];

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

QValueList< KDEDefaultHandler::BorderSize > KDEDefaultHandler::borderSizes() const
{ // the list must be sorted
  return QValueList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}


// ===========================================================================

KDEDefaultButton::KDEDefaultButton(KDEDefaultClient *parent, const char *name,
           bool largeButton, bool isLeftButton, bool isStickyButton,
           const unsigned char *bitmap, const QString& tip, const int realizeBtns )
    : QButton(parent->widget(), name)
{
	realizeButtons = realizeBtns;

    QToolTip::add( this, tip );
    setCursor( arrowCursor );
    setBackgroundMode( QWidget::NoBackground );
	setToggleButton( isStickyButton );

	isMouseOver = false;
	deco 		= NULL;
    large 		= largeButton;
	isLeft 		= isLeftButton;
	isSticky 	= isStickyButton;
	client 		= parent;

    if (large)
       setFixedSize(normalTitleHeight, normalTitleHeight);
    else
       setFixedSize(toolTitleHeight, toolTitleHeight);

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
      return( QSize(normalTitleHeight, normalTitleHeight) );
   else
      return( QSize(toolTitleHeight, toolTitleHeight) );
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
							*leftBtnDownPix[large] : *ileftBtnDownPix[large];
			else
				btnbg = client->isActive() ?
							*leftBtnUpPix[large] : *ileftBtnUpPix[large];
		} else {
			if (isDown())
				btnbg = client->isActive() ?
							*rightBtnDownPix[large] : *irightBtnDownPix[large];
			else
				btnbg = client->isActive() ?
							*rightBtnUpPix[large] : *irightBtnUpPix[large];
		}

		p->drawPixmap( 0, 0, btnbg );

	} else if ( isLeft ) {

		// Fill the button background with an appropriate color/gradient
		// This is for sticky and menu buttons
		KPixmap* grad = client->isActive() ? aUpperGradient : iUpperGradient;
		if (!grad) {
			QColor c = KDecoration::options()->color(ColorTitleBar, client->isActive());
			p->fillRect(0, 0, width(), height(), c );
		} else
		 	p->drawPixmap( 0, 0, *grad, 0,1, width(), height() );

	} else {
		// Draw a plain background for menus or sticky buttons on RHS
   		QColor c = KDecoration::options()->color(ColorFrame, client->isActive());
		p->fillRect(0, 0, width(), height(), c);
	}


	// If we have a decoration bitmap, then draw that
	// otherwise we paint a menu button (with mini icon), or a sticky button.
	if( deco ) {
		// Select the appropriate button decoration color
   		bool darkDeco = qGray( KDecoration::options()->color(
				isLeft? ColorTitleBar : ColorButtonBg,
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
			btnpix = client->icon().pixmap( QIconSet::Small, QIconSet::Normal );

		// Intensify the image if required
		if (isMouseOver) {
                    btnpix = KPixmapEffect::intensity(btnpix, 0.8);
		}

		// Smooth scale the pixmap for small titlebars
		// This is slow, but we assume this isn't done too often
		if ( width() < 16 ) {
			btnpix.convertFromImage(btnpix.convertToImage().smoothScale(12, 12));
			p->drawPixmap( 0, 0, btnpix );
		}
		else
			p->drawPixmap( width()/2-8, height()/2-8, btnpix );
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
	QButton::enterEvent(e);
}


void KDEDefaultButton::leaveEvent(QEvent *e)
{
	isMouseOver=false;
	repaint(false);
	QButton::leaveEvent(e);
}


void KDEDefaultButton::mousePressEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					(e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mousePressEvent( &me );
}


void KDEDefaultButton::mouseReleaseEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					(e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mouseReleaseEvent( &me );
}


// ===========================================================================

KDEDefaultClient::KDEDefaultClient( KDecorationBridge* b, KDecorationFactory* f )
    : KDecoration( b, f ),
      m_closing(false)
{
}

void KDEDefaultClient::init()
{
    connect( this, SIGNAL( keepAboveChanged( bool )), SLOT( keepAboveChange( bool )));
    connect( this, SIGNAL( keepBelowChanged( bool )), SLOT( keepBelowChange( bool )));

    createMainWidget( WResizeNoErase | WStaticContents | WRepaintNoErase );
    widget()->installEventFilter( this );

	// No flicker thanks
    widget()->setBackgroundMode( QWidget::NoBackground );

	// Set button pointers to NULL so we can track things
	for(int i=0; i < KDEDefaultClient::BtnCount; i++)
		button[i] = NULL;

    // Finally, toolWindows look small
    if ( isTool() ) {
		titleHeight  = toolTitleHeight;
		largeButtons = false;
	}
	else {
		titleHeight  = normalTitleHeight;
		largeButtons = true;
    }

    // Pack the windowWrapper() window within a grid
    g = new QGridLayout(widget(), 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);       // Top grab bar
    g->addRowSpacing(2, 1);       // line under titlebar
    if( isPreview())
        g->addWidget( new QLabel( i18n( "<b><center>KDE2 preview</center></b>" ), widget()), 3, 1);
    else
        g->addItem( new QSpacerItem( 0, 0 ), 3, 1); // no widget in the middle

	// without the next line, unshade flickers
	g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed,
								 QSizePolicy::Expanding ) );
    g->setRowStretch(3, 10);      // Wrapped window

	// Determine the size of the lower grab bar
    spacer = new QSpacerItem(10,
			mustDrawHandle() ? grabBorderWidth : borderWidth,
			QSizePolicy::Expanding, QSizePolicy::Minimum);
    g->addItem(spacer, 4, 1);

    g->addColSpacing(0, borderWidth);
    g->addColSpacing(2, borderWidth);

    // Pack the titlebar HBox with items
    hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0 );
    hb->setResizeMode( QLayout::FreeResize );
    g->addLayout ( hb, 1, 1 );

	addClientButtons( options()->titleButtonsLeft() );
    titlebar = new QSpacerItem( 10, titleHeight, QSizePolicy::Expanding,
								QSizePolicy::Minimum );
    hb->addItem(titlebar);
    hb->addSpacing(2);
	addClientButtons( options()->titleButtonsRight(), false );
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
							largeButtons, isLeft, false, NULL, i18n("Menu"), LeftButton|RightButton);
   					connect( button[BtnMenu], SIGNAL(pressed()),
							this, SLOT(menuButtonPressed()) );
					connect( button[BtnMenu], SIGNAL(released()),
							 this, SLOT(menuButtonReleased()));
					hb->addWidget( button[BtnMenu] );
				}
				break;

			// Sticky button
			case 'S':
				if (!button[BtnSticky])
				{
   					button[BtnSticky] = new KDEDefaultButton(this, "sticky",
							largeButtons, isLeft, true, NULL,
                                                        isOnAllDesktops()?i18n("Not on all desktops"):i18n("On all desktops"));
					button[BtnSticky]->turnOn( isOnAllDesktops() );
   					connect( button[BtnSticky], SIGNAL(clicked()),
							this, SLOT(toggleOnAllDesktops()) );
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
							this, SLOT( showContextHelp() ));
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
							this, SLOT(minimize()) );
					hb->addWidget( button[BtnIconify] );
				}
				break;

			// Maximize button
			case 'A':
				if ( (!button[BtnMax]) && isMaximizable())
				{
				    button[BtnMax]  = new KDEDefaultButton(this, "maximize",
							largeButtons, isLeft, true, maximize_bits,
							i18n("Maximize"), LeftButton|MidButton|RightButton);
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

			// Above button
			case 'F':
				if ( (!button[BtnAbove]))
				{
				    button[BtnAbove]  = new KDEDefaultButton(this, "above",
							largeButtons, isLeft, true,
                                                        keepAbove() ? above_on_bits : above_off_bits,
							i18n("Keep Above Others"));
				    connect( button[BtnAbove], SIGNAL( clicked()),
							this, SLOT(slotAbove()) );
					hb->addWidget( button[BtnAbove] );
				}
				break;

			// Below button
			case 'B':
				if ( (!button[BtnBelow]))
				{
				    button[BtnBelow]  = new KDEDefaultButton(this, "below",
							largeButtons, isLeft, true,
                                                        keepBelow() ? below_on_bits : below_off_bits,
							i18n("Keep Below Others"));
				    connect( button[BtnBelow], SIGNAL( clicked()),
							this, SLOT(slotBelow()) );
					hb->addWidget( button[BtnBelow] );
				}
				break;

			// Shade button
			case 'L':
				if ( (!button[BtnShade]) && isShadeable())
				{
				    button[BtnShade]  = new KDEDefaultButton(this, "shade",
							largeButtons, isLeft, true,
                                                        isSetShade() ? shade_on_bits : shade_off_bits,
							isSetShade() ? i18n( "Unshade" ) : i18n("Shade"));
				    connect( button[BtnShade], SIGNAL( clicked()),
							this, SLOT(slotShade()) );
					hb->addWidget( button[BtnShade] );
				}
				break;

			// Spacer item (only for non-tool windows)
			case '_':
    			if ( !isTool() )
	   				hb->addSpacing(borderWidth/2);
		}
	}
}

void KDEDefaultClient::reset( unsigned long )
{
    widget()->repaint();
}

bool KDEDefaultClient::mustDrawHandle() const 
{ 
    bool drawSmallBorders = !options()->moveResizeMaximizedWindows();
    if (drawSmallBorders && (maximizeMode() & MaximizeVertical)) {
		return false;
    } else {
		return showGrabBar && isResizable();
    }
}

void KDEDefaultClient::iconChange()
{
	if (button[BtnMenu] && button[BtnMenu]->isVisible())
		button[BtnMenu]->repaint(false);
}

void KDEDefaultClient::desktopChange()
{
	if (button[BtnSticky]) {
                bool on = isOnAllDesktops();
		button[BtnSticky]->turnOn(on);
		button[BtnSticky]->repaint(false);
                QToolTip::remove( button[BtnSticky] );
		QToolTip::add( button[BtnSticky], on ? i18n("Not on all desktops") : i18n("On all desktops"));
	}
}

void KDEDefaultClient::keepAboveChange( bool above )
{
	if (button[BtnAbove]) {
		button[BtnAbove]->setBitmap( above ? above_on_bits : above_off_bits );
		button[BtnAbove]->repaint(false);
	}
}

void KDEDefaultClient::keepBelowChange( bool below )
{
	if (button[BtnBelow]) {
		button[BtnBelow]->setBitmap( below ? below_on_bits : below_off_bits );
		button[BtnBelow]->repaint(false);
	}
}

void KDEDefaultClient::slotMaximize()
{
    maximize( button[BtnMax]->last_button );
}

void KDEDefaultClient::slotAbove()
{
    setKeepAbove( !keepAbove());
    button[BtnAbove]->turnOn(keepAbove());
    button[BtnAbove]->repaint(true);
}

void KDEDefaultClient::slotBelow()
{
    setKeepBelow( !keepBelow());
    button[BtnBelow]->turnOn(keepBelow());
    button[BtnBelow]->repaint(true);
}

void KDEDefaultClient::slotShade()
{
    setShade( !isSetShade());
    button[BtnShade]->setBitmap(isSetShade() ? shade_on_bits : shade_off_bits );
    button[BtnShade]->repaint(true);
}

void KDEDefaultClient::resizeEvent( QResizeEvent* e)
{
	doShape();
    calcHiddenButtons();

    if ( widget()->isShown())
    {
        widget()->update( widget()->rect());
#if 1 // what's the point of this, when paintEvent() repaints everything anyway?
        int dx = 0;
        int dy = 0;

        if ( e->oldSize().width() != width() )
           dx = 32 + QABS( e->oldSize().width() -  width() );

        if ( e->oldSize().height() != height() )
	       dy = 8 + QABS( e->oldSize().height() -  height() );

        if ( dy )
	       widget()->update( 0, height() - dy + 1, width(), dy );

        if ( dx )
        {
  	       widget()->update( width() - dx + 1, 0, dx, height() );
	       widget()->update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() -
					QPoint(1,0) ) );
	       widget()->update( QRect( titlebar->geometry().topRight(), QPoint(width() - 4,
						  titlebar->geometry().bottom()) ) );
           // Titlebar needs no paint event
	       QApplication::postEvent( widget(), new QPaintEvent(titlebar->geometry(),
									 FALSE) );
        }
#endif
    }
}


void KDEDefaultClient::captionChange()
{
    widget()->repaint( titlebar->geometry(), false );
}


void KDEDefaultClient::paintEvent( QPaintEvent* )
{
	if (!KDEDefault_initialized)
		return;

	QColorGroup g;
	int offset;

	KPixmap* upperGradient = isActive() ? aUpperGradient : iUpperGradient;

    QPainter p(widget());

    // Obtain widget bounds.
    QRect r(widget()->rect());
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
	g = options()->colorGroup(ColorTitleBar, isActive());
	p.setPen(g.light());
	p.drawLine(x+1, y+1, rightOffset-1, y+1);
	p.drawLine(x+1, y+1, x+1, leftFrameStart+borderWidth-4);

	// Draw titlebar colour separator line
	p.setPen(g.dark());
	p.drawLine(rightOffset-1, y+1, rightOffset-1, titleHeight+2);

	p.fillRect(x+2, y+titleHeight+3,
	           borderWidth-4, leftFrameStart+borderWidth-y-titleHeight-8,
	           options()->color(ColorTitleBar, isActive() ));

	// Finish drawing the titlebar extension
	p.setPen(Qt::black);
	p.drawLine(x+1, leftFrameStart+borderWidth-4, x+borderWidth-2, leftFrameStart-1);
	p.setPen(g.mid());
	p.drawLine(x+borderWidth-2, y+titleHeight+3, x+borderWidth-2, leftFrameStart-2);

    // Fill out the border edges
    g = options()->colorGroup(ColorFrame, isActive());
    p.setPen(g.light());
    p.drawLine(rightOffset, y+1, x2-1, y+1);
    p.drawLine(x+1, leftFrameStart+borderWidth-3, x+1, y2-1);
    p.setPen(g.dark());
    p.drawLine(x2-1, y+1, x2-1, y2-1);
    p.drawLine(x+1, y2-1, x2-1, y2-1);

	p.setPen(options()->color(ColorFrame, isActive()));
	QPointArray a;
	QBrush brush( options()->color(ColorFrame, isActive()), Qt::SolidPattern );
	p.setBrush( brush );                       // use solid, yellow brush
    a.setPoints( 4, x+2,             leftFrameStart+borderWidth-4,
	                x+borderWidth-2, leftFrameStart,
	                x+borderWidth-2, y2-2,
	                x+2,             y2-2);
    p.drawPolygon( a );
	p.fillRect(x2-borderWidth+2, y+titleHeight+3,
	           borderWidth-3, y2-y-titleHeight-4,
	           options()->color(ColorFrame, isActive() ));

	// Draw the bottom handle if required
	if (mustDrawHandle())
	{
		if(w > 50)
		{
			qDrawShadePanel(&p, x+1, y2-grabBorderWidth+2, 2*borderWidth+12, grabBorderWidth-2,
							g, false, 1, &g.brush(QColorGroup::Mid));
			qDrawShadePanel(&p, x+2*borderWidth+13, y2-grabBorderWidth+2, w-4*borderWidth-26, grabBorderWidth-2,
							g, false, 1, isActive() ?
							&g.brush(QColorGroup::Background) :
							&g.brush(QColorGroup::Mid));
			qDrawShadePanel(&p, x2-2*borderWidth-12, y2-grabBorderWidth+2, 2*borderWidth+12, grabBorderWidth-2,
							g, false, 1, &g.brush(QColorGroup::Mid));
		} else
			qDrawShadePanel(&p, x+1, y2-grabBorderWidth+2, w-2, grabBorderWidth-2,
							g, false, 1, isActive() ?
							&g.brush(QColorGroup::Background) :
							&g.brush(QColorGroup::Mid));
		offset = grabBorderWidth;
	} else
		{
			p.fillRect(x+2, y2-borderWidth+2, w-4, borderWidth-3,
			           options()->color(ColorFrame, isActive() ));
			offset = borderWidth;
		}

    // Draw a frame around the wrapped widget.
    p.setPen( g.dark() );
    p.drawRect( x+borderWidth-1, y+titleHeight+3, w-2*borderWidth+2, h-titleHeight-offset-2 );

    // Draw the title bar.
	r = titlebar->geometry();

    // Obtain titlebar blend colours
    QColor c1 = options()->color(ColorTitleBar, isActive() );
    QColor c2 = options()->color(ColorFrame, isActive() );

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
    QFont fnt = options()->font(true);

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

    p2.setPen( options()->color(ColorFont, isActive()) );
    p2.drawText(r.x(), 1, r.width()-1, r.height(),
		(caption().isRightToLeft() ? AlignRight : AlignLeft) | AlignVCenter,
		caption() );

	bitBlt( widget(), 2, 2, titleBuffer );

    p2.end();

	// Ensure a shaded window has no unpainted areas
	// Is this still needed?
#if 1
	p.setPen(c2);
    p.drawLine(x+borderWidth, y+titleHeight+4, x2-borderWidth, y+titleHeight+4);
#endif
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


void KDEDefaultClient::showEvent(QShowEvent *)
{
    calcHiddenButtons();
	doShape();
}


void KDEDefaultClient::mouseDoubleClickEvent( QMouseEvent * e )
{
	if (titlebar->geometry().contains( e->pos() ) )
                titlebarDblClickOperation();
}


void KDEDefaultClient::maximizeChange()
{
	if (button[BtnMax]) {
                bool m = maximizeMode() == MaximizeFull;
		button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
                QToolTip::remove( button[ BtnMax ] );
                QToolTip::add( button[BtnMax], m ? i18n("Restore") : i18n("Maximize"));
	}
	spacer->changeSize(10, mustDrawHandle() ? 8 : 4,
			QSizePolicy::Expanding, QSizePolicy::Minimum);
	g->activate();
}


void KDEDefaultClient::activeChange()
{
	for(int i=KDEDefaultClient::BtnHelp; i < KDEDefaultClient::BtnCount; i++)
		if(button[i])
			button[i]->repaint(false);
	widget()->repaint(false);
}

void KDEDefaultClient::shadeChange()
{
	if (button[BtnShade]) {
                bool on = isShade();
		button[BtnShade]->turnOn(on);
		button[BtnShade]->repaint(false);
                QToolTip::remove( button[BtnShade] );
		QToolTip::add( button[BtnShade], on ? i18n("Unshade") : i18n("Shade"));
	}
}

QSize KDEDefaultClient::minimumSize() const
{
    return QSize( 100, 50 ); // FRAME
}

void KDEDefaultClient::resize( const QSize& s )
{
    widget()->resize( s );
}

void KDEDefaultClient::borders( int& left, int& right, int& top, int& bottom ) const
{ // FRAME
    left = right = borderWidth;
//    , y+titleHeight+3, w-6, h-titleHeight-offset-6 );
    top = titleHeight + 4;
	bottom = mustDrawHandle() ? grabBorderWidth : borderWidth;
}

// The hiding button while shrinking, show button while expanding magic
void KDEDefaultClient::calcHiddenButtons()
{
	// Hide buttons in this order:
	// Shade, Below, Above, Sticky, Help, Maximize, Minimize, Close, Menu.
	KDEDefaultButton* btnArray[] = { button[ BtnShade ], button[ BtnBelow ],
                        button[ BtnAbove ], button[BtnSticky], button[BtnHelp],
			button[BtnMax], button[BtnIconify], button[BtnClose],
			button[BtnMenu] };
        const int buttons_cnt = sizeof( btnArray ) / sizeof( btnArray[ 0 ] );

	int minwidth  = largeButtons ? 10 * normalTitleHeight : 10 * toolTitleHeight; // Start hiding at this width
	int btn_width = largeButtons ? normalTitleHeight : toolTitleHeight;
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
	if (count > buttons_cnt) count = buttons_cnt;

	// Hide the required buttons...
	for(i = 0; i < count; i++)
	{
		if (btnArray[i] && btnArray[i]->isVisible() )
			btnArray[i]->hide();
	}

	// Show the rest of the buttons...
	for(i = count; i < buttons_cnt; i++)
	{
		if (btnArray[i] && (!btnArray[i]->isVisible()) )
			btnArray[i]->show();
	}
}


KDecoration::Position KDEDefaultClient::mousePosition( const QPoint& p ) const
{
	Position m = PositionCenter;

	int bottomSize = mustDrawHandle() ? grabBorderWidth : borderWidth;

    const int range = 14 + 3*borderWidth/2;

    if ( ( p.x() > borderWidth && p.x() < width() - borderWidth )
         && ( p.y() > 4 && p.y() < height() - bottomSize ) )
        m = PositionCenter;
    else if ( p.y() <= range && p.x() <= range)
        m = PositionTopLeft;
    else if ( p.y() >= height()-range && p.x() >= width()-range)
        m = PositionBottomRight;
    else if ( p.y() >= height()-range && p.x() <= range)
        m = PositionBottomLeft;
    else if ( p.y() <= range && p.x() >= width()-range)
        m = PositionTopRight;
    else if ( p.y() <= 4 )
        m = PositionTop;
    else if ( p.y() >= height()-bottomSize )
        m = PositionBottom;
    else if ( p.x() <= borderWidth )
        m = PositionLeft;
    else if ( p.x() >= width()-borderWidth )
        m = PositionRight;
    else
        m = PositionCenter;

	// Modify the mouse position if we are using a grab bar.
	if (mustDrawHandle())
		if (p.y() >= (height() - grabBorderWidth))
		{
			if (p.x() >= (width() - 2*borderWidth - 12))
				m = PositionBottomRight;
			else if (p.x() <= 2*borderWidth + 12)
				m = PositionBottomLeft;
			else
			m = PositionBottom;
		}

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
		m_closing = true;
		return;
	}

	QPoint menupoint ( button[BtnMenu]->rect().bottomLeft().x()-1,
					   button[BtnMenu]->rect().bottomLeft().y()+2 );
        KDecorationFactory* f = factory();
	QRect menuRect = button[BtnMenu]->rect();
	QPoint menutop = button[BtnMenu]->mapToGlobal(menuRect.topLeft());
	QPoint menubottom = button[BtnMenu]->mapToGlobal(menuRect.bottomRight());
	showWindowMenu(QRect(menutop, menubottom));
        if( !f->exists( this )) // 'this' was destroyed
            return;
	button[BtnMenu]->setDown(false);
}

void KDEDefaultClient::menuButtonReleased()
{
	if (m_closing)
		closeWindow();
}

const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
    | NET::ToolbarMask | NET::MenuMask | NET::DialogMask | NET::OverrideMask | NET::TopMenuMask
    | NET::UtilityMask | NET::SplashMask;

bool KDEDefaultClient::isTool() const
{
    NET::WindowType type = windowType( SUPPORTED_WINDOW_TYPES_MASK );
    return type == NET::Toolbar || type == NET::Utility || type == NET::Menu;
}


bool KDEDefaultClient::eventFilter( QObject* o, QEvent* e )
{
    if( o != widget())
	return false;
    switch( e->type())
	{
	case QEvent::Resize:
	    resizeEvent( static_cast< QResizeEvent* >( e ));
	    return true;
	case QEvent::Paint:
	    paintEvent( static_cast< QPaintEvent* >( e ));
	    return true;
	case QEvent::MouseButtonDblClick:
	    mouseDoubleClickEvent( static_cast< QMouseEvent* >( e ));
	    return true;
	case QEvent::MouseButtonPress:
	    processMousePressEvent( static_cast< QMouseEvent* >( e ));
	    return true;
	case QEvent::Show:
	    showEvent( static_cast< QShowEvent* >( e ));
	    return true;
	default:
	    break;
	}
    return false;
}


} // namespace

// Extended KWin plugin interface
extern "C" KDecorationFactory* create_factory()
{
    return new Default::KDEDefaultHandler();
}

#include "kdedefault.moc"
// vim: ts=4
