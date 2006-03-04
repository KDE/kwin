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
#include <QPolygon>

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
	// SettingButtons is handled by KCommonDecoration
        bool need_recreate = ( changed & ( SettingDecoration | SettingFont | SettingBorder )) != 0;
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

        bool new_showGrabBar 		= conf->readEntry("ShowGrabBar", QVariant(true)).toBool();
	bool new_showTitleBarStipple = conf->readEntry("ShowTitleBarStipple", QVariant(true)).toBool();
	bool new_useGradients 		= conf->readEntry("UseGradients", QVariant(true)).toBool();
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
	g = options()->palette( ColorButtonBg, true ).active();
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
	g = options()->palette( ColorButtonBg, false ).active();
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
	g = options()->palette( ColorTitleBar, true ).active();
	drawButtonBackground( leftBtnUpPix[true], g, false );
	drawButtonBackground( leftBtnDownPix[true], g, true );
	drawButtonBackground( leftBtnUpPix[false], g, false );
	drawButtonBackground( leftBtnDownPix[false], g, true );

	g = options()->palette( ColorButtonBg, true ).active();
	drawButtonBackground( rightBtnUpPix[true], g, false );
	drawButtonBackground( rightBtnDownPix[true], g, true );
	drawButtonBackground( rightBtnUpPix[false], g, false );
	drawButtonBackground( rightBtnDownPix[false], g, true );

	g = options()->palette( ColorTitleBar, false ).active();
	drawButtonBackground( ileftBtnUpPix[true], g, false );
	drawButtonBackground( ileftBtnDownPix[true], g, true );
	drawButtonBackground( ileftBtnUpPix[false], g, false );
	drawButtonBackground( ileftBtnDownPix[false], g, true );

	g = options()->palette( ColorButtonBg, false ).active();
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

QList< KDEDefaultHandler::BorderSize > KDEDefaultHandler::borderSizes() const
{ // the list must be sorted
  return QList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

bool KDEDefaultHandler::supports( Ability ability )
{
    switch( ability )
        {
        case AbilityAnnounceButtons:
        case AbilityButtonMenu:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonSpacer:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
            return true;
        default:
            return false;
        };
}

// ===========================================================================

KDEDefaultButton::KDEDefaultButton(ButtonType type, KDEDefaultClient *parent, const char *name)
	: KCommonDecorationButton(type, parent)
{
    setAttribute( Qt::WA_NoBackground );

	isMouseOver = false;
	deco 		= NULL;
	large 		= !decoration()->isToolWindow();
}


KDEDefaultButton::~KDEDefaultButton()
{
	if (deco)
		delete deco;
}


void KDEDefaultButton::reset(unsigned long changed)
{
	if (changed&DecorationReset || changed&ManualReset || changed&SizeChange || changed&StateChange) {
		switch (type() ) {
			case CloseButton:
				setBitmap(close_bits);
				break;
			case HelpButton:
				setBitmap(question_bits);
				break;
			case MinButton:
				setBitmap(iconify_bits);
				break;
			case MaxButton:
				setBitmap( isChecked() ? minmax_bits : maximize_bits );
				break;
			case OnAllDesktopsButton:
				setBitmap(0);
				break;
			case ShadeButton:
				setBitmap( isChecked() ? shade_on_bits : shade_off_bits );
				break;
			case AboveButton:
				setBitmap( isChecked() ? above_on_bits : above_off_bits );
				break;
			case BelowButton:
				setBitmap( isChecked() ? below_on_bits : below_off_bits );
				break;
			default:
				setBitmap(0);
				break;
		}

		this->update();
	}
}


void KDEDefaultButton::setBitmap(const unsigned char *bitmap)
{
	delete deco;
	deco = 0;

	if (bitmap) {
		deco = new QBitmap(10, 10, bitmap, true);
		deco->setMask( *deco );
	}
}

void KDEDefaultButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	drawButton(&p);
}

void KDEDefaultButton::drawButton(QPainter *p)
{
	if (!KDEDefault_initialized)
		return;

	const bool active = decoration()->isActive();

	if (deco) {
		// Fill the button background with an appropriate button image
 		KPixmap btnbg;

		if (isLeft() )	{
			if (isDown())
				btnbg = active ?
							*leftBtnDownPix[large] : *ileftBtnDownPix[large];
			else
				btnbg = active ?
							*leftBtnUpPix[large] : *ileftBtnUpPix[large];
		} else {
			if (isDown())
				btnbg = active ?
							*rightBtnDownPix[large] : *irightBtnDownPix[large];
			else
				btnbg = active ?
							*rightBtnUpPix[large] : *irightBtnUpPix[large];
		}

		p->drawPixmap( 0, 0, btnbg );

	} else if ( isLeft() ) {

		// Fill the button background with an appropriate color/gradient
		// This is for sticky and menu buttons
		KPixmap* grad = active ? aUpperGradient : iUpperGradient;
		if (!grad) {
			QColor c = KDecoration::options()->color(KDecoration::ColorTitleBar, active);
			p->fillRect(0, 0, width(), height(), c );
		} else
		 	p->drawPixmap( 0, 0, *grad, 0,1, width(), height() );

	} else {
		// Draw a plain background for menus or sticky buttons on RHS
		QColor c = KDecoration::options()->color(KDecoration::ColorFrame, active);
		p->fillRect(0, 0, width(), height(), c);
	}


	// If we have a decoration bitmap, then draw that
	// otherwise we paint a menu button (with mini icon), or a sticky button.
	if( deco ) {
		// Select the appropriate button decoration color
   		bool darkDeco = qGray( KDecoration::options()->color(
				isLeft() ? KDecoration::ColorTitleBar : KDecoration::ColorButtonBg,
				active).rgb() ) > 127;

		if (isMouseOver)
			p->setPen( darkDeco ? Qt::darkGray : Qt::lightGray );
		else
			p->setPen( darkDeco ? Qt::black : Qt::white );

		int xOff = (width()-10)/2;
		int yOff = (height()-10)/2;
		p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, *deco);

	} else {
		KPixmap btnpix;

		if (type()==OnAllDesktopsButton) {
			if (active)
				btnpix = isChecked() ? *pinDownPix : *pinUpPix;
			else
				btnpix = isChecked() ? *ipinDownPix : *ipinUpPix;
		} else
			btnpix = decoration()->icon().pixmap( QIcon::Small, QIcon::Normal );

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


void KDEDefaultButton::enterEvent(QEvent *e)
{
	isMouseOver=true;
	repaint(false);
	KCommonDecorationButton::enterEvent(e);
}


void KDEDefaultButton::leaveEvent(QEvent *e)
{
	isMouseOver=false;
	repaint(false);
	KCommonDecorationButton::leaveEvent(e);
}


// ===========================================================================

KDEDefaultClient::KDEDefaultClient( KDecorationBridge* b, KDecorationFactory* f )
    : KCommonDecoration( b, f )/*,
      m_closing(false)*/
{
}

QString KDEDefaultClient::visibleName() const
{
	return i18n("KDE2");
}

QString KDEDefaultClient::defaultButtonsLeft() const
{
	return "MS";
}

QString KDEDefaultClient::defaultButtonsRight() const
{
	return "HIAX";
}

bool KDEDefaultClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
	switch (behaviour) {
		case DB_MenuClose:
			return true;
		case DB_WindowMask:
			return true;
		case DB_ButtonHide:
			return true;

		default:
			return KCommonDecoration::decorationBehaviour(behaviour);
	}
}

int KDEDefaultClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
	switch (lm) {
		case LM_BorderLeft:
		case LM_BorderRight:
			return borderWidth;

		case LM_BorderBottom:
			return mustDrawHandle() ? grabBorderWidth : borderWidth;

		case LM_TitleEdgeLeft:
		case LM_TitleEdgeRight:
			return borderWidth;

		case LM_TitleEdgeTop:
			return 3;

		case LM_TitleEdgeBottom:
			return 1;

		case LM_TitleBorderLeft:
		case LM_TitleBorderRight:
			return 1;

		case LM_TitleHeight:
			return titleHeight;

		case LM_ButtonWidth:
		case LM_ButtonHeight:
			return titleHeight;

		case LM_ButtonSpacing:
			return 0;

		case LM_ExplicitButtonSpacer:
			if ( !isToolWindow() )
				return borderWidth/2;

		default:
			return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
	}
}

KCommonDecorationButton *KDEDefaultClient::createButton(ButtonType type)
{
	switch (type) {
		case MenuButton:
			return new KDEDefaultButton(MenuButton, this, "menu");
		case OnAllDesktopsButton:
			return new KDEDefaultButton(OnAllDesktopsButton, this, "on_all_desktops");
		case HelpButton:
			return new KDEDefaultButton(HelpButton, this, "help");
		case MinButton:
			return new KDEDefaultButton(MinButton, this, "minimize");
		case MaxButton:
			return new KDEDefaultButton(MaxButton, this, "maximize");
		case CloseButton:
			return new KDEDefaultButton(CloseButton, this, "close");
		case AboveButton:
			return new KDEDefaultButton(AboveButton, this, "above");
		case BelowButton:
			return new KDEDefaultButton(BelowButton, this, "below");
		case ShadeButton:
			return new KDEDefaultButton(ShadeButton, this, "shade");

		default:
			return 0;
	}
}

void KDEDefaultClient::init()
{
    // Finally, toolWindows look small
    if ( isToolWindow() ) {
		titleHeight  = toolTitleHeight;
	}
	else {
		titleHeight  = normalTitleHeight;
    }

	KCommonDecoration::init();
}

void KDEDefaultClient::reset( unsigned long changed)
{
    widget()->repaint();

	KCommonDecoration::reset(changed);
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
	r = titleRect();
	int rightOffset = r.x()+r.width()+1;

    // Create a disposable pixmap buffer for the titlebar
	// very early before drawing begins so there is no lag
	// during painting pixels.
    titleBuffer->resize( rightOffset-3, titleHeight+1 );

	// Draw an outer black frame
	p.setPen(Qt::black);
	p.drawRect(x,y,w,h);

    // Draw part of the frame that is the titlebar color
	g = options()->palette(ColorTitleBar, isActive()).active();
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
	g = options()->palette(ColorFrame, isActive()).active();
    p.setPen(g.light());
    p.drawLine(rightOffset, y+1, x2-1, y+1);
    p.drawLine(x+1, leftFrameStart+borderWidth-3, x+1, y2-1);
    p.setPen(g.dark());
    p.drawLine(x2-1, y+1, x2-1, y2-1);
    p.drawLine(x+1, y2-1, x2-1, y2-1);

	p.setPen(options()->color(ColorFrame, isActive()));
	QPolygon a;
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
	r = titleRect();

    // Obtain titlebar blend colours
    QColor c1 = options()->color(ColorTitleBar, isActive() );
    QColor c2 = options()->color(ColorFrame, isActive() );

	// Fill with frame color behind RHS buttons
	p.fillRect( rightOffset, y+2, x2-rightOffset-1, titleHeight+1, c2);

    QPainter p2( titleBuffer );
    p2.initFrom( widget());

	// Draw the titlebar gradient
	if (upperGradient)
		p2.drawTiledPixmap(0, 0, rightOffset-3, titleHeight+1, *upperGradient);
	else
	    p2.fillRect(0, 0, rightOffset-3, titleHeight+1, c1);

    // Draw the title text on the pixmap, and with a smaller font
    // for toolwindows than the default.
    QFont fnt = options()->font(true);

    if ( isToolWindow() )
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
		(caption().isRightToLeft() ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
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

QRegion KDEDefaultClient::cornerShape(WindowCorner corner)
{
	switch (corner) {
		case WC_TopLeft:
			return QRect(0, 0, 1, 1);

		case WC_TopRight:
			return QRect(width()-1, 0, 1, 1);

		case WC_BottomLeft:
			return QRect(0, height()-1, 1, 1);

		case WC_BottomRight:
			return QRect(width()-1, height()-1, 1, 1);

		default:
			return QRegion();
	}
}

} // namespace

// Extended KWin plugin interface
extern "C" KDE_EXPORT KDecorationFactory* create_factory()
{
    return new Default::KDEDefaultHandler();
}

// vim: ts=4
// kate: space-indent off; tab-width 4;
