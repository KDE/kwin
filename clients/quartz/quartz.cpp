/*
 * $Id$
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
#include <kglobal.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <qlayout.h>
#include <qdrawutil.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"
#include "quartz.h"
#include <qimage.h>


using namespace KWinInternal;

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
bool stickyButtonOnLeft = true;
bool coloredFrame		= true;

KPixmap* titleBlocks 	= NULL;
KPixmap* ititleBlocks 	= NULL;
KPixmap* pinDownPix		= NULL;
KPixmap* pinUpPix		= NULL;
KPixmap* ipinDownPix	= NULL;
KPixmap* ipinUpPix		= NULL;

bool quartz_initialized = false;
QuartzHandler* clientHandler;

///////////////////////////////////////////////////////////////////////////


QuartzHandler::QuartzHandler()
{
	quartz_initialized = false;
	readConfig();
	createPixmaps();
	quartz_initialized = true;
	connect( options, SIGNAL(resetClients()), this, SLOT(slotReset()) );
}


QuartzHandler::~QuartzHandler()
{
	quartz_initialized = false;
	freePixmaps();
}


void QuartzHandler::slotReset()
{
	quartz_initialized = false;
	freePixmaps();
	readConfig();
	createPixmaps();
	quartz_initialized = true;

	// Make kwin create new clients for each window
	Workspace::self()->slotResetAllClientsDelayed();
}


void QuartzHandler::readConfig()
{
	KConfig conf("kwinquartzrc");
	conf.setGroup("General");
	coloredFrame = conf.readBoolEntry( "UseTitleBarBorderColors", true );

	// A small hack to make the sticky button look nicer
	stickyButtonOnLeft = (bool)options->titleButtonsLeft().contains( 'S' );
}


// This does the colour transition magic. (You say "Oh, is that it?")
// This may be made configurable at a later stage
void QuartzHandler::drawBlocks( KPixmap *pi, KPixmap &p, const QColor &c1, const QColor &c2 )
{
	QPainter px;

	px.begin( pi );

	// Draw a background gradient first
	KPixmapEffect::gradient(p, c1, c2, KPixmapEffect::HorizontalGradient);

	px.fillRect( 2, 1, 3, 3, c1.light(120) );
	px.fillRect( 2, 5, 3, 3, c1 );
	px.fillRect( 2, 9, 3, 3, c1.light(110) );
	px.fillRect( 2, 13, 3, 3, c1 );

	px.fillRect( 6, 1, 3, 3, c1.light(110) );
	px.fillRect( 6, 5, 3, 3, c2.light(110) );
	px.fillRect( 6, 9, 3, 3, c1.light(120) );
	px.fillRect( 6, 13, 3, 3, c2.light(130) );

	px.fillRect( 10, 5, 3, 3, c1.light(110) );
	px.fillRect( 10, 9, 3, 3, c2.light(120) );
	px.fillRect( 10, 13, 3, 3, c2.light(150) );

	px.fillRect( 14, 1, 3, 3, c1.dark(110) );
	px.fillRect( 14, 9, 3, 3, c2.light(120) );
	px.fillRect( 14, 13, 3, 3, c1.dark(120) );

	px.fillRect( 18, 5, 3, 3, c1.light(110) );
	px.fillRect( 18, 13, 3, 3, c1.dark(110) );

	px.fillRect( 22, 9, 3, 3, c2.light(120));
	px.fillRect( 22, 13, 3, 3, c2.light(110) );
}


// This paints the button pixmaps upon loading the style.
void QuartzHandler::createPixmaps()
{
    // Obtain titlebar blend colours, and create the block stuff on pixmaps.
    QColorGroup g2 = options->colorGroup(Options::TitleBlend, true);
    QColor c2 = g2.background();
    g2 = options->colorGroup(Options::TitleBar, true );
    QColor c = g2.background().light(130);

	titleBlocks = new KPixmap();
    titleBlocks->resize( 25, 18 );
    drawBlocks( titleBlocks, *titleBlocks, c, c2 );

    g2 = options->colorGroup(Options::TitleBlend, false);
    c2 = g2.background();
    g2 = options->colorGroup(Options::TitleBar, false );
    c = g2.background().light(130);

	ititleBlocks = new KPixmap();
    ititleBlocks->resize( 25, 18 );
    drawBlocks( ititleBlocks, *ititleBlocks, c, c2 );

	// Set the sticky pin pixmaps;
	QColorGroup g;
	QPainter p;

	g = options->colorGroup( stickyButtonOnLeft ? Options::TitleBar : Options::TitleBlend, true );
	c = stickyButtonOnLeft ? g.background().light(130) : g.background();
	g2 = options->colorGroup( Options::ButtonBg, true );

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
	g = options->colorGroup( stickyButtonOnLeft ? Options::TitleBar : Options::TitleBlend, false );
	c = stickyButtonOnLeft ? g.background().light(130) : g.background();
	g2 = options->colorGroup( Options::ButtonBg, false );

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
	// Title images
	if (titleBlocks)
		delete titleBlocks;
	if (ititleBlocks)
		delete ititleBlocks;

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


QuartzButton::QuartzButton(Client *parent, const char *name, bool largeButton,
		bool isLeftButton, bool isStickyButton, const unsigned char *bitmap,
		const QString& tip)
    : KWinButton(parent, name, tip)
{
    // Eliminate any possible background flicker
    setBackgroundMode( QWidget::NoBackground ); 
	setToggleButton( isStickyButton );

	deco 	 = NULL;
    large 	 = largeButton;
	isLeft 	 = isLeftButton;
	isSticky = isStickyButton;
	client   = parent;

    if ( large )
       setFixedSize(16, 16);
    else
       setFixedSize(10, 10);

    if(bitmap)
        setBitmap(bitmap);
}


QuartzButton::~QuartzButton()
{
	if (deco)
		delete deco;
}


QSize QuartzButton::sizeHint() const
{
   if ( large )
      return( QSize(16,16) );
   else
      return( QSize(10,10) );
}


void QuartzButton::setBitmap(const unsigned char *bitmap)
{
	if (deco)
		delete deco;

    deco = new QBitmap(10, 10, bitmap, true);
    deco->setMask( *deco );
    repaint( false );   
}


void QuartzButton::drawButton(QPainter *p)
{
	// Never paint if the pixmaps have not been created
	if (!quartz_initialized)
		return;

	QColor c;

	if (isLeft)
		c = options->color(Options::TitleBar, client->isActive()).light(130);
	else
		c = options->color(Options::TitleBlend, client->isActive());

	// Fill the button background with an appropriate color
	p->fillRect(0, 0, width(), height(), c );

	// If we have a decoration bitmap, then draw that
	// otherwise we paint a menu button (with mini icon), or a sticky button.
	if( deco )
	{
		int xOff = (width()-10)/2;
		int yOff = (height()-10)/2;
		p->setPen( Qt::black );
		p->drawPixmap(isDown() ? xOff+2: xOff+1, isDown() ? yOff+2 : yOff+1, *deco);
		p->setPen( options->color(Options::ButtonBg, client->isActive()).light(150) );
		p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, *deco);
	} else 
		{
			QPixmap btnpix;
			int Offset = 0;

			if (isSticky)
			{
				if (isDown())
					Offset = 1;

				// Select the right sticky button to paint
				if (client->isActive())
					btnpix = isOn() ? *pinDownPix : *pinUpPix;
				else
					btnpix = isOn() ? *ipinDownPix : *ipinUpPix;

			} else
				btnpix = client->miniIcon();

			// Shrink the miniIcon for tiny titlebars.
			if ( !large )
			{
				QPixmap tmpPix;

				// Smooth scale the image
				tmpPix.convertFromImage( btnpix.convertToImage().smoothScale(10, 10));
				p->drawPixmap( 0, 0, tmpPix );
			} else
				p->drawPixmap( Offset, Offset, btnpix );       
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
					LeftButton, e->state() );
	KWinButton::mousePressEvent( &me );
}


void QuartzButton::mouseReleaseEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(),
					LeftButton, e->state() );
	KWinButton::mouseReleaseEvent( &me );
}


///////////////////////////////////////////////////////////////////////////

QuartzClient::QuartzClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WStaticContents | WRepaintNoErase )
{
	// No flicker thanks
    setBackgroundMode( QWidget::NoBackground );

	// Set button pointers to NULL so we can track things
	for(int i=0; i < QuartzClient::BtnCount; i++)
		button[i] = NULL;

    // Finally, toolWindows look small
    if ( isTool() ) {
		titleHeight  = 12; 
		largeButtons = false;
	} 
	else {
		titleHeight = 18;    
		largeButtons = true;
    }

    // Pack the windowWrapper() window within a grid
    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);       // Top grab bar 
	g->addWidget(windowWrapper(), 3, 1);
	// without the next line, unshade flickers
	g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, 
				QSizePolicy::Expanding ) );
    g->setRowStretch(3, 10);      // Wrapped window
    g->addRowSpacing(2, 1);       // line under titlebar
    g->addRowSpacing(4, 4);       // bottom handles
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    // Pack the titlebar HBox with items
    hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0);
    hb->setResizeMode( QLayout::FreeResize );
    g->addLayout ( hb, 1, 1 );

	addClientButtons( options->titleButtonsLeft() );

    titlebar = new QSpacerItem( 10, titleHeight, QSizePolicy::Expanding, QSizePolicy::Minimum );
    hb->addItem(titlebar);
    hb->addSpacing(2);

	addClientButtons( options->titleButtonsRight(), false );

    hb->addSpacing(2);
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
    					button[BtnSticky] = new QuartzButton(this, "sticky", 
								 largeButtons, isLeft, true, NULL, i18n("Sticky"));
						button[BtnSticky]->turnOn( isSticky() );
    					connect( button[BtnSticky], SIGNAL(clicked()),
								 this, SLOT(toggleSticky()) );
    					hb->addSpacing(1);
						hb->addWidget( button[BtnSticky] );
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
								 this, SLOT(contextHelp()));
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
								 this, SLOT(iconify()) );
						hb->addWidget( button[BtnIconify] );
					}
					break;

				// Maximize button
				case 'A':
					if ( (!button[BtnMax]) && isMaximizable())
					{
					    button[BtnMax]  = new QuartzButton(this, "maximize", 
 								 largeButtons, isLeft, true, maximize_bits, i18n("Maximize"));
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


void QuartzClient::stickyChange(bool on)
{
	if (button[BtnSticky])
	{
		button[BtnSticky]->turnOn(on);
		button[BtnSticky]->repaint(false);		
		button[BtnSticky]->setTipText(on ? i18n("Un-Sticky") : i18n("Sticky"));
	}
}


void QuartzClient::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
	   maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
       maximize( MaximizeHorizontal );
    else
	   maximize();
}


void QuartzClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

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
	       update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	       update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
           // Titlebar needs no paint event
	       QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
        }
    } 
}


void QuartzClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


// Quartz Paint magic goes here.
void QuartzClient::paintEvent( QPaintEvent* )
{
	// Never paint if the pixmaps have not been created
	if (!quartz_initialized)
		return;	

	QColorGroup g;
    QPainter p(this);

    // Obtain widget bounds.
    QRect r(rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width() - 1;
    int y2 = r.height() - 1;
    int w  = r.width();
    int h  = r.height();

    // Draw part of the frame that is the title color

	if( coloredFrame )
    	g = options->colorGroup(Options::TitleBar, isActive());
	else
		g = options->colorGroup(Options::Frame, isActive());
 
    // Draw outer highlights and lowlights
    p.setPen( g.light().light(120) );
    p.drawLine( x, y, x2-1, y );
    p.drawLine( x, y+1, x, y2-1 );
    p.setPen( g.dark().light(120) );
    p.drawLine( x2, y, x2, y2 );
    p.drawLine( x, y2, x2, y2 );

    // Fill out the border edges
	if ( coloredFrame)
	    p.setPen( g.background().light(130) );
	else
	    p.setPen( g.background() );
    p.drawRect( x+1, y+1, w-2, h-2 );
    p.drawRect( x+2, y+2, w-4, h-4 );

    // Draw a frame around the wrapped widget.
    p.setPen( g.background() );
    p.drawRect( x+3, y + titleHeight + 3, w-6, h-titleHeight-6 );

    // Drawing this extra line removes non-drawn areas when shaded
    p.drawLine( x+4, y2-4, x2-4, y2-4); 

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
    QColor c1 = options->color(Options::TitleBar, isActive() ).light(130);
    QColor c2 = options->color(Options::TitleBlend, isActive() );

    // Create a disposable pixmap buffer for the titlebar 
    KPixmap* titleBuffer = new KPixmap;
    titleBuffer->resize( w-6, titleHeight );  

    QPainter p2( titleBuffer, this );

	int rightoffset = r.x()+r.width()-25-4;	// subtract titleBlocks pixmap width and some

    p2.fillRect( 0, 0, w, r.height(), c1 );
    p2.fillRect( rightoffset, 0, w-rightoffset-6, r.height(), c2 );

    // 8 bit displays will be a bit dithered, but they still look ok.
    if ( isActive() )
		p2.drawPixmap( rightoffset, 0, *titleBlocks );
	else
		p2.drawPixmap( rightoffset, 0, *ititleBlocks );

    // Draw the title text on the pixmap, and with a smaller font
    // for toolwindows than the default.
    QFont fnt = options->font(true);
    if ( !largeButtons )
    { 
       fnt.setPointSize( fnt.pointSize() - 3 );  // Shrink font by 3pt 
       fnt.setWeight( QFont::Normal );           // and disable bold
    }
    p2.setFont( fnt );

    p2.setPen( options->color(Options::Font, isActive() ));
    p2.drawText(r.x(), 0, r.width()-3, r.height(),
                AlignLeft | AlignVCenter, caption() );
    p2.end();

    p.drawPixmap( 3, 3, *titleBuffer );

    delete titleBuffer;
}


void QuartzClient::showEvent(QShowEvent *ev)
{
    calcHiddenButtons();  
    show();
    Client::showEvent(ev);
}


void QuartzClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
       workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
}


void QuartzClient::maximizeChange(bool m)
{
	if (button[BtnMax]) {
	    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
		button[BtnMax]->setTipText(m ? i18n("Restore") : i18n("Maximize"));
	}
}


void QuartzClient::activeChange(bool)
{
    for(int i=QuartzClient::BtnHelp; i < QuartzClient::BtnCount; i++)
        if(button[i])
            button[i]->repaint(false);

    repaint(false);
}


// The hiding button while shrinking, show button while expanding magic
void QuartzClient::calcHiddenButtons()
{
	//Hide buttons in this order - Sticky, Help, Maximize, Menu, Minimize, Close.
	QuartzButton* btnArray[] = { button[BtnSticky], button[BtnHelp], button[BtnMax], 
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
    workspace()->showWindowMenu( button[BtnMenu]->mapToGlobal( menupoint ), this );
	button[BtnMenu]->setDown(false);
}

};

// Extended KWin plugin interface
/////////////////////////////////
extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
        return(new Quartz::QuartzClient(ws, w));
    }

	void init()
	{
        Quartz::clientHandler = new Quartz::QuartzHandler();
	}

	void reset()
	{
		// QuartzHandler takes care of this for us
	}

	void deinit()
	{
		delete Quartz::clientHandler;
	}
}



#include "quartz.moc"
// vim: ts=4
