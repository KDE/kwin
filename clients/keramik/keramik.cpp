/*
 * $Id$
 *
 * Keramik KWin client (version 0.6)
 *
 * Copyright (C) 2002 Fredrik Höglund <fredrik@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <kconfig.h>
#include <klocale.h>
#include <kiconeffect.h>

#include <qpainter.h>
#include <qlayout.h>
#include <qbitmap.h>
#include <qstyle.h>

#include "../../workspace.h"
#include "../../options.h"

#include "keramik.h"
#include "../../../config.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
#include <X11/extensions/shape.h>
#else
#define XShapeCombineMask(a,b,c,d,e,f,g)
#endif

#include "keramik.moc"

using namespace KWinInternal;



// -------------------------------------------------------------------------------------------



static const int buttonMargin    = 9;
static const int buttonSpacing   = 4;
static const int iconSpacing     = 5;

static const bool recolorPixmaps = false; // ### only for debugging

// Default button layout
static const char default_left[]  = "M";
static const char default_right[] = "IAX";

// Titlebar button bitmaps
static unsigned char menu_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0xf0, 0x07, 0x00,
   0xe0, 0x03, 0x00, 0xc0, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char sticky_on_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0xf0, 0x0f, 0x00,
   0xf0, 0x0f, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char sticky_off_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
   0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char help_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00,
   0xf0, 0x07, 0x00, 0x30, 0x06, 0x00, 0x00, 0x07, 0x00, 0x80, 0x03, 0x00,
   0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char iconify_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char maximize_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x80, 0x01, 0x00, 0xc0, 0x03, 0x00, 0xe0, 0x07, 0x00, 0xf0, 0x0f, 0x00,
   0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char restore_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
   0xf0, 0x0f, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00, 0x80, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};

static unsigned char close_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x30, 0x0c, 0x00, 0x70, 0x0e, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00,
   0xc0, 0x03, 0x00, 0xe0, 0x07, 0x00, 0x70, 0x0e, 0x00, 0x30, 0x0c, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00};



// ------------------------------------------------------------------------------------------



KeramikHandler *clientHandler = NULL;
bool keramik_initialized = false;



// -------------------------------------------------------------------------------------------



KeramikHandler::KeramikHandler()
	: QObject( NULL, NULL )
{
	for ( int i = 0; i < NumTiles; i++ ) {
		activeTiles[i]   = NULL;
		inactiveTiles[i] = NULL;
	}

	settings_cache = NULL;
	
	// Create the button deco bitmaps
	buttonDecos[ Menu ]     = new QBitmap( 17, 17, menu_bits,       true );
	buttonDecos[ Sticky ]   = new QBitmap( 17, 17, sticky_on_bits,  true );
	buttonDecos[ Unsticky ] = new QBitmap( 17, 17, sticky_off_bits, true );
	buttonDecos[ Help ]     = new QBitmap( 17, 17, help_bits,       true );
	buttonDecos[ Iconify ]  = new QBitmap( 17, 17, iconify_bits,    true );
	buttonDecos[ Maximize ] = new QBitmap( 17, 17, maximize_bits,   true );
	buttonDecos[ Restore ]  = new QBitmap( 17, 17, restore_bits,    true );
	buttonDecos[ Close ]    = new QBitmap( 17, 17, close_bits,      true );

	// Selfmask the bitmaps
	for ( int i = 0; i < NumButtonDecos; i++ )
		buttonDecos[i]->setMask( *buttonDecos[i] );

	// Flip the bitmaps horizontally in right-to-left mode
	if ( QApplication::reverseLayout() ) {
		for ( int i = 0; i < NumButtonDecos; i++ )
			flip( (QPixmap*)buttonDecos[i] );
	}

	readConfig();
	createPixmaps();

	keramik_initialized = true;
}


KeramikHandler::~KeramikHandler()
{
	keramik_initialized = false;
	destroyPixmaps();

	for ( int i = 0; i < NumButtonDecos; i++ )
		delete buttonDecos[i];

	if ( settings_cache )
		delete settings_cache;
}


void KeramikHandler::createPixmaps()
{
	QColor titleColor  = options->color( Options::TitleBar, true );
	QColor buttonColor = options->color( Options::ButtonBg, true );

	activeTiles[ TitleLeft ]       = loadPixmap( "titlebar-active-left",   titleColor );
	activeTiles[ TitleCenter ]     = loadPixmap( "titlebar-active-center", titleColor );
	activeTiles[ TitleRight ]      = loadPixmap( "titlebar-active-right",  titleColor );

	inactiveTiles[ TitleLeft ]     = loadPixmap( "titlebar-inactive-left",   titleColor );
	inactiveTiles[ TitleCenter ]   = loadPixmap( "titlebar-inactive-center", titleColor );
	inactiveTiles[ TitleRight ]    = loadPixmap( "titlebar-inactive-right",  titleColor );

	if ( smallCaptionBubbles ) {
		activeTiles[ CaptionLeft ]   = loadPixmap( "caption-active-small-left",   titleColor );
		activeTiles[ CaptionCenter ] = loadPixmap( "caption-active-small-center", titleColor );
		activeTiles[ CaptionRight ]  = loadPixmap( "caption-active-small-right",  titleColor );
	} else {
		activeTiles[ CaptionLeft ]   = loadPixmap( "caption-active-large-left",   titleColor );
		activeTiles[ CaptionCenter ] = loadPixmap( "caption-active-large-center", titleColor );
		activeTiles[ CaptionRight ]  = loadPixmap( "caption-active-large-right",  titleColor );
	}

	inactiveTiles[ CaptionLeft ]   = loadPixmap( "caption-inactive-left",   titleColor );
	inactiveTiles[ CaptionCenter ] = loadPixmap( "caption-inactive-center", titleColor );
	inactiveTiles[ CaptionRight ]  = loadPixmap( "caption-inactive-right",  titleColor );

	activeTiles[ GrabBarLeft ]     = loadPixmap( "grabbar-active-left",   titleColor );
	activeTiles[ GrabBarCenter ]   = loadPixmap( "grabbar-active-center", titleColor );
	activeTiles[ GrabBarRight ]    = loadPixmap( "grabbar-active-right",  titleColor );

	inactiveTiles[ GrabBarLeft ]   = loadPixmap( "grabbar-inactive-left",   titleColor );
	inactiveTiles[ GrabBarCenter ] = loadPixmap( "grabbar-inactive-center", titleColor );
	inactiveTiles[ GrabBarRight ]  = loadPixmap( "grabbar-inactive-right",  titleColor );

	activeTiles[ BorderLeft ]    = loadPixmap( "border-active-left",  titleColor );
	activeTiles[ BorderRight ]   = loadPixmap( "border-active-right", titleColor );

	inactiveTiles[ BorderLeft ]  = loadPixmap( "border-inactive-left",  titleColor );
	inactiveTiles[ BorderRight ] = loadPixmap( "border-inactive-right", titleColor );
	
	titleButtonRound  = loadPixmap( "titlebutton-round",  buttonColor );
	titleButtonSquare = loadPixmap( "titlebutton-square", buttonColor );

	if ( QApplication::reverseLayout() ) {
		
		// Fix lighting
		flip( activeTiles[CaptionLeft], activeTiles[CaptionRight] );
		flip( inactiveTiles[CaptionLeft], inactiveTiles[CaptionRight] );

		flip( activeTiles[TitleLeft], activeTiles[TitleRight] );
		flip( inactiveTiles[TitleLeft], inactiveTiles[TitleRight] );
		
		flip( activeTiles[BorderLeft], activeTiles[BorderRight] );
		flip( inactiveTiles[BorderLeft], inactiveTiles[BorderRight] );
		
		flip( activeTiles[GrabBarLeft], activeTiles[GrabBarRight] );
		flip( inactiveTiles[GrabBarLeft], inactiveTiles[GrabBarRight] );
		
		flip( titleButtonRound );
		flip( titleButtonSquare );
	}
}



void KeramikHandler::destroyPixmaps()
{
	for ( int i = 0; i < NumTiles; i++ ) {
		if ( activeTiles[i] ) {
			delete activeTiles[i];
			activeTiles[i] = NULL;
		}

		if ( inactiveTiles[i] ) {
			delete inactiveTiles[i];
			inactiveTiles[i] = NULL;
		}
	}

	delete titleButtonRound;
	delete titleButtonSquare;
}


void KeramikHandler::flip( QPixmap *&pix1, QPixmap *&pix2 )
{
	// Flip the pixmaps horizontally
	QPixmap *tmp = new QPixmap( pix1->xForm( QWMatrix(-1,0,0,1,pix1->width(),0) ) );

	delete pix1;
	pix1 = new QPixmap( pix2->xForm( QWMatrix(-1,0,0,1,pix2->width(),0) ) );

	delete pix2;
	pix2 = tmp;
}


void KeramikHandler::flip( QPixmap *&pix )
{
	// Flip the pixmap horizontally
	QPixmap *tmp = new QPixmap( pix->xForm( QWMatrix(-1,0,0,1,pix->width(),0) ) );
	delete pix;
	pix = tmp;
}


void KeramikHandler::readConfig()
{
	KConfig *c = new KConfig( "kwinkeramikrc" );

	c->setGroup( "General" );
	showIcons = c->readBoolEntry( "ShowAppIcons", true );
	shadowedText = c->readBoolEntry( "UseShadowedText", true );
	smallCaptionBubbles = c->readBoolEntry( "SmallCaptionBubbles", false );

	if ( ! settings_cache ) {
		settings_cache = new SettingsCache;
	
		if ( options->customButtonPositions() ) {
			settings_cache->buttonsLeft  = options->titleButtonsLeft();
			settings_cache->buttonsRight = options->titleButtonsRight();
		} else {
			settings_cache->buttonsLeft  = QString( default_left );
			settings_cache->buttonsRight = QString( default_right );
		}
		
		settings_cache->titleColor  = options->color( Options::TitleBar, true );
		settings_cache->buttonColor = options->color( Options::ButtonBg, true );
		settings_cache->smallCaptionBubbles = smallCaptionBubbles;
	}

	delete c;
}


QPixmap *KeramikHandler::loadPixmap( const QString &name, const QColor &col )
{
	if ( recolorPixmaps ) {
		QImage img = qembed_findImage( name ).copy();
		//KIconEffect::colorize( img, col, 1.0 );
		recolor( img, col );
		return new QPixmap( img );
	} else
		return new QPixmap( qembed_findImage(name) );
}


// This is the recoloring method from the Keramik widget style,
// copyright (c) 2002 Malte Starostik <malte@kde.org>.
// Modified to work with 8bpp images.
void KeramikHandler::recolor( QImage &img, const QColor& color )
{
	int hue = -1, sat = 0, val = 228;
	if ( color.isValid() ) color.hsv( &hue, &sat, &val );

	register int pixels = (img.depth() > 8 ? img.width() * img.height() : img.numColors());
	register Q_UINT32* data = ( img.depth() > 8 ? reinterpret_cast< Q_UINT32* >( img.bits() ) :
			reinterpret_cast< Q_UINT32* >( img.colorTable() ) );
	
	for ( int i = 0; i < pixels; i++ )
	{
		QColor c( *data );
		int h, s, v;
		c.hsv( &h, &s, &v );
		if ( hue >= 0 && h >= 0 ) h = ( h + 114 + hue ) % 360;
		if ( s ) s += sat / 2;
		c.setHsv( h, QMIN( s, 255 ), QMIN( v * val / 228, 255 ) );
		*data = ( c.rgb() & RGB_MASK ) | ( *data & ~RGB_MASK );
		data++;
	}
}


void KeramikHandler::reset()
{
	QString buttonsLeft, buttonsRight;
	
	keramik_initialized = false;
	
	bool needHardReset  = false;
	bool pixmapsInvalid = false;

	// Re-read the config file
	readConfig();

	// Check if the color scheme has changed
	if ( settings_cache->titleColor != options->color(Options::TitleBar, true) ||
			settings_cache->buttonColor != options->color(Options::ButtonBg, true) ) {
		pixmapsInvalid = true;
	}
	
	// Check if the caption bubble size has changed
	if ( settings_cache->smallCaptionBubbles != smallCaptionBubbles ) {
		pixmapsInvalid = true;
		needHardReset  = true;
	}

	// Check if button positions have changed
	if ( options->customButtonPositions() ) {
		buttonsLeft  = options->titleButtonsLeft();
		buttonsRight = options->titleButtonsRight();
	} else {
		buttonsLeft  = QString( default_left );
		buttonsRight = QString( default_right );
	}
		
	if ( (settings_cache->buttonsLeft != buttonsLeft) ||
			(settings_cache->buttonsRight != buttonsRight) ) {
		needHardReset = true;
	}
	
	// Update our config cache
	settings_cache->titleColor          = options->color( Options::TitleBar, true );
	settings_cache->buttonColor         = options->color( Options::ButtonBg, true );
	settings_cache->smallCaptionBubbles = smallCaptionBubbles;
	settings_cache->buttonsLeft         = buttonsLeft;
	settings_cache->buttonsRight        = buttonsRight;
	
	
	// Do we need to recreate the pixmaps?
	if ( pixmapsInvalid ) {
		destroyPixmaps();
		createPixmaps();
	}

	keramik_initialized = true;

	// Do we need to "hit the wooden hammer" ?
	if ( needHardReset )
		Workspace::self()->slotResetAllClients();
	else
		emit softReset();
}


const QPixmap *KeramikHandler::tile( TilePixmap tilePix, bool active ) const
{
	return ( active ? activeTiles[ tilePix ] : inactiveTiles[ tilePix ] );
}



// -------------------------------------------------------------------------------------------



KeramikButton::KeramikButton( Client* parent, const char *name, Button btn, const QString &tip )
		: KWinButton( parent, name, tip ),
		client( parent ), button( btn ), hover( false ), lastbutton( 0 )
{
	setBackgroundMode( NoBackground );
	setFixedSize( 17, 17 );

	setToggleButton( (button == StickyButton) );
}


KeramikButton::~KeramikButton()
{
	// Empty.
}


void KeramikButton::enterEvent( QEvent *e )
{
	KWinButton::enterEvent( e );
	
	hover = true;
	repaint( false );
}


void KeramikButton::leaveEvent( QEvent *e )
{
	KWinButton::leaveEvent( e );
	
	hover = false;
	repaint( false );
}


void KeramikButton::mousePressEvent( QMouseEvent *e )
{
	lastbutton = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	KWinButton::mousePressEvent( &me );
}


void KeramikButton::mouseReleaseEvent( QMouseEvent *e )
{
	lastbutton = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	KWinButton::mouseReleaseEvent( &me );
}


void KeramikButton::drawButton( QPainter *p )
{
	const QPixmap *pix;
	const QBitmap *deco;

	// Get the bevel from the client handler
	if ( button == MenuButton || button == StickyButton || button == HelpButton ) 
		pix = clientHandler->roundButton();
	else
		pix = clientHandler->squareButton();

	// Draw the button background
	p->drawPixmap( 0, 0, *clientHandler->tile( TitleCenter, client->isActive() ),
			0, 5, 17, 17 );

	if ( isDown() ) {
		// Pressed
		p->drawPixmap( QPoint(), *pix, QStyle::visualRect( QRect(34, 0, 17, 17), pix->rect() ) );
		p->translate( QApplication::reverseLayout() ? -1 : 1,  1 );
	} else if ( hover )
		// Mouse over
		p->drawPixmap( QPoint(), *pix, QStyle::visualRect( QRect(17, 0, 17, 17), pix->rect() ) );
	else
		// Normal
		p->drawPixmap( QPoint(), *pix, QStyle::visualRect( QRect(0, 0, 17, 17), pix->rect() ) );

	
	// Draw the button deco on the bevel
	switch ( button ) {
		case MenuButton:
			deco = clientHandler->buttonDeco( Menu );
			break;

		case StickyButton:
			deco = clientHandler->buttonDeco( isOn() ? Unsticky : Sticky );
			break;

		case HelpButton:
			deco = clientHandler->buttonDeco( Help );
			break;
			
		case MinButton:
			deco = clientHandler->buttonDeco( Iconify );
			break;
		
		case MaxButton: 
			deco = clientHandler->buttonDeco( client->isMaximized() ? Restore : Maximize );
			break;

		case CloseButton:
			deco = clientHandler->buttonDeco( Close );
			break;
	
		default:
			deco = NULL;
	}

	p->setPen( Qt::black ); // ### hardcoded color
	p->drawPixmap( 0, 0, *deco );
}



// ------------------------------------------------------------------------------------------



KeramikClient::KeramikClient( Workspace *ws, WId w, QWidget *parent, const char *name )
		: Client( ws, w, parent, name, WStaticContents | WResizeNoErase | WRepaintNoErase ),
		activeIcon( NULL ), inactiveIcon( NULL ), captionBufferDirty( true ), maskDirty( true )
{
	// Minimize flicker
	setBackgroundMode( NoBackground );

	for ( int i=0; i < NumButtons; i++ )
		button[i] = NULL;

	QVBoxLayout *mainLayout   = new QVBoxLayout( this );
	QHBoxLayout *titleLayout  = new QHBoxLayout();
	QHBoxLayout *windowLayout = new QHBoxLayout();

	// Button margin
	int topMargin = clientHandler->titleBarHeight()
		- clientHandler->titleBarBaseHeight() + 1;
	mainLayout->addSpacing( topMargin );

	mainLayout->addLayout( titleLayout );         // Titlebar
	mainLayout->addLayout( windowLayout, 1 );     // Left border + window + right border
	mainLayout->addSpacing( 8 );                  // Bottom grab bar + shadow

	titleLayout->setSpacing( buttonSpacing );
	
	titleLayout->addSpacing( buttonMargin );      // Left button margin
	addButtons( titleLayout, options->customButtonPositions() ?
			options->titleButtonsLeft() : QString(default_left) );

	titlebar = new QSpacerItem( 10, clientHandler->titleBarHeight() - topMargin,
		QSizePolicy::Expanding, QSizePolicy::Minimum );
	titleLayout->addItem( titlebar );

	titleLayout->addSpacing( buttonSpacing );
	addButtons( titleLayout, options->customButtonPositions() ?
				options->titleButtonsRight() : QString(default_right) );
	titleLayout->addSpacing( buttonMargin );      // Right button margin

	windowLayout->addSpacing( 3 );                // Left border
	windowLayout->addWidget( windowWrapper() );   // Window wrapper
	windowLayout->addSpacing( 4 );                // Right border

	connect( clientHandler, SIGNAL(softReset()), SLOT(reset()) );
}


KeramikClient::~KeramikClient()
{
	if ( activeIcon )
		delete activeIcon;

	if ( inactiveIcon )
		delete inactiveIcon;

	activeIcon = inactiveIcon = NULL;
}


void KeramikClient::reset()
{
	calculateCaptionRect();
	
	captionBufferDirty = maskDirty = true;

	if ( isVisible() )
		repaint( false );
}

void KeramikClient::addButtons( QHBoxLayout *layout, const QString &s )
{
	for ( uint i=0; i < s.length(); i++ )
	{
		switch ( s[i].latin1() )
		{
			// Menu button
			case 'M' :
				if ( !button[MenuButton] ) {
					button[MenuButton] = new KeramikButton( this, "menu", MenuButton, i18n("Menu") );
					connect( button[MenuButton], SIGNAL( pressed() ), SLOT( menuButtonPressed() ) );
					layout->addWidget( button[MenuButton] );
				}
				break;

			// Sticky button
			case 'S' :
				if ( !button[StickyButton] ) {
					button[StickyButton] = new KeramikButton( this, "sticky", StickyButton, i18n("Sticky") );
					connect( button[StickyButton], SIGNAL( clicked() ), SLOT( toggleSticky() ) );
					layout->addWidget( button[StickyButton] );
				}
				break;
				
			// Help button
			case 'H' :
				if ( !button[HelpButton] && providesContextHelp() ) {
					button[HelpButton] = new KeramikButton( this, "help", HelpButton, i18n("Help") );
					connect( button[HelpButton], SIGNAL( clicked() ), SLOT( contextHelp() ) );
					layout->addWidget( button[HelpButton] );
				}
				break;

			// Minimize button
			case 'I' :
				if ( !button[MinButton] && isMinimizable() ) {
					button[MinButton] = new KeramikButton( this, "iconify", MinButton, i18n("Minimize") );
					connect( button[MinButton], SIGNAL( clicked() ), SLOT( iconify() ) );
					layout->addWidget( button[MinButton] );
				}
				break;

			// Maximize button
			case 'A' :
				if ( !button[MaxButton] && isMaximizable() ) {
					button[MaxButton] = new KeramikButton( this, "maximize", MaxButton, i18n("Maximize") );
					connect( button[MaxButton], SIGNAL( clicked() ), SLOT( slotMaximize() ) );
					layout->addWidget( button[MaxButton] );
				}
				break;
			
			// Close button
			case 'X' :
				if ( !button[CloseButton] ) {
					button[CloseButton] = new KeramikButton( this, "close", CloseButton, i18n("Close") );
					connect( button[CloseButton], SIGNAL( clicked() ), SLOT( closeWindow() ) );
					layout->addWidget( button[CloseButton] );
				}
				break;
				
			// Additional spacing
			case '_' :
				layout->addSpacing( buttonSpacing );
				break;
		}
	}
}


void KeramikClient::updateMask()
{
	if ( !keramik_initialized )
		return;

	// This code is written in Xlib to work around a bug somewhere in the
	// Qt masking/bitmap code.

	Display *dpy = QPaintDevice::x11AppDisplay();
	int screen   = QPaintDevice::x11Screen();
	
	Pixmap pix   = XCreatePixmap( dpy, handle(), width(), height(), 1 );

	const QBitmap *tile;

	GC gc = XCreateGC( dpy, pix, 0, 0 );
	XSetFillStyle( dpy, gc, FillSolid );

	// Clear the titlebar area
	XSetForeground( dpy, gc, BlackPixel(dpy, screen) );
	XFillRectangle( dpy, pix, gc, 0, 0, width(), clientHandler->titleBarHeight() );

	int titleBaseY = clientHandler->titleBarHeight()
		- clientHandler->titleBarBaseHeight();

	// Set the background and foreground colors for XCopyArea()
	XSetForeground( dpy, gc, WhitePixel(dpy, screen) );
	XSetBackground( dpy, gc, BlackPixel(dpy, screen) );

	// Top left corner
	tile = clientHandler->tile( TitleLeft, isActive() )->mask();
	XCopyArea( dpy, tile->handle(), pix, gc, 0, 0, tile->width(), tile->height(), 0, titleBaseY );

	// Space between top left & top right corners
	XFillRectangle( dpy, pix, gc, 15, titleBaseY, width() - 30, clientHandler->titleBarBaseHeight() );

	// Caption bubble
	if ( isActive() && titleBaseY && captionRect.width() >= 28 ) {
		// Left caption corner
		tile = clientHandler->tile( CaptionLeft, true )->mask();
		XCopyArea( dpy, tile->handle(), pix, gc, 0, 0, tile->width(), tile->height(),
				captionRect.left(), 0 );

		// Caption center
		XFillRectangle( dpy, pix, gc, captionRect.left() + 14, 0, captionRect.width() - 28,
				clientHandler->titleBarHeight() );

		// Right caption corner
		tile = clientHandler->tile( CaptionRight, true )->mask();
		XCopyArea( dpy, tile->handle(), pix, gc, 0, 0, tile->width(), tile->height(),
				captionRect.left() + captionRect.width() - 14, 0 );
	}

	// Top right corner
	tile = clientHandler->tile( TitleRight, true )->mask();
	XCopyArea( dpy, tile->handle(), pix, gc, 0, 0, tile->width(), tile->height(),
			width() - 15, titleBaseY );

	// Bottom part of the window
	XFillRectangle( dpy, pix, gc, 0, clientHandler->titleBarHeight(), width(),
			height() - clientHandler->titleBarHeight() );
	
	XFreeGC( dpy, gc );

	// Set the mask
	XShapeCombineMask( dpy, handle(), ShapeBounding, 0, 0, pix, ShapeSet );	
	XFreePixmap( dpy, pix );

	maskDirty = false;
}


void KeramikClient::updateCaptionBuffer()
{
	if ( !keramik_initialized )
		return;

	bool active = isActive();
	QPixmap *icon = NULL;

	if ( captionBuffer.size() != captionRect.size() )
		captionBuffer.resize( captionRect.size() );

	QPainter p( &captionBuffer );

	// Draw the caption bubble
	p.drawPixmap( 0, 0, *clientHandler->tile( CaptionLeft, active ) );
	p.drawTiledPixmap( 14, 0, captionRect.width() - 28, clientHandler->titleBarHeight(),
		*clientHandler->tile( CaptionCenter, active ) );
	p.drawPixmap( captionRect.width() - 14, 0, *clientHandler->tile( CaptionRight, active ) );
	
	if ( clientHandler->showAppIcons() )
	{
		if ( active ) {
			if ( ! activeIcon ) {
				if ( miniIcon().width() > 16 ) {
					QImage img = miniIcon().convertToImage();
					img.smoothScale( 16, 16 );
					activeIcon = new QPixmap( img );
				} else
					activeIcon = new QPixmap( miniIcon() );
			}
			icon = activeIcon;
		} else {
			if ( ! inactiveIcon ) {
				QImage img = miniIcon().convertToImage();

				if ( img.width() > 16 )
					img.smoothScale( 16, 16 );

				KIconEffect::semiTransparent( img );
				inactiveIcon = new QPixmap( img );
			}
			icon = inactiveIcon;
		}
	}

	p.setFont( options->font( active ) );
	int tw = p.fontMetrics().width( caption() ) +
		( clientHandler->showAppIcons() ? 16 + iconSpacing : 0 );

	int xpos = QMAX( (captionRect.width() - tw) / 3, 10 );
	QRect tr = QStyle::visualRect( QRect(xpos, 1, captionRect.width() - xpos - 10,
				captionRect.height() - 4), captionBuffer.rect() );

	//p.setPen( Qt::red ); // debug
	//p.drawRect( tr );    // debug
	
	// Application icon
	if ( clientHandler->showAppIcons() )
	{
		if ( tr.width() > 14 ) {
			QRect iconRect = QStyle::visualRect( QRect(tr.x(),
						1 + (captionRect.height() - 4 - 16) / 2, 16, 16), tr );
			QRect r( icon->rect() );
			r.moveCenter( iconRect.center() );
			p.drawPixmap( r, *icon );
		}
		
		//p.drawRect( r ); // debug
		
		if ( QApplication::reverseLayout() )
			tr.addCoords( 0, 0, -(16 + iconSpacing), 0 );
		else
			tr.addCoords( (16 + iconSpacing), 0, 0, 0 );
	}

	// Draw the titlebar text
	int flags = AlignVCenter | SingleLine;
	flags |= ( QApplication::reverseLayout() ? AlignRight : AlignLeft );

	if ( clientHandler->useShadowedText() )
	{
		p.translate( QApplication::reverseLayout() ? -1 : 1, 1 );
		p.setPen( active ? QColor(65,153,238).dark() :
				QColor(180,210,246).dark() ); // ### hardcoded color
		p.drawText( tr,	flags, caption() );
		p.translate( QApplication::reverseLayout() ? 1 : -1, -1 );
	}

	p.setPen( Qt::white ); // ### hardcoded color
	//p.setPen( options->color( Options::Font, active ) );
	p.drawText( tr,	flags, caption() );

	captionBufferDirty = false;
}


void KeramikClient::calculateCaptionRect()
{
	QFontMetrics fm( options->font(isActive()) );
	int cw = fm.width( caption() ) + 95;
	int titleBaseY = clientHandler->titleBarHeight() - clientHandler->titleBarBaseHeight();

	if ( clientHandler->showAppIcons() )
		cw += 16 + 4; // icon width + space

	cw = QMIN( cw, titlebar->geometry().width() );
	captionRect = QStyle::visualRect( QRect(titlebar->geometry().x(), isActive() ? 0 : titleBaseY,
				cw, clientHandler->titleBarHeight() - ( isActive() ? 0 : titleBaseY )),
				titlebar->geometry() );
}


void KeramikClient::captionChange( const QString& )
{
	QRect r( captionRect );
	calculateCaptionRect();

	if ( r.size() != captionRect.size() )
		maskDirty = true;

	captionBufferDirty = true;
	
	repaint( r | captionRect, false );
}


void KeramikClient::iconChange()
{
	if ( clientHandler->showAppIcons() ) {

		// Force updateCaptionBuffer() to recreate the cached icons
		if ( activeIcon )
			delete activeIcon;

		if ( inactiveIcon )
			delete inactiveIcon;

		activeIcon = inactiveIcon = NULL;

		captionBufferDirty = true;
		repaint( captionRect, false );
	}
}


void KeramikClient::activeChange( bool )
{
	// Note: It's assumed that the same font will always be used for both active
	//       and inactive windows, since the fonts kcm hasn't supported setting
	//       different fonts for different window states for some time.
	if ( clientHandler->titleBarHeight() != clientHandler->titleBarBaseHeight() ) {
		calculateCaptionRect();
		maskDirty = true;
	}
	
	captionBufferDirty = true;
	
	repaint( false );

	for ( int i=0; i < NumButtons; i++ )
		if ( button[i] ) button[i]->repaint();
}


void KeramikClient::maximizeChange( bool maximized )
{
	if ( button[ MaxButton ] ) {
		button[ MaxButton ]->setTipText( maximized ? i18n("Restore") : i18n("Maximize") );
		button[ MaxButton ]->repaint();
	}
}


void KeramikClient::stickyChange( bool on )
{
	if ( button[ StickyButton ] )
		button[ StickyButton ]->setTipText( on ? i18n("Un-Sticky") : i18n("Sticky") );
}


void KeramikClient::menuButtonPressed()
{
	static QTime *t = NULL;
	static KeramikClient *tc = NULL;

	if ( !t )
		t = new QTime;

	if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() )
	{
		QPoint menuPoint ( button[MenuButton]->rect().bottomLeft().x() - 6, 
				button[MenuButton]->rect().bottomLeft().y() + 3 );
		workspace()->clientPopup( this )->popup( button[MenuButton]->mapToGlobal( menuPoint ) );
	
		// Post a fake mouse button release event to the menu button
		// to ensure that it's redrawn in its unpressed state
		QApplication::postEvent( button[MenuButton], new QMouseEvent( QEvent::MouseButtonRelease,
						QPoint(0,0), Qt::LeftButton, Qt::LeftButton ) );
	} else
		closeWindow();

	
	t->start();
	tc = this;
}


void KeramikClient::slotMaximize()
{
	if ( button[ MaxButton ]->lastButton() == MidButton )
		maximize( MaximizeVertical );
	
	else if ( button[ MaxButton ]->lastButton() == RightButton )
		maximize( MaximizeHorizontal );
	
	else
		maximize();
}


void KeramikClient::paintEvent( QPaintEvent *e )
{
	if ( !keramik_initialized )
		return;

	QPainter p( this );
	QRect updateRect( e->rect() );
	bool active = isActive();
	int titleBaseY = clientHandler->titleBarHeight()
		- clientHandler->titleBarBaseHeight();

	if ( maskDirty )
		updateMask();
	
	// Titlebar
	// -----------------------------------------------------------------------
	if ( updateRect.y() < clientHandler->titleBarHeight() )
	{
		if ( captionBufferDirty )
			updateCaptionBuffer();
		
		// Top left corner
		if ( updateRect.x() < 15 )
			p.drawPixmap( 0, titleBaseY, *clientHandler->tile( TitleLeft, active ) );

		// Space between the top left corner and the caption bubble
		if ( updateRect.x() < captionRect.left() && updateRect.right() > 15 ) {
			int x1 = QMAX( 15, updateRect.x() );
			int x2 = QMIN( captionRect.left(), updateRect.right() );
		
			p.drawTiledPixmap( x1, titleBaseY, x2 - x1 + 1, clientHandler->titleBarBaseHeight(),
					*clientHandler->tile( TitleCenter, active ) );
		}

		// Caption bubble
		if ( updateRect.x() < captionRect.right() && updateRect.right() > 15 ) {
			if ( captionRect.width() >= 28 )
				p.drawPixmap( captionRect.left(), active ? 0 : titleBaseY, captionBuffer );
			else
				p.drawTiledPixmap( captionRect.x(), titleBaseY, captionRect.width(),
						clientHandler->titleBarBaseHeight(),
						*clientHandler->tile( TitleCenter, active ) );
		}
		
		// Space between the caption bubble and the top right corner
		if ( updateRect.right() > captionRect.right() && updateRect.x() < width() - 15 ) {
			int x1 = QMAX( captionRect.right() + 1, updateRect.x() );
			int x2 = QMIN( width() - 15, updateRect.right() );
			
			p.drawTiledPixmap( x1, titleBaseY, x2 - x1 + 1, clientHandler->titleBarBaseHeight(),
					*clientHandler->tile( TitleCenter, active ) );
		}

		// Top right corner
		if ( updateRect.right() > width() - 15 )
			p.drawPixmap( width() - 15, titleBaseY, *clientHandler->tile( TitleRight, active ) );
	}

	// Borders
	// -----------------------------------------------------------------------
	if ( updateRect.bottom() > clientHandler->titleBarHeight()
			&& updateRect.top() < height() - 8 )
	{
		int top    = QMAX( clientHandler->titleBarHeight(), updateRect.top() );
		int bottom = QMIN( updateRect.bottom(), height() - 8 );
		
		// Left border
		if ( updateRect.x() <= 4 ) {
			const QPixmap *tile = clientHandler->tile( BorderLeft, active );
			p.drawTiledPixmap( 0, top, tile->width(), bottom - top + 1, *tile );
		}

		// Right border
		if ( e->rect().right() >= width()-5 ) {
			const QPixmap *tile = clientHandler->tile( BorderRight, active );
			p.drawTiledPixmap( width() - tile->width(), top,
					tile->width(), bottom - top + 1, *tile );
		}
	}
	
	// Bottom grab bar
	// -----------------------------------------------------------------------
	if ( updateRect.bottom() > height() - 8 ) {
		// Bottom left corner
		if ( updateRect.x() < 9 )
			p.drawPixmap( 0, height() - 8, *clientHandler->tile( GrabBarLeft, active ) );

		// Space between the left corner and the right corner
		if ( updateRect.x() < width() - 9 ) {
			int x1 = QMAX( 9, updateRect.x() );
			int x2 = QMIN( width() - 9, updateRect.right() );
			
			p.drawTiledPixmap( x1, height() - 8, x2 - x1 + 1, 8,
					*clientHandler->tile( GrabBarCenter, active ) );
		}

		// Bottom right corner
		if ( updateRect.right() > width() - 9 )
			p.drawPixmap( width()-9, height()-8, *clientHandler->tile( GrabBarRight, active ) );
	}
}


void KeramikClient::resizeEvent( QResizeEvent *e )
{
	Client::resizeEvent( e );

	QRect r( captionRect );
	calculateCaptionRect();

	if ( r.size() != captionRect.size() )
		captionBufferDirty = true;

	maskDirty = true;
	
	if ( isVisible() )
	{
		update( rect() );
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


void KeramikClient::mouseDoubleClickEvent( QMouseEvent *e )
{
	if ( QRect( 0, 0, width(), clientHandler->titleBarHeight() ).contains( e->pos() ) )
		workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
}


Client::MousePosition KeramikClient::mousePosition( const QPoint &p ) const
{
	int titleBaseY = clientHandler->titleBarHeight()
		- clientHandler->titleBarBaseHeight();

	int leftBorder  = clientHandler->tile( BorderLeft, true )->width();
	int rightBorder = width() - clientHandler->tile( BorderRight, true )->width() - 1;
	
	
	// Test corners & sides
	if ( p.x() < leftBorder + 11 ) {
		if ( p.y() < titleBaseY + 11 ) {
			if ( (p.y() < titleBaseY + 3 && p.x() < leftBorder + 11) ||
					(p.y() < titleBaseY + 6 && p.x() < leftBorder + 6) ||
					(p.y() < titleBaseY + 11 && p.x() < leftBorder + 3) )
				return TopLeft;
			else
				return Center;
		}

		if ( p.y() > height()-23 )
			return BottomLeft;

		if ( p.x() > leftBorder )
			return Center;

		return Left;
	}

	if ( p.x() > rightBorder - 11 ) {
		if ( p.y() < titleBaseY + 11 ) {
			if ( (p.y() < titleBaseY + 3 && p.x() > rightBorder - 11) ||
					(p.y() < titleBaseY + 6 && p.x() > rightBorder - 6) ||
					(p.y() < titleBaseY + 11 && p.x() > rightBorder - 3) )
				return TopRight;
			else
				return Center;
		}
		
		if ( p.y() > height()-23 )
			return BottomRight;
		
		if ( p.x() < rightBorder )
			return Center;
		
		return Right;
	}

	// Test top & bottom
	if ( p.y() <= 3 )
		return Top;

	if ( (p.x() < captionRect.left() || p.x() > captionRect.right())
			&& p.y() < titleBaseY+3 )
		return Top;

	if ( p.y() > height() - 8 )
		return Bottom;

	return Center;
}



// -------------------------------------------------------------------------------------------



extern "C"
{
	Client *allocate( Workspace *ws, WId w )
	{
		return new KeramikClient( ws, w );
	}
	
	void init()
	{
		clientHandler = new KeramikHandler();
	}
	
	void reset()
	{
		clientHandler->reset();
	}
	
	void deinit()
	{
		delete clientHandler;
	}
}



// vim: set noet ts=4 sw=4:
