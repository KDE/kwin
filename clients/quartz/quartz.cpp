/*
 *
 * Gallium-Quartz KWin client
 *
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
#include <qcursor.h>
#include <qdrawutil.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qapplication.h>

#include "quartz.h"


namespace Quartz {

static unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00,
  0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

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


///////////////////////////////////////////////////////////////////////////

// Titlebar button positions
bool onAllDesktopsButtonOnLeft = true;
bool coloredFrame		= true;

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
	}

	if (needHardReset) {
		return true;
	} else {
		resetDecorations(changed);
		return false;
	}
	return true;
}


void QuartzHandler::readConfig()
{
	KConfig conf("kwinquartzrc");
	conf.setGroup("General");
	coloredFrame = conf.readBoolEntry( "UseTitleBarBorderColors", true );

	// A small hack to make the on all desktops button look nicer
	onAllDesktopsButtonOnLeft = KDecoration::options()->titleButtonsLeft().contains( 'S' );
        if ( QApplication::reverseLayout() )
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
		borderWidth = 4;
	}

	normalTitleHeight = QFontMetrics(options()->font(true)).height();
	if (normalTitleHeight < 18) normalTitleHeight = 18;
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

	px.fillRect( x, y,          square, square, c1.light(120) );
	px.fillRect( x, y+factor,   square, square, c1 );
	px.fillRect( x, y+2*factor, square, square, c1.light(110) );
	px.fillRect( x, y+3*factor, square, square, c1 );

	px.fillRect( x+factor, y,          square, square, c1.light(110) );
	px.fillRect( x+factor, y+factor,   square, square, c2.light(110) );
	px.fillRect( x+factor, y+2*factor, square, square, c1.light(120) );
	px.fillRect( x+factor, y+3*factor, square, square, c2.light(130) );

	px.fillRect( x+2*factor, y+factor,   square, square, c1.light(110) );
	px.fillRect( x+2*factor, y+2*factor, square, square, c2.light(120) );
	px.fillRect( x+2*factor, y+3*factor, square, square, c2.light(150) );

	px.fillRect( x+3*factor, y,          square, square, c1.dark(110) );
	px.fillRect( x+3*factor, y+2*factor, square, square, c2.light(120) );
	px.fillRect( x+3*factor, y+3*factor, square, square, c1.dark(120) );

	px.fillRect( x+4*factor, y+factor,   square, square, c1.light(110) );
	px.fillRect( x+4*factor, y+3*factor, square, square, c1.dark(110) );

	px.fillRect( x+5*factor, y+2*factor, square, square, c2.light(120));
	px.fillRect( x+5*factor, y+3*factor, square, square, c2.light(110) );
}


// This paints the button pixmaps upon loading the style.
void QuartzHandler::createPixmaps()
{
    // Obtain titlebar blend colours, and create the block stuff on pixmaps.
    QColorGroup g2 = options()->colorGroup(ColorTitleBlend, true);
    QColor c2 = g2.background();
    g2 = options()->colorGroup(ColorTitleBar, true );
    QColor c = g2.background().light(130);

	titleBlocks = new KPixmap();
    titleBlocks->resize( normalTitleHeight*25/18, normalTitleHeight );
    drawBlocks( titleBlocks, *titleBlocks, c, c2 );

    g2 = options()->colorGroup(ColorTitleBlend, false);
    c2 = g2.background();
    g2 = options()->colorGroup(ColorTitleBar, false );
    c = g2.background().light(130);

	ititleBlocks = new KPixmap();
    ititleBlocks->resize( normalTitleHeight*25/18, normalTitleHeight );
    drawBlocks( ititleBlocks, *ititleBlocks, c, c2 );

	// Set the on all desktops pin pixmaps;
	QColorGroup g;
	QPainter p;

	g = options()->colorGroup( onAllDesktopsButtonOnLeft ? ColorTitleBar : ColorTitleBlend, true );
	c = onAllDesktopsButtonOnLeft ? g.background().light(130) : g.background();
	g2 = options()->colorGroup( ColorButtonBg, true );

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
	g = options()->colorGroup( onAllDesktopsButtonOnLeft ? ColorTitleBar : ColorTitleBlend, false );
	c = onAllDesktopsButtonOnLeft ? g.background().light(130) : g.background();
	g2 = options()->colorGroup( ColorButtonBg, false );

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


QValueList< QuartzHandler::BorderSize > QuartzHandler::borderSizes() const
{ // the list must be sorted
  return QValueList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}


QuartzButton::QuartzButton(QuartzClient *parent, const char *name, bool largeButton,
		bool isLeftButton, bool isOnAllDesktopsButton, const unsigned char *bitmap,
		const QString& tip, const int realizeBtns)
    : QButton(parent->widget(), name)
{
	setTipText(tip);
	setCursor(ArrowCursor);

    // Eliminate any possible background flicker
    setBackgroundMode( QWidget::NoBackground );
	setToggleButton( isOnAllDesktopsButton );

	realizeButtons = realizeBtns;

	deco 	 = NULL;
        large 	 = largeButton;
        if ( QApplication::reverseLayout() )
             isLeft 	 = !isLeftButton;
         else
             isLeft 	 = isLeftButton;
	isOnAllDesktops = isOnAllDesktopsButton;
	client   = parent;

    if ( large )
       setFixedSize(normalTitleHeight-2, normalTitleHeight-2);
    else
       setFixedSize(toolTitleHeight-2, toolTitleHeight-2);

    if(bitmap)
        setBitmap(bitmap);
}


QuartzButton::~QuartzButton()
{
    delete deco;
}


QSize QuartzButton::sizeHint() const
{
   if ( large )
      return( QSize(normalTitleHeight-2, normalTitleHeight-2) );
   else
      return( QSize(toolTitleHeight-2, toolTitleHeight-2) );
}


void QuartzButton::setBitmap(const unsigned char *bitmap)
{
    delete deco;

    deco = new QBitmap(10, 10, bitmap, true);
    deco->setMask( *deco );
    repaint( false );
}


void QuartzButton::setTipText(const QString &tip) {
	if(KDecoration::options()->showTooltips()) {
		QToolTip::remove(this );
		QToolTip::add(this, tip );
	}
}


void QuartzButton::drawButton(QPainter *p)
{
	// Never paint if the pixmaps have not been created
	if (!quartz_initialized)
		return;

	QColor c;

	if (isLeft)
		c = KDecoration::options()->color(KDecoration::ColorTitleBar, client->isActive()).light(130);
	else
		c = KDecoration::options()->color(KDecoration::ColorTitleBlend, client->isActive());

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
		p->setPen( KDecoration::options()->color(KDecoration::ColorButtonBg, client->isActive()).light(150) );
		p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, *deco);
	} else
		{
			QPixmap btnpix;
			int Offset = 0;

			if (isOnAllDesktops)
			{
				if (isDown())
					Offset = 1;

				// Select the right onAllDesktops button to paint
				if (client->isActive())
					btnpix = isOn() ? *pinDownPix : *pinUpPix;
				else
					btnpix = isOn() ? *ipinDownPix : *ipinUpPix;

			} else
				btnpix = client->icon().pixmap( QIconSet::Small, QIconSet::Normal);

			// Shrink the miniIcon for tiny titlebars.
			if ( height() < 16)
			{
				QPixmap tmpPix;

				// Smooth scale the image
				tmpPix.convertFromImage( btnpix.convertToImage().smoothScale(height(), height()));
				p->drawPixmap( 0, 0, tmpPix );
			} else {
				Offset += (height() - 16)/2;
				p->drawPixmap( Offset, Offset, btnpix );
			}
	}
}


// Make the protected member public
void QuartzButton::turnOn( bool isOn )
{
	if ( isToggleButton() )
		setOn( isOn );
}


void QuartzButton::mousePressEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					(e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mousePressEvent( &me );
}


void QuartzButton::mouseReleaseEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					(e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mouseReleaseEvent( &me );
}


///////////////////////////////////////////////////////////////////////////

QuartzClient::QuartzClient(KDecorationBridge* bridge, KDecorationFactory* factory)
    : KDecoration (bridge, factory)
{
}


void QuartzClient::init()
{
	createMainWidget(WNoAutoErase | WStaticContents);

	widget()->installEventFilter( this );

	// No flicker thanks
	widget()->setBackgroundMode( QWidget::NoBackground );

	// Set button pointers to NULL so we can track things
	for(int i=0; i < QuartzClient::BtnCount; i++)
		button[i] = NULL;

	// Finally, toolWindows look small
	if ( isTool() ) {
		titleHeight  = toolTitleHeight;
		largeButtons = false;
	}
	else {
		titleHeight = normalTitleHeight;
		largeButtons = true;
    }

    borderSize = borderWidth;

    // Pack the fake window window within a grid
    QGridLayout* g = new QGridLayout(widget(), 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, borderSize-1);       // Top grab bar
    if( isPreview())
	g->addWidget(new QLabel( i18n( "<center><b>Quartz</b></center>" ), widget()), 3, 1);
    else
	g->addItem(new QSpacerItem( 0, 0 ), 3, 1); // no widget in the middle

	// without the next line, unshade flickers
	g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed,
				QSizePolicy::Expanding ) );
    g->setRowStretch(3, 10);      // Wrapped window
    g->addRowSpacing(2, 1);       // line under titlebar
    g->addRowSpacing(4, borderSize);       // bottom handles
    g->addColSpacing(0, borderSize);
    g->addColSpacing(2, borderSize);

    // Pack the titlebar HBox with items
    hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0);
    hb->setResizeMode( QLayout::FreeResize );
    g->addLayout ( hb, 1, 1 );

	addClientButtons( options()->titleButtonsLeft() );

    titlebar = new QSpacerItem( 10, titleHeight, QSizePolicy::Expanding, QSizePolicy::Minimum );
    hb->addItem(titlebar);
    hb->addSpacing(2);

	addClientButtons( options()->titleButtonsRight(), false );

    hb->addSpacing(2);
}

void QuartzClient::reset( unsigned long changed )
{
	if (changed & SettingColors || changed & SettingFont)
	{
		// repaint the whole thing
		widget()->repaint(false);
	}
}

bool QuartzClient::isTool()
{
	NET::WindowType type = windowType(NET::NormalMask|NET::ToolbarMask|NET::UtilityMask|NET::MenuMask);
	return ((type==NET::Toolbar)||(type==NET::NET::Utility)||(type==NET::Menu));
}


void QuartzClient::addClientButtons( const QString& s, bool isLeft )
{
	if (s.length() > 0)
		for(unsigned int i = 0; i < s.length(); i++)
		{
			switch( s[i].latin1() )
			{
				// Menu button
				case 'M':
					if (!button[BtnMenu])
					{
    					button[BtnMenu] = new QuartzButton(this, "menu",
								 largeButtons, isLeft, false, NULL, i18n("Menu"), LeftButton|RightButton);
    					connect( button[BtnMenu], SIGNAL(pressed()),
								 this, SLOT(menuButtonPressed()) );
						hb->addWidget( button[BtnMenu] );
					}
					break;

				// On all desktops button
				case 'S':
					if (!button[BtnOnAllDesktops])
					{
    					button[BtnOnAllDesktops] = new QuartzButton(this, "on_all_desktops",
								 largeButtons, isLeft, true, NULL, i18n("On All Desktops"));
						button[BtnOnAllDesktops]->turnOn( isOnAllDesktops() );
    					connect( button[BtnOnAllDesktops], SIGNAL(clicked()),
								 this, SLOT(toggleOnAllDesktops()) );
    					hb->addSpacing(1);
						hb->addWidget( button[BtnOnAllDesktops] );
    					hb->addSpacing(1);
					}
					break;

				// Help button
				case 'H':
    				if( providesContextHelp() && (!button[BtnHelp]) )
					{
						button[BtnHelp] = new QuartzButton(this, "help",
								 largeButtons, isLeft, true, question_bits, i18n("Help"));
						connect( button[BtnHelp], SIGNAL( clicked() ),
								 this, SLOT(showContextHelp()));
						hb->addWidget( button[BtnHelp] );
					}
					break;

				// Minimize button
				case 'I':
					if ( (!button[BtnIconify]) && isMinimizable())
					{
					    button[BtnIconify] = new QuartzButton(this, "iconify",
								 largeButtons, isLeft, true, iconify_bits, i18n("Minimize"));
					    connect( button[BtnIconify], SIGNAL( clicked()),
								 this, SLOT(minimize()) );
						hb->addWidget( button[BtnIconify] );
					}
					break;

				// Maximize button
				case 'A':
					if ( (!button[BtnMax]) && isMaximizable())
					{
					    button[BtnMax]  = new QuartzButton(this, "maximize",
 								 largeButtons, isLeft, true, maximize_bits, i18n("Maximize"), LeftButton|MidButton|RightButton);
					    connect( button[BtnMax], SIGNAL( clicked()),
								 this, SLOT(slotMaximize()) );
						hb->addWidget( button[BtnMax] );
					}
					break;

				// Close button
				case 'X':
					if (!button[BtnClose] && isCloseable())
					{
		    			button[BtnClose] = new QuartzButton(this, "close",
								 largeButtons, isLeft, true, close_bits, i18n("Close"));
					    connect( button[BtnClose], SIGNAL( clicked()),
								 this, SLOT(closeWindow()) );
						hb->addWidget( button[BtnClose] );
					}
			}
		}
}


void QuartzClient::iconChange()
{
	if (button[BtnMenu] && button[BtnMenu]->isVisible())
		button[BtnMenu]->repaint(false);
}


void QuartzClient::desktopChange()
{
	if (button[BtnOnAllDesktops])
	{
		button[BtnOnAllDesktops]->turnOn(isOnAllDesktops());
		button[BtnOnAllDesktops]->repaint(false);
		button[BtnOnAllDesktops]->setTipText(isOnAllDesktops() ? i18n("Not On All Desktops") : i18n("On All Desktops"));
	}
}


void QuartzClient::slotMaximize()
{
	if (button[BtnMax])
	{
		switch (button[BtnMax]->last_button)
		{
			case MidButton:
				maximize(maximizeMode() ^ MaximizeVertical );
				break;
			case RightButton:
				maximize(maximizeMode() ^ MaximizeHorizontal );
				break;
			default:
				maximize(maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull );
		}
	}
}


void QuartzClient::resizeEvent( QResizeEvent* e)
{
    calcHiddenButtons();

    if (widget()->isVisibleToTLW())
    {
        widget()->update(widget()->rect());
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
	       widget()->update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	       widget()->update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
           // Titlebar needs no paint event
           widget()->repaint(titlebar->geometry(), false);
        }
    }
}


void QuartzClient::captionChange()
{
    widget()->repaint( titlebar->geometry(), false );
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
    	g = options()->colorGroup(ColorTitleBar, isActive());
	else
		g = options()->colorGroup(ColorFrame, isActive());

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
    r = titlebar->geometry();

    // Obtain titlebar blend colours
    QColor c1 = options()->color(ColorTitleBar, isActive() ).light(130);
    QColor c2 = options()->color(ColorTitleBlend, isActive() );

    // Create a disposable pixmap buffer for the titlebar
    KPixmap* titleBuffer = new KPixmap;
    titleBuffer->resize( maxFull?w-2:(w-2*(borderSize-1)), titleHeight );

    QPainter p2( titleBuffer, this );

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
                AlignLeft | AlignVCenter, caption() );
    p2.end();

    p.drawPixmap( maxFull?1:borderSize-1, borderSize-1, *titleBuffer );

    delete titleBuffer;
}


void QuartzClient::showEvent(QShowEvent *)
{
    calcHiddenButtons();
    widget()->show();
}


void QuartzClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
       titlebarDblClickOperation();
}


void QuartzClient::maximizeChange()
{
	if (button[BtnMax]) {
	    button[BtnMax]->setBitmap((maximizeMode()==MaximizeFull) ? minmax_bits : maximize_bits);
		button[BtnMax]->setTipText((maximizeMode()==MaximizeFull) ? i18n("Restore") : i18n("Maximize"));
	}
}


void QuartzClient::activeChange()
{
	for(int i=QuartzClient::BtnHelp; i < QuartzClient::BtnCount; i++)
		if(button[i])
			button[i]->repaint(false);

	widget()->repaint(false);
}


QuartzClient::Position QuartzClient::mousePosition(const QPoint &point) const
{
	const int corner = 3*borderSize/2 + 18;
	Position pos = PositionCenter;

	QRect r(widget()->rect());

	if(point.y() < (borderSize-1)) {
		if(point.x() < corner)                   return PositionTopLeft;
		else if(point.x() > (r.right()-corner))  return PositionTopRight;
		else                                     return PositionTop;
	} else if(point.y() > (r.bottom()-borderSize)) {
		if(point.x() < corner)                   return PositionBottomLeft;
		else if(point.x() > (r.right()-corner))  return PositionBottomRight;
		else                                     return PositionBottom;
	} else if(point.x() < borderSize) {
		if(point.y() < corner)                   return PositionTopLeft;
		else if(point.y() > (r.bottom()-corner)) return PositionBottomLeft;
		else                                     return PositionLeft;
	} else if(point.x() > (r.right()-borderSize)) {
		if(point.y() < corner)                   return PositionTopRight;
		else if(point.y() > (r.bottom()-corner)) return PositionBottomRight;
		else                                     return PositionRight;
	}

	return pos;
}


void QuartzClient::borders(int& left, int& right, int& top, int& bottom) const
{
    left = borderSize;
    right = borderSize;
    top =  1 + titleHeight + (borderSize-1);
    bottom = borderSize;

    if ((maximizeMode()==MaximizeFull) && !options()->moveResizeMaximizedWindows()) {
        left = right = bottom = 0;
        top = 1 + titleHeight + (borderSize-1);
    }
}


void QuartzClient::resize( const QSize& s )
{
    widget()->resize( s );
}


QSize QuartzClient::minimumSize() const
{
    return widget()->minimumSize();
}


// The hiding button while shrinking, show button while expanding magic
void QuartzClient::calcHiddenButtons()
{
	//Hide buttons in this order - OnAllDesktops, Help, Maximize, Menu, Minimize, Close.
	QuartzButton* btnArray[] = { button[BtnOnAllDesktops], button[BtnHelp], button[BtnMax],
								 button[BtnMenu], button[BtnIconify], button[BtnClose] };

	int minwidth  = largeButtons ? 180 : 140;	// Start hiding buttons at this width
	int btn_width = largeButtons ? 16 : 10;
	int current_width = width();
	int count = 0;
	int i;

	// Find out how many buttons we have to hide.
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


// Make sure the menu button follows double click conventions set in kcontrol
void QuartzClient::menuButtonPressed()
{
    QPoint menupoint ( button[BtnMenu]->rect().bottomLeft().x()-1,
                       button[BtnMenu]->rect().bottomLeft().y()+2 );
	menupoint = button[BtnMenu]->mapToGlobal( menupoint );
        KDecorationFactory* f = factory();
        showWindowMenu(menupoint);
        if( !f->exists( this )) // 'this' was destroyed
            return;
	button[BtnMenu]->setDown(false);
}

bool QuartzClient::eventFilter( QObject* o, QEvent* e )
{
    if( o != widget())
    return false;
    switch( e->type())
    {
    case QEvent::Resize:
        resizeEvent(static_cast< QResizeEvent* >( e ) );
        return true;
    case QEvent::Paint:
        paintEvent(static_cast< QPaintEvent* >( e ) );
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast< QMouseEvent* >( e ) );
        return true;
    case QEvent::MouseButtonPress:
        processMousePressEvent(static_cast< QMouseEvent* >( e ) );
        return true;
    default:
        break;
    }
    return false;
}

}

// Extended KWin plugin interface
/////////////////////////////////
extern "C"
{
	KDecorationFactory *create_factory()
	{
		Quartz::clientHandler = new Quartz::QuartzHandler();
		return Quartz::clientHandler;
	}
}



#include "quartz.moc"
// vim: ts=4
