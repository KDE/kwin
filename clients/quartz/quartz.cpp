/*
 *
 * Gallium-Quartz KWin client
 *
 * Copyright (C) 2005 Sandro Giessl <sandro@giessl.com>
 * Copyright 2001
 *   Karol Szwed <gallium@kde.org>
 *   http://gallium.n3.net/
 *
 * Based on the KDE default client.
 *
 * Includes mini titlebars for ToolWindow Support.
 * Button positions are now customizable.
 *
 */

#include <kconfig.h>
#include <kdrawutil.h>
#include <kglobal.h>
#include <klocale.h>
#include <kpixmapeffect.h>
#include <qbitmap.h>
#include <qdrawutil.h>
#include <qimage.h>
#include <qapplication.h>

#include "quartz.h"
//Added by qt3to4:
#include <QPixmap>
#include <QPaintEvent>


namespace Quartz {

static const unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00,
  0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char close_bits[] = {
  0x00, 0x00, 0x86, 0x01, 0xcc, 0x00, 0x78, 0x00, 0x30, 0x00, 0x78, 0x00,
  0xcc, 0x00, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00};

static const unsigned char maximize_bits[] = {
  0xff, 0x01, 0xff, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0xff, 0x01, 0x00, 0x00};

static const unsigned char minmax_bits[] = {
  0xfc, 0x00, 0xfc, 0x00, 0x84, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0xe1, 0x00,
  0x21, 0x00, 0x21, 0x00, 0x3f, 0x00, 0x00, 0x00};

static const unsigned char question_bits[] = {
  0x00, 0x00, 0x3c, 0x00, 0x66, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00,
  0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00};

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

static const unsigned char above_on_bits[] = {
  0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x30, 0x00, 0xfc, 0x00, 0x78, 0x00,
  0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char above_off_bits[] = {
  0x30, 0x00, 0x78, 0x00, 0xfc, 0x00, 0x30, 0x00, 0xfe, 0x01, 0xfe, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char below_on_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x78, 0x00, 0xfc, 0x00,
  0x30, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00};

static const unsigned char below_off_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01,
  0x30, 0x00, 0xfc, 0x00, 0x78, 0x00, 0x30, 0x00};

static const unsigned char shade_on_bits[] = {
  0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x02, 0x01, 0x02, 0x01,
  0x02, 0x01, 0x02, 0x01, 0xfe, 0x01, 0x00, 0x00};

static const unsigned char shade_off_bits[] = {
  0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


///////////////////////////////////////////////////////////////////////////

// Titlebar button positions
bool onAllDesktopsButtonOnLeft = true;
bool coloredFrame		= true;
bool extraSlim = false;

KPixmap* titleBlocks 	= NULL;
KPixmap* ititleBlocks 	= NULL;
KPixmap* pinDownPix		= NULL;
KPixmap* pinUpPix		= NULL;
KPixmap* ipinDownPix	= NULL;
KPixmap* ipinUpPix		= NULL;
static int normalTitleHeight;
static int toolTitleHeight;
static int borderWidth;

bool quartz_initialized = false;
QuartzHandler* clientHandler;

///////////////////////////////////////////////////////////////////////////


QuartzHandler::QuartzHandler()
{
	quartz_initialized = false;
	readConfig();
	createPixmaps();
	quartz_initialized = true;
}


QuartzHandler::~QuartzHandler()
{
	quartz_initialized = false;
	freePixmaps();
}


KDecoration* QuartzHandler::createDecoration( KDecorationBridge* bridge )
{
	return new QuartzClient( bridge, this );
}


bool QuartzHandler::reset(unsigned long changed)
{
	quartz_initialized = false;
	freePixmaps();
	readConfig();
	createPixmaps();
	quartz_initialized = true;

	// Do we need to "hit the wooden hammer" ?
	bool needHardReset = true;
	if (changed & SettingColors)
	{
		needHardReset = false;
	} else if (changed & SettingButtons) {
		// handled by KCommonDecoration
		needHardReset = false;
	}

	if (needHardReset) {
		return true;
	} else {
		resetDecorations(changed);
		return false;
	}
	return true;
}


bool QuartzHandler::supports( Ability ability )
{
    switch( ability )
    {
        case AbilityAnnounceButtons:
        case AbilityButtonMenu:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
		case AbilityButtonSpacer:
            return true;
        default:
            return false;
    };
}


void QuartzHandler::readConfig()
{
	KConfig conf("kwinquartzrc");
	conf.setGroup("General");
	coloredFrame = conf.readEntry( "UseTitleBarBorderColors", QVariant(true )).toBool();
	extraSlim    = conf.readEntry( "UseQuartzExtraSlim", QVariant(false )).toBool();

	// A small hack to make the on all desktops button look nicer
	onAllDesktopsButtonOnLeft = KDecoration::options()->titleButtonsLeft().contains( 'S' );
        if ( QApplication::isRightToLeft() )
            onAllDesktopsButtonOnLeft = !onAllDesktopsButtonOnLeft;
	switch(options()->preferredBorderSize(this)) {
	case BorderLarge:
		borderWidth = 8;
		break;
	case BorderVeryLarge:
		borderWidth = 12;
		break;
	case BorderHuge:
		borderWidth = 18;
		break;
	case BorderVeryHuge:
		borderWidth = 27;
		break;
	case BorderOversized:
		borderWidth = 40;
		break;
	case BorderTiny:
	case BorderNormal:
	default:
		borderWidth = extraSlim?2:4;
	}

	normalTitleHeight = QFontMetrics(options()->font(true)).height();
	int nTH_limit=extraSlim?14:18;
	normalTitleHeight = QFontMetrics(options()->font(true)).height()-(extraSlim?1:0);
	if (normalTitleHeight < nTH_limit) normalTitleHeight = nTH_limit;
	if (normalTitleHeight < borderWidth) normalTitleHeight = borderWidth;

	toolTitleHeight = QFontMetrics(options()->font(true, true)).height();
	if (toolTitleHeight < 12) toolTitleHeight = 12;
	if (toolTitleHeight < borderWidth) toolTitleHeight = borderWidth;
}


// This does the colour transition magic. (You say "Oh, is that it?")
// This may be made configurable at a later stage
void QuartzHandler::drawBlocks( KPixmap *pi, KPixmap &p, const QColor &c1, const QColor &c2 )
{
	QPainter px;

	px.begin( pi );

	// Draw a background gradient first
	KPixmapEffect::gradient(p, c1, c2, KPixmapEffect::HorizontalGradient);

	int factor = (pi->height()-2)/4;
	int square = factor - (factor+2)/4;

	int x = pi->width() - 5*factor - square;
	int y = (pi->height() - 4*factor)/2;

	px.fillRect( x, y,         square, square, c1.light(120) );
	px.fillRect( x, y+factor,  square, square, c1 );
	px.fillRect( x, y+2*factor, square, square, c1.light(110) );
	px.fillRect( x, y+3*factor, square, square, c1 );

	px.fillRect( x+factor, y,         square, square, c1.light(110) );
	px.fillRect( x+factor, y+factor,  square, square, c2.light(110) );
	px.fillRect( x+factor, y+2*factor, square, square, c1.light(120) );
	px.fillRect( x+factor, y+3*factor, square, square, c2.light(130) );

	px.fillRect( x+2*factor, y+factor,  square, square, c1.light(110) );
	px.fillRect( x+2*factor, y+2*factor, square, square, c2.light(120) );
	px.fillRect( x+2*factor, y+3*factor, square, square, c2.light(150) );

	px.fillRect( x+3*factor, y,         square, square, c1.dark(110) );
	px.fillRect( x+3*factor, y+2*factor, square, square, c2.light(120) );
	px.fillRect( x+3*factor, y+3*factor, square, square, c1.dark(120) );

	px.fillRect( x+4*factor, y+factor,  square, square, c1.light(110) );
	px.fillRect( x+4*factor, y+3*factor, square, square, c1.dark(110) );

	px.fillRect( x+5*factor, y+2*factor, square, square, c2.light(120));
	px.fillRect( x+5*factor, y+3*factor, square, square, c2.light(110) );
}


// This paints the button pixmaps upon loading the style.
void QuartzHandler::createPixmaps()
{
    // Obtain titlebar blend colours, and create the block stuff on pixmaps.
	QColorGroup g2 = options()->palette(ColorTitleBlend, true).active();
    QColor c2 = g2.background();
	g2 = options()->palette(ColorTitleBar, true ).active();
    QColor c = g2.background().light(130);

	titleBlocks = new KPixmap();
    titleBlocks->resize( normalTitleHeight*25/18, normalTitleHeight );
    drawBlocks( titleBlocks, *titleBlocks, c, c2 );

	g2 = options()->palette(ColorTitleBlend, false).active();
    c2 = g2.background();
	g2 = options()->palette(ColorTitleBar, false ).active();
    c = g2.background().light(130);

	ititleBlocks = new KPixmap();
    ititleBlocks->resize( normalTitleHeight*25/18, normalTitleHeight );
    drawBlocks( ititleBlocks, *ititleBlocks, c, c2 );

	// Set the on all desktops pin pixmaps;
	QColorGroup g;
	QPainter p;

	g = options()->palette( onAllDesktopsButtonOnLeft ? ColorTitleBar : ColorTitleBlend, true ).active();
	c = onAllDesktopsButtonOnLeft ? g.background().light(130) : g.background();
	g2 = options()->palette( ColorButtonBg, true ).active();

	pinUpPix = new KPixmap();
	pinUpPix->resize(16, 16);
	p.begin( pinUpPix );
	p.fillRect( 0, 0, 16, 16, c);
	kColorBitmaps( &p, g2, 0, 1, 16, 16, true, pinup_white_bits,
					pinup_gray_bits, NULL, NULL, pinup_dgray_bits, NULL );
	p.end();

	pinDownPix = new KPixmap();
	pinDownPix->resize(16, 16);
	p.begin( pinDownPix );
	p.fillRect( 0, 0, 16, 16, c);
	kColorBitmaps( &p, g2, 0, 1, 16, 16, true, pindown_white_bits,
					pindown_gray_bits, NULL, NULL, pindown_dgray_bits, NULL );
	p.end();


	// Inactive pins
	g = options()->palette( onAllDesktopsButtonOnLeft ? ColorTitleBar : ColorTitleBlend, false ).active();
	c = onAllDesktopsButtonOnLeft ? g.background().light(130) : g.background();
	g2 = options()->palette( ColorButtonBg, false ).active();

	ipinUpPix = new KPixmap();
	ipinUpPix->resize(16, 16);
	p.begin( ipinUpPix );
	p.fillRect( 0, 0, 16, 16, c);
	kColorBitmaps( &p, g2, 0, 1, 16, 16, true, pinup_white_bits,
					pinup_gray_bits, NULL, NULL, pinup_dgray_bits, NULL );
	p.end();

	ipinDownPix = new KPixmap();
	ipinDownPix->resize(16, 16);
	p.begin( ipinDownPix );
	p.fillRect( 0, 0, 16, 16, c);
	kColorBitmaps( &p, g2, 0, 1, 16, 16, true, pindown_white_bits,
					pindown_gray_bits, NULL, NULL, pindown_dgray_bits, NULL );
	p.end();
}


void QuartzHandler::freePixmaps()
{
    delete titleBlocks;
    delete ititleBlocks;

    // On all desktops pin images
    delete pinUpPix;
    delete ipinUpPix;
    delete pinDownPix;
    delete ipinDownPix;
}


QList< QuartzHandler::BorderSize > QuartzHandler::borderSizes() const
{ // the list must be sorted
  return QList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}


QuartzButton::QuartzButton(ButtonType type, QuartzClient *parent, const char *name)
    : KCommonDecorationButton(type, parent)
{
    // Eliminate any possible background flicker
    setBackgroundMode( Qt::NoBackground );

	deco = 0;
}


QuartzButton::~QuartzButton()
{
    delete deco;
}

void QuartzButton::reset(unsigned long changed)
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

void QuartzButton::setBitmap(const unsigned char *bitmap)
{
	delete deco;
	deco = 0;

	if (bitmap) {
		deco = new QBitmap(10, 10, bitmap, true);
		deco->setMask( *deco );
		repaint( false );
	}
}

void QuartzButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	drawButton(&p);
}

void QuartzButton::drawButton(QPainter *p)
{
	// Never paint if the pixmaps have not been created
	if (!quartz_initialized)
		return;

	QColor c;

	if (isLeft() )
		c = KDecoration::options()->color(KDecoration::ColorTitleBar, decoration()->isActive()).light(130);
	else
		c = KDecoration::options()->color(KDecoration::ColorTitleBlend, decoration()->isActive());

	// Fill the button background with an appropriate color
	p->fillRect(0, 0, width(), height(), c );

	// If we have a decoration bitmap, then draw that
	// otherwise we paint a menu button (with mini icon), or a onAllDesktops button.
	if( deco )
	{
		int xOff = (width()-10)/2;
		int yOff = (height()-10)/2;
		p->setPen( Qt::black );
		p->drawPixmap(isDown() ? xOff+2: xOff+1, isDown() ? yOff+2 : yOff+1, *deco);
		p->setPen( KDecoration::options()->color(KDecoration::ColorButtonBg, decoration()->isActive()).light(150) );
		p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, *deco);
	} else
		{
			QPixmap btnpix;
			int Offset = 0;

			if (type() == OnAllDesktopsButton)
			{
				if (isDown())
					Offset = 1;

				// Select the right onAllDesktops button to paint
				if (decoration()->isActive())
					btnpix = isChecked() ? *pinDownPix : *pinUpPix;
				else
					btnpix = isChecked() ? *ipinDownPix : *ipinUpPix;

			} else
				btnpix = decoration()->icon().pixmap( QIcon::Small, QIcon::Normal);

			// Shrink the miniIcon for tiny titlebars.
			if ( height() < 16)
			{
				QPixmap tmpPix;

				// Smooth scale the image
				tmpPix.convertFromImage( btnpix.convertToImage().scaled(height(), height(), Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
				p->drawPixmap( 0, 0, tmpPix );
			} else {
				Offset += (height() - 16)/2;
				p->drawPixmap( Offset, Offset, btnpix );
			}
	}
}

///////////////////////////////////////////////////////////////////////////

QuartzClient::QuartzClient(KDecorationBridge* bridge, KDecorationFactory* factory)
    : KCommonDecoration (bridge, factory)
{
}

QString QuartzClient::visibleName() const
{
	return i18n("Quartz");
}

QString QuartzClient::defaultButtonsLeft() const
{
	return "M";
}

QString QuartzClient::defaultButtonsRight() const
{
	return "HIAX";
}

bool QuartzClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
	switch (behaviour) {
		case DB_MenuClose:
			return false;

		case DB_WindowMask:
			return false;

		case DB_ButtonHide:
			return true;

		default:
			return KCommonDecoration::decorationBehaviour(behaviour);
	}
}

int QuartzClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
	bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();

	switch (lm) {
		case LM_BorderLeft:
		case LM_BorderRight:
		case LM_BorderBottom:
		case LM_TitleEdgeLeft:
		case LM_TitleEdgeRight:
		{
			if (respectWindowState && maximized) {
				return 0;
			} else {
				return borderSize;
			}
		}

		case LM_TitleEdgeTop:
			return borderSize-1;

		case LM_TitleEdgeBottom:
			return 1;

		case LM_TitleBorderLeft:
		case LM_TitleBorderRight:
			return 5;

		case LM_TitleHeight:
			return titleHeight;

		case LM_ButtonWidth:
		case LM_ButtonHeight:
			return titleHeight-2;

		case LM_ButtonSpacing:
			return 1;

		case LM_ExplicitButtonSpacer:
			return 3;

		default:
			return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
	}
}

KCommonDecorationButton *QuartzClient::createButton(ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new QuartzButton(MenuButton, this, "menu");

        case OnAllDesktopsButton:
            return new QuartzButton(OnAllDesktopsButton, this, "on_all_desktops");

        case HelpButton:
            return new QuartzButton(HelpButton, this, "help");

        case MinButton:
            return new QuartzButton(MinButton, this, "minimize");

        case MaxButton:
            return new QuartzButton(MaxButton, this, "maximize");

        case CloseButton:
            return new QuartzButton(CloseButton, this, "close");

        case AboveButton:
            return new QuartzButton(AboveButton, this, "above");

        case BelowButton:
            return new QuartzButton(BelowButton, this, "below");

        case ShadeButton:
            return new QuartzButton(ShadeButton, this, "shade");

        default:
            return 0;
    }
}


void QuartzClient::init()
{
	// Finally, toolWindows look small
	if ( isToolWindow() ) {
		titleHeight  = toolTitleHeight;
		largeButtons = false;
	}
	else {
		titleHeight = normalTitleHeight;
		largeButtons = true;
    }

    borderSize = borderWidth;

    KCommonDecoration::init();
}

void QuartzClient::reset( unsigned long changed )
{
	if (changed & SettingColors || changed & SettingFont)
	{
		// repaint the whole thing
		widget()->repaint(false);
	}

	KCommonDecoration::reset(changed);
}


// Quartz Paint magic goes here.
void QuartzClient::paintEvent( QPaintEvent* )
{
	// Never paint if the pixmaps have not been created
	if (!quartz_initialized)
		return;

	const bool maxFull = (maximizeMode()==MaximizeFull) && !options()->moveResizeMaximizedWindows();

	QColorGroup g;
    QPainter p(widget());

    // Obtain widget bounds.
    QRect r(widget()->rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width() - 1;
    int y2 = r.height() - 1;
    int w  = r.width();
    int h  = r.height();

    // Draw part of the frame that is the title color

	if( coloredFrame )
		g = options()->palette(ColorTitleBar, isActive()).active();
	else
		g = options()->palette(ColorFrame, isActive()).active();

    // Draw outer highlights and lowlights
    p.setPen( g.light().light(120) );
    p.drawLine( x, y, x2-1, y );
    p.drawLine( x, y+1, x, y2-1 );
    p.setPen( g.dark().light(120) );
    p.drawLine( x2, y, x2, y2 );
    p.drawLine( x, y2, x2, y2 );

    // Fill out the border edges
	QColor frameColor;
	if ( coloredFrame)
		frameColor = g.background().light(130);
	else
		frameColor = g.background();
	if (borderSize > 2) {
		p.fillRect(x+1, y+1, w-2, borderSize-2, frameColor); // top
		if (!maxFull) {
			p.fillRect(x+1, y+h-(borderSize-1), w-2, borderSize-2, frameColor); // bottom
			p.fillRect(x+1, y+borderSize-1, borderSize-1, h-2*(borderSize-1), frameColor); // left
			p.fillRect(x+w-(borderSize), y+borderSize-1, borderSize-1, h-2*(borderSize-1), frameColor); // right
		}
	}

    // Draw a frame around the wrapped widget.
    p.setPen( g.background() );
	if (maxFull) {
		p.drawLine(x+1, y+titleHeight+(borderSize-1), w-2, y+titleHeight+(borderSize-1));
	} else {
		p.drawRect( x+(borderSize-1), y+titleHeight+(borderSize-1), w-2*(borderSize-1), h-titleHeight-2*(borderSize-1) );
	}

	// Drawing this extra line removes non-drawn areas when shaded
	p.drawLine( x+borderSize, y2-borderSize, x2-borderSize, y2-borderSize);

    // Highlight top corner
    p.setPen( g.light().light(160) );
    p.drawPoint( x, y );
    p.setPen( g.light().light(140) );
    p.drawPoint( x+1, y );
    p.drawPoint( x, y+1 );

    // Draw the title bar.
    // ===================
    int r_x, r_y, r_x2, r_y2;
    widget()->rect().getCoords(&r_x, &r_y, &r_x2, &r_y2);
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
    const int titleEdgeRight = layoutMetric(LM_TitleEdgeRight);
    const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);
    const int ttlHeight = layoutMetric(LM_TitleHeight);
    const int titleEdgeBottomBottom = r_y+titleEdgeTop+ttlHeight+titleEdgeBottom-1;
    r = QRect(r_x+titleEdgeLeft+buttonsLeftWidth(), r_y+titleEdgeTop,
              r_x2-titleEdgeRight-buttonsRightWidth()-(r_x+titleEdgeLeft+buttonsLeftWidth()),
              titleEdgeBottomBottom-(r_y+titleEdgeTop) );

    // Obtain titlebar blend colours
    QColor c1 = options()->color(ColorTitleBar, isActive() ).light(130);
    QColor c2 = options()->color(ColorTitleBlend, isActive() );

    // Create a disposable pixmap buffer for the titlebar
    KPixmap* titleBuffer = new KPixmap;
    titleBuffer->resize( maxFull?w-2:(w-2*(borderSize-1)), titleHeight );

    QPainter p2( titleBuffer );

	// subtract titleBlocks pixmap width and some
	int rightoffset = r.x()+r.width()-titleBlocks->width()-borderSize;

    p2.fillRect( 0, 0, w, r.height(), c1 );
    p2.fillRect( rightoffset, 0, maxFull?w-rightoffset:w-rightoffset-2*(borderSize-1), r.height(), c2 );

    // 8 bit displays will be a bit dithered, but they still look ok.
    if ( isActive() )
		p2.drawPixmap( rightoffset, 0, *titleBlocks );
	else
		p2.drawPixmap( rightoffset, 0, *ititleBlocks );

	// Draw the title text on the pixmap, and with a smaller font
	// for toolwindows than the default.
	QFont fnt;
	if ( largeButtons ) {
		fnt = options()->font(true, false); // font not small
	} else {
		fnt = options()->font(true, true); // font small
		fnt.setWeight( QFont::Normal ); // and disable bold
	}
    p2.setFont( fnt );

    p2.setPen( options()->color(ColorFont, isActive() ));
    p2.drawText(r.x()+4-borderSize, 0, r.width()-3, r.height(),
                Qt::AlignLeft | Qt::AlignVCenter, caption() );
    p2.end();

    p.drawPixmap( maxFull?1:borderSize-1, borderSize-1, *titleBuffer );

    delete titleBuffer;
}

}

// Extended KWin plugin interface
/////////////////////////////////
extern "C"
{
	KDE_EXPORT KDecorationFactory *create_factory()
	{
		Quartz::clientHandler = new Quartz::QuartzHandler();
		return Quartz::clientHandler;
	}
}



#include "quartz.moc"
// vim: ts=4
// kate: space-indent off; tab-width 4;
