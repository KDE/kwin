/*
 * $Id$
 *
 * Keramik KWin client (version 0.8)
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
#define XShapeCombineRectangles(a,b,c,d,e,f,g,h,i)
#endif

#include "keramik.moc"

using namespace KWinInternal;



// -------------------------------------------------------------------------------------------



namespace Keramik
{

	const int buttonMargin     =  9;  // Margin between the window edge and the buttons
	const int buttonSpacing    =  4;  // Spacing between the titlebar buttons
	const int iconSpacing      =  5;  // Spacing between the icon and the text label
	const int bottomCornerSize = 30;  // Size of the bottom diagonal resize corners

	// Default button layout
	const char default_left[]  = "M";
	const char default_right[] = "HIAX";

	// Titlebar button bitmaps
	unsigned char menu_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0xf0, 0x07, 0x00,
	   0xe0, 0x03, 0x00, 0xc0, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char sticky_on_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char sticky_off_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char help_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00,
	   0xf0, 0x07, 0x00, 0x30, 0x06, 0x00, 0x00, 0x07, 0x00, 0x80, 0x03, 0x00,
	   0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char iconify_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char maximize_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x80, 0x01, 0x00, 0xc0, 0x03, 0x00, 0xe0, 0x07, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char restore_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	unsigned char close_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x30, 0x0c, 0x00, 0x70, 0x0e, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00,
	   0xc0, 0x03, 0x00, 0xe0, 0x07, 0x00, 0x70, 0x0e, 0x00, 0x30, 0x0c, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};


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

	imageDb = KeramikImageDb::instance();

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
		for ( int i = 0; i < Help; i++ )
			flip( reinterpret_cast<QPixmap**>(buttonDecos)[i] );
		
		for ( int i = Help + 1; i < NumButtonDecos; i++ )
			flip( reinterpret_cast<QPixmap**>(buttonDecos)[i] );
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

	KeramikImageDb::release();
	imageDb = NULL;
}


void KeramikHandler::createPixmaps()
{
	QColor titleColor, captionColor, buttonColor;
	QImage *titleCenter = NULL, *captionLeft = NULL,
		*captionRight = NULL, *captionCenter = NULL;


	// Active tiles
	// -------------------------------------------------------------------------
	captionColor = options->color( Options::TitleBar,   true );
	titleColor   = options->color( Options::TitleBlend, true );

	// Load the titlebar corners.
	activeTiles[ TitleLeft ]  = loadPixmap( "titlebar-left",  titleColor );
	activeTiles[ TitleRight ] = loadPixmap( "titlebar-right", titleColor );

	// Load the titlebar center tile image (this will be used as
	//     the background for the caption bubble tiles).
	titleCenter = loadImage( "titlebar-center", titleColor );

	// Load the small version of the caption bubble corner & center images.
	captionLeft   = loadImage( "caption-small-left",   captionColor );
	captionRight  = loadImage( "caption-small-right",  captionColor );
	captionCenter = loadImage( "caption-small-center", captionColor );

	// Create the caption bubble tiles (by blending the images onto the titlebar)
	activeTiles[ CaptionSmallLeft   ] = composite( captionLeft,   titleCenter );
	activeTiles[ CaptionSmallRight  ] = composite( captionRight,  titleCenter );
	activeTiles[ CaptionSmallCenter ] = composite( captionCenter, titleCenter );

	delete captionLeft;
	delete captionRight;
	delete captionCenter;

	// Now do the same with the large version
	captionLeft   = loadImage( "caption-large-left",   captionColor );
	captionRight  = loadImage( "caption-large-right",  captionColor );
	captionCenter = loadImage( "caption-large-center", captionColor );

	activeTiles[ CaptionLargeLeft   ] = composite( captionLeft,   titleCenter );
	activeTiles[ CaptionLargeRight  ] = composite( captionRight,  titleCenter );
	activeTiles[ CaptionLargeCenter ] = composite( captionCenter, titleCenter );

	delete captionLeft;
	delete captionRight;
	delete captionCenter;

	// Create the titlebar center tile
	activeTiles[ TitleCenter ] = new QPixmap( *titleCenter );

	delete titleCenter;

	// Load the left & right border pixmaps
	activeTiles[ BorderLeft ]  = loadPixmap( "border-left",  titleColor );
	activeTiles[ BorderRight ] = loadPixmap( "border-right", titleColor );

	// Load the bottom grabbar pixmaps
	if ( largeGrabBars ) {
		activeTiles[ GrabBarLeft ]   = loadPixmap( "grabbar-left",   titleColor );
		activeTiles[ GrabBarRight ]  = loadPixmap( "grabbar-right",  titleColor );
		activeTiles[ GrabBarCenter ] = loadPixmap( "grabbar-center", titleColor );
	} else {
		activeTiles[ GrabBarLeft ]   = loadPixmap( "bottom-left",   titleColor );
		activeTiles[ GrabBarRight ]  = loadPixmap( "bottom-right",  titleColor );
		activeTiles[ GrabBarCenter ] = loadPixmap( "bottom-center", titleColor );
	}

	// Inactive tiles
	// -------------------------------------------------------------------------
	captionColor = options->color( Options::TitleBar,   false );
	titleColor   = options->color( Options::TitleBlend, false );

	inactiveTiles[ TitleLeft ]  = loadPixmap( "titlebar-left",  titleColor );
	inactiveTiles[ TitleRight ] = loadPixmap( "titlebar-right", titleColor );

	titleCenter = loadImage( "titlebar-center", titleColor );

	captionLeft   = loadImage( "caption-small-left",   captionColor );
	captionRight  = loadImage( "caption-small-right",  captionColor );
	captionCenter = loadImage( "caption-small-center", captionColor );

	inactiveTiles[ CaptionSmallLeft  ]  = composite( captionLeft,   titleCenter );
	inactiveTiles[ CaptionSmallRight ]  = composite( captionRight,  titleCenter );
	inactiveTiles[ CaptionSmallCenter ] = composite( captionCenter, titleCenter );

	delete captionLeft;
	delete captionRight;
	delete captionCenter;

	inactiveTiles[ TitleCenter ] = new QPixmap( *titleCenter );

	delete titleCenter;

	inactiveTiles[ BorderLeft ]  = loadPixmap( "border-left",  titleColor );
	inactiveTiles[ BorderRight ] = loadPixmap( "border-right", titleColor );

	if ( largeGrabBars ) {
		inactiveTiles[ GrabBarLeft ]   = loadPixmap( "grabbar-left",   titleColor );
		inactiveTiles[ GrabBarRight ]  = loadPixmap( "grabbar-right",  titleColor );
		inactiveTiles[ GrabBarCenter ] = loadPixmap( "grabbar-center", titleColor );
	} else {
		inactiveTiles[ GrabBarLeft ]   = loadPixmap( "bottom-left",   titleColor );
		inactiveTiles[ GrabBarRight ]  = loadPixmap( "bottom-right",  titleColor );
		inactiveTiles[ GrabBarCenter ] = loadPixmap( "bottom-center", titleColor );
	}

	// Buttons
	// -------------------------------------------------------------------------
	buttonColor  = QColor(); //options->color( Options::ButtonBg, true );

	titleButtonRound  = loadPixmap( "titlebutton-round",  buttonColor );
	titleButtonSquare = loadPixmap( "titlebutton-square", buttonColor );


	// Prepare the tiles for use
	// -------------------------------------------------------------------------
	if ( QApplication::reverseLayout() ) {

		// Fix lighting
		flip( activeTiles[CaptionSmallLeft], activeTiles[CaptionSmallRight] );
		flip( inactiveTiles[CaptionSmallLeft], inactiveTiles[CaptionSmallRight] );

		flip( activeTiles[CaptionLargeLeft], activeTiles[CaptionLargeRight] );

		flip( activeTiles[TitleLeft], activeTiles[TitleRight] );
		flip( inactiveTiles[TitleLeft], inactiveTiles[TitleRight] );

		flip( activeTiles[BorderLeft], activeTiles[BorderRight] );
		flip( inactiveTiles[BorderLeft], inactiveTiles[BorderRight] );

		flip( activeTiles[GrabBarLeft], activeTiles[GrabBarRight] );
		flip( inactiveTiles[GrabBarLeft], inactiveTiles[GrabBarRight] );

		flip( titleButtonRound );
		flip( titleButtonSquare );
	}

	// Pretile the center & border tiles for optimal performance
	pretile( activeTiles[ CaptionSmallCenter ], 64, Qt::Horizontal );
	pretile( activeTiles[ CaptionLargeCenter ], 64, Qt::Horizontal );
	pretile( activeTiles[ TitleCenter ], 64, Qt::Horizontal );
	pretile( activeTiles[ GrabBarCenter ], 128, Qt::Horizontal );
	pretile( activeTiles[ BorderLeft ], 128, Qt::Vertical );
	pretile( activeTiles[ BorderRight ], 128, Qt::Vertical );

	pretile( inactiveTiles[ CaptionSmallCenter ], 64, Qt::Horizontal );
	pretile( inactiveTiles[ TitleCenter ], 64, Qt::Horizontal );
	pretile( inactiveTiles[ GrabBarCenter ], 128, Qt::Horizontal );
	pretile( inactiveTiles[ BorderLeft ], 128, Qt::Vertical );
	pretile( inactiveTiles[ BorderRight ], 128, Qt::Vertical );
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


void KeramikHandler::pretile( QPixmap *&pix, int size, Qt::Orientation dir )
{
	QPixmap *newpix;
	QPainter p;

	if ( dir == Qt::Horizontal )
		newpix = new QPixmap( size, pix->height() );
	else
		newpix = new QPixmap( pix->width(), size );

	p.begin( newpix );
	p.drawTiledPixmap( newpix->rect(), *pix ) ;
	p.end();

	delete pix;
	pix = newpix;
}


void KeramikHandler::readConfig()
{
	KConfig *c = new KConfig( "kwinkeramikrc" );

	c->setGroup( "General" );
	showIcons = c->readBoolEntry( "ShowAppIcons", true );
	shadowedText = c->readBoolEntry( "UseShadowedText", true );
	smallCaptionBubbles = c->readBoolEntry( "SmallCaptionBubbles", false );
	largeGrabBars = c->readBoolEntry( "LargeGrabBars", true );

	if ( ! settings_cache ) {
		settings_cache = new SettingsCache;

		if ( options->customButtonPositions() ) {
			settings_cache->buttonsLeft  = options->titleButtonsLeft();
			settings_cache->buttonsRight = options->titleButtonsRight();
		} else {
			settings_cache->buttonsLeft  = QString( default_left );
			settings_cache->buttonsRight = QString( default_right );
		}

		settings_cache->aTitleColor   = options->color( Options::TitleBar,   true );
		settings_cache->aTitleBlend   = options->color( Options::TitleBlend, true );
		settings_cache->iTitleColor   = options->color( Options::TitleBar,   false );
		settings_cache->iTitleBlend   = options->color( Options::TitleBlend, false );
		settings_cache->buttonColor   = options->color( Options::ButtonBg,   true );
		settings_cache->showTooltips  = options->showTooltips();
		settings_cache->largeGrabBars = largeGrabBars;
	}

	delete c;
}


QPixmap *KeramikHandler::composite( QImage *over, QImage *under )
{
	QImage dest( over->width(), over->height(), 32 );
	int width = over->width(), height = over->height();

	// Clear the destination image
	Q_UINT32 *data = reinterpret_cast<Q_UINT32*>( dest.bits() );
	for (int i = 0; i < width * height; i++)
		*(data++) = 0;	

	// Copy the under image (bottom aligned) to the destination image
	for (int y1 = height - under->height(), y2 = 0; y1 < height; y1++, y2++ )
	{
		register Q_UINT32 *dst = reinterpret_cast<Q_UINT32*>( dest.scanLine(y1) );
		register Q_UINT32 *src = reinterpret_cast<Q_UINT32*>( under->scanLine(y2) );
	
		for ( int x = 0; x < width; x++ )
			*(dst++) = *(src++);
	}

	// Blend the over image onto the destination
	register Q_UINT32 *dst = reinterpret_cast<Q_UINT32*>( dest.bits() );
	register Q_UINT32 *src = reinterpret_cast<Q_UINT32*>( over->bits() );
	for ( int i = 0; i < width * height; i++ )
	{
		int r1 = qRed( *dst ), g1 = qGreen( *dst ), b1 = qBlue( *dst );
		int r2 = qRed( *src ), g2 = qGreen( *src ), b2 = qBlue( *src );
		int a  = qAlpha( *src );

		if ( a == 0xff )
			*dst = *src;
		
		else if ( a != 0x00 )
			*dst = qRgba( Q_UINT8( r1 + (((r2 - r1) * a) >> 8) ),
		                  Q_UINT8( g1 + (((g2 - g1) * a) >> 8) ),
		                  Q_UINT8( b1 + (((b2 - b1) * a) >> 8) ),
		                  0xff );
		
		else if ( qAlpha(*dst) == 0x00 )
			*dst = 0;
		
		src++; dst++;
	}

	// Create the final pixmap and return it
	return new QPixmap( dest );	
}


QImage *KeramikHandler::loadImage( const QString &name, const QColor &col )
{
	if ( col.isValid() ) {
		QImage *img = new QImage( imageDb->image(name)->copy() );
		KIconEffect::colorize( *img, col, 1.0 );
		return img;
	} else
		return new QImage( imageDb->image(name)->copy() );
}


QPixmap *KeramikHandler::loadPixmap( const QString &name, const QColor &col )
{
	QImage *img = loadImage( name, col );
	QPixmap *pix = new QPixmap( *img );
	delete img;
	
	return pix;
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
	if ( settings_cache->aTitleColor != options->color(Options::TitleBar,   true ) ||
	     settings_cache->aTitleBlend != options->color(Options::TitleBlend, true ) ||
	     settings_cache->iTitleColor != options->color(Options::TitleBar,   false) ||
	     settings_cache->iTitleBlend != options->color(Options::TitleBlend, false) ||
	     settings_cache->buttonColor != options->color(Options::ButtonBg,   true ) )
	{
		pixmapsInvalid = true;
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

	// Check if tooltips options have changed
	if ( (settings_cache->showTooltips != options->showTooltips()) ) {
		needHardReset = true;
	}

	if ( (settings_cache->largeGrabBars != largeGrabBars) ) {
		pixmapsInvalid = true;
		needHardReset = true;
	}

	// Update our config cache
	settings_cache->aTitleColor         = options->color( Options::TitleBar,   true  );
	settings_cache->aTitleBlend         = options->color( Options::TitleBlend, true  );
	settings_cache->iTitleColor         = options->color( Options::TitleBar,   false );
	settings_cache->iTitleBlend         = options->color( Options::TitleBlend, false );
	settings_cache->buttonColor         = options->color( Options::ButtonBg,   true  );
	settings_cache->showTooltips        = options->showTooltips();
	settings_cache->buttonsLeft         = buttonsLeft;
	settings_cache->buttonsRight        = buttonsRight;
	settings_cache->largeGrabBars       = largeGrabBars;	

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
			// The '?' won't be flipped around in the ctor, so we need to
			//  shift it to the right to compensate for the button shadow
			//  being on the left side of the button in RTL mode.
			if ( QApplication::reverseLayout() )
				p->translate( 2, 0 );
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
	QBoxLayout *titleLayout   = new QBoxLayout( 0, QBoxLayout::LeftToRight, 0, 0, 0 );
	QHBoxLayout *windowLayout = new QHBoxLayout();

	largeTitlebar = ( !maximizedVertical() && clientHandler->largeCaptionBubbles() );
	largeCaption = ( isActive() && largeTitlebar );
	
	int grabBarHeight = clientHandler->grabBarHeight();
	int topSpacing = ( largeTitlebar ? 4 : 1 );
	topSpacer = new QSpacerItem( 10, topSpacing,
				QSizePolicy::Expanding, QSizePolicy::Minimum );

	mainLayout->addItem( topSpacer );
	
	mainLayout->addLayout( titleLayout );     // Titlebar
	mainLayout->addLayout( windowLayout, 1 ); // Left border + window + right border
	mainLayout->addSpacing( grabBarHeight );  // Bottom grab bar

	titleLayout->setSpacing( buttonSpacing );
	
	titleLayout->addSpacing( buttonMargin );      // Left button margin
	addButtons( titleLayout, options->customButtonPositions() ?
			options->titleButtonsLeft() : QString(default_left) );

	titlebar = new QSpacerItem( 10, clientHandler->titleBarHeight(largeTitlebar)
			- topSpacing, QSizePolicy::Expanding, QSizePolicy::Minimum );
	titleLayout->addItem( titlebar );

	titleLayout->addSpacing( buttonSpacing );
	addButtons( titleLayout, options->customButtonPositions() ?
				options->titleButtonsRight() : QString(default_right) );
	titleLayout->addSpacing( buttonMargin - 1 );  // Right button margin

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
	if ( clientHandler->largeCaptionBubbles() && !largeTitlebar )
	{
		// We're switching from small caption bubbles to large
		if ( !maximizedVertical() ) {
			topSpacer->changeSize( 10, 4, QSizePolicy::Expanding, QSizePolicy::Minimum );
			largeTitlebar = true;
			largeCaption = isActive();
		
			layout()->activate();
			
			// Compensate for the titlebar size change
			setGeometry( x(), y() - 3, width(), height() + 3 );
		}
	}
	else if ( !clientHandler->largeCaptionBubbles() && largeTitlebar )
	{
		// We're switching from large caption bubbles to small
		topSpacer->changeSize( 10, 1, QSizePolicy::Expanding, QSizePolicy::Minimum );
		largeTitlebar = largeCaption = false;
		
		layout()->activate();
		
		// Compensate for the titlebar size change
		setGeometry( x(), y() + 3, width(), height() - 3 );
	}
	
	calculateCaptionRect();
	
	captionBufferDirty = maskDirty = true;

	// Only repaint the window if it's visible 
	// (i.e. not minimized and on the current desktop)
	if ( isVisible() ) {
		repaint( false );
		
		for ( int i = 0; i < NumButtons; i++ )
			if ( button[i] ) button[i]->repaint( false );
	}
}


void KeramikClient::addButtons( QBoxLayout *layout, const QString &s )
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
				if ( !button[CloseButton] && isCloseable() ) {
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

	// To maximize performance this code uses precalculated bounding rects
	// to set the window mask. This saves us from having to allocate a 1bpp
	// pixmap, paint the mask on it and then have the X server iterate
	// over the pixels to compute the bounding rects from it.

	// To elliminate any possible overhead introduced by Qt it uses Xlib
	// calls directly, which also has the advantage that it allows us to
	// optimize the sorting of the rectangles.

	XRectangle rects[11];
	register XRectangle *rect = rects;
	register int w, y = 0;
	int nrects;

	if ( QApplication::reverseLayout() ) {

		// If the caption bubble is visible and extends above the titlebar
		if ( largeCaption && captionRect.width() >= 25 ) {
			nrects = 11;
			register int x = captionRect.left();
			w = captionRect.width();
			setRectangle( rect++, x + 11, y++, w - 19, 1 );
			setRectangle( rect++, x + 9,  y++, w - 15, 1 );
			setRectangle( rect++, x + 7,  y++, w - 12, 1 );
		} else {
			nrects = 8;
			
			// Do we have a large titlebar with a retracted caption bubble?
			// (i.e. the style is set to use large caption bubbles, we're
			//       not maximized and not active)
			if ( largeTitlebar )
				y = 3;
		}
	
		w = width();

		// The rounded titlebar corners
		setRectangle( rect++, 9, y++, w - 17, 1 );
		setRectangle( rect++, 7, y++, w - 13, 1 );
		setRectangle( rect++, 5, y++, w - 9,  1 );
		setRectangle( rect++, 4, y++, w - 7,  1 );
		setRectangle( rect++, 3, y++, w - 5,  1 );
		setRectangle( rect++, 2, y++, w - 4,  1 );
		setRectangle( rect++, 1, y++, w - 2,  2 );
	} else {

		// If the caption bubble is visible and extends above the titlebar
		if ( largeCaption && captionRect.width() >= 25 ) {
			nrects = 11;
			register int x = captionRect.left();
			w = captionRect.width();
			setRectangle( rect++, x + 8, y++, w - 19, 1 );
			setRectangle( rect++, x + 6, y++, w - 15, 1 );
			setRectangle( rect++, x + 5, y++, w - 12, 1 );
		} else {
			nrects = 8;

			// Do we have a large titlebar with a retracted caption bubble?
			// (i.e. the style is set to use large caption bubbles, we're
			//       not maximized and not active)
			if ( largeTitlebar )
				y = 3;
		}

		w = width();

		// The rounded titlebar corners
		setRectangle( rect++, 8, y++, w - 17, 1 );
		setRectangle( rect++, 6, y++, w - 13, 1 );
		setRectangle( rect++, 4, y++, w - 9,  1 );
		setRectangle( rect++, 3, y++, w - 7,  1 );
		setRectangle( rect++, 2, y++, w - 5,  1 );
		setRectangle( rect++, 2, y++, w - 4,  1 );
		setRectangle( rect++, 1, y++, w - 2,  2 );
	}

	y++;

	// The part of the window below the titlebar
	setRectangle( rect++, 0, y, w, height() - y );

	// Send the rects to the X server
	XShapeCombineRectangles( x11AppDisplay(), handle(), ShapeBounding,
		0, 0, rects, nrects, ShapeSet, YXBanded );

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
	if ( active && largeCaption ) {
		p.drawPixmap( 0, 0, *clientHandler->tile( CaptionLargeLeft, true ) );
		p.drawTiledPixmap( 15, 0, captionRect.width() - 30, captionRect.height(),
				*clientHandler->tile( CaptionLargeCenter, true ) );
		p.drawPixmap( captionRect.width() - 15, 0, *clientHandler->tile( CaptionLargeRight, true ) );
	} else {
		p.drawPixmap( 0, 0, *clientHandler->tile( CaptionSmallLeft, active ) );
		p.drawTiledPixmap( 15, 0, captionRect.width() - 30, captionRect.height(),
				*clientHandler->tile( CaptionSmallCenter, active ) );
		p.drawPixmap( captionRect.width() - 15, 0, *clientHandler->tile( CaptionSmallRight, active ) );
	}

	if ( clientHandler->showAppIcons() )
	{
		if ( active ) {
			if ( ! activeIcon )
				activeIcon = new QPixmap( miniIcon() );
			icon = activeIcon;
		} else {
			if ( ! inactiveIcon ) {
				QImage img = miniIcon().convertToImage();
				KIconEffect::semiTransparent( img );
				inactiveIcon = new QPixmap( img );
			}
			icon = inactiveIcon;
		}
	}

	p.setFont( options->font( active ) );
	int tw = p.fontMetrics().width( caption() ) +
		( clientHandler->showAppIcons() ? 16 + iconSpacing : 0 );

	int xpos = QMAX( (captionRect.width() - tw) / 3, 8 );
	QRect tr = QStyle::visualRect( QRect(xpos, 1, captionRect.width() - xpos - 10,
				captionRect.height() - 4), captionBuffer.rect() );

	//p.setPen( Qt::red ); // debug
	//p.drawRect( tr );    // debug

	// Application icon
	if ( clientHandler->showAppIcons() )
	{
		QRect iconRect = QStyle::visualRect( QRect(tr.x(),
					1 + (captionRect.height() - 4 - 16) / 2, 16, 16), tr );
		QRect r( icon->rect() );
		r.moveCenter( iconRect.center() );

		if ( tr.width() > 16 ) {
			p.drawPixmap( r, *icon );
		} else {
			QRect sr( 0, 0, icon->width(), icon->height() );

			if ( QApplication::reverseLayout() )
				sr.addCoords( icon->width() - tr.width(), 0, 0, 0 );
			else
				sr.addCoords( 0, 0, -( icon->width() - tr.width() ), 0 ); 

			p.drawPixmap( r.x() + sr.x(), r.y() + sr.y(), *icon,
					sr.x(), sr.y(), sr.width(), sr.height() );
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
		p.setPen( options->color(Options::TitleBar, active).dark() );
		p.drawText( tr, flags, caption() );
		p.translate( QApplication::reverseLayout() ? 1 : -1, -1 );
	}

	p.setPen( options->color( Options::Font, active ) );
	p.drawText( tr, flags, caption() );

	captionBufferDirty = false;
}


void KeramikClient::calculateCaptionRect()
{
	QFontMetrics fm( options->font(isActive()) );
	int cw = fm.width( caption() ) + 95;
	int titleBaseY = ( largeTitlebar ? 3 : 0 );

	if ( clientHandler->showAppIcons() )
		cw += 16 + 4; // icon width + space

	cw = QMIN( cw, titlebar->geometry().width() );
	captionRect = QStyle::visualRect( QRect(titlebar->geometry().x(), (largeCaption ? 0 : titleBaseY),
				cw, clientHandler->titleBarHeight(largeCaption) ),
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


void KeramikClient::activeChange( bool active )
{
	// Note: It's assumed that the same font will always be used for both active
	//       and inactive windows, since the fonts kcm hasn't supported setting
	//       different fonts for different window states for some time.
	if ( largeTitlebar ) {
		largeCaption = ( active && !maximizedVertical() );
		calculateCaptionRect();
		maskDirty = true;
	}
	
	captionBufferDirty = true;
	
	repaint( false );

	for ( int i=0; i < NumButtons; i++ )
		if ( button[i] ) button[i]->repaint( false );
}


void KeramikClient::maximizeChange( bool maximized )
{
	if ( clientHandler->largeCaptionBubbles() )
	{
		if ( maximized && maximizeMode() != MaximizeHorizontal ) {
			// We've been maximized - shrink the titlebar by 3 pixels
			topSpacer->changeSize( 10, 1, QSizePolicy::Expanding, QSizePolicy::Minimum );
			largeCaption = largeTitlebar = false;

			calculateCaptionRect();
			captionBufferDirty = maskDirty = true;
			
			layout()->activate();
			repaint( false );
		} else if ( !maximized && !largeTitlebar ) {
			// We've been restored - enlarge the titlebar by 3 pixels
			topSpacer->changeSize( 10, 4, QSizePolicy::Expanding, QSizePolicy::Minimum );
			largeCaption = largeTitlebar = true;
			
			calculateCaptionRect();
			captionBufferDirty = maskDirty = true;
			
			layout()->activate();	
			repaint( false );
		}
	}
	
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
	QPoint menuPoint ( button[MenuButton]->rect().bottomLeft().x() - 6, 
					   button[MenuButton]->rect().bottomLeft().y() + 3 );
	workspace()->showWindowMenu( button[MenuButton]->mapToGlobal( menuPoint ), this );
	button[MenuButton]->setDown(false);
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

	int titleBaseY         = ( largeTitlebar ? 3 : 0 );
	int titleBarHeight     = clientHandler->titleBarHeight( largeTitlebar );
	int grabBarHeight      = clientHandler->grabBarHeight();
	int leftBorderWidth    = clientHandler->tile( BorderLeft, active )->width();
	int rightBorderWidth   = clientHandler->tile( BorderRight, active )->width();

	if ( maskDirty )
		updateMask();

	// Titlebar
	// -----------------------------------------------------------------------
	if ( updateRect.y() < titleBarHeight )
	{
		int titleBarBaseHeight = titleBarHeight - titleBaseY;
		
		if ( captionBufferDirty )
			updateCaptionBuffer();
		
		// Top left corner
		if ( updateRect.x() < 15 )
			p.drawPixmap( 0, titleBaseY,
					*clientHandler->tile( TitleLeft, active ) );

		// Space between the top left corner and the caption bubble
		if ( updateRect.x() < captionRect.left() && updateRect.right() >= 15 ) {
			int x1 = QMAX( 15, updateRect.x() );
			int x2 = QMIN( captionRect.left(), updateRect.right() );
		
			p.drawTiledPixmap( x1, titleBaseY, x2 - x1 + 1, titleBarBaseHeight,
					*clientHandler->tile( TitleCenter, active ) );
		}

		// Caption bubble
		if ( updateRect.x() <= captionRect.right() && updateRect.right() > 15 ) {
			if ( captionRect.width() >= 25 )
				p.drawPixmap( captionRect.left(), active ? 0 : titleBaseY, captionBuffer );
			else
				p.drawTiledPixmap( captionRect.x(), titleBaseY, captionRect.width(),
						titleBarBaseHeight, *clientHandler->tile( TitleCenter, active ) );
		}
		
		// Space between the caption bubble and the top right corner
		if ( updateRect.right() > captionRect.right() && updateRect.x() < width() - 15 ) {
			int x1 = QMAX( captionRect.right() + 1, updateRect.x() );
			int x2 = QMIN( width() - 15, updateRect.right() );
			
			p.drawTiledPixmap( x1, titleBaseY, x2 - x1 + 1, titleBarBaseHeight,
					*clientHandler->tile( TitleCenter, active ) );
		}

		// Top right corner
		if ( updateRect.right() >= width() - 15 )
			p.drawPixmap( width() - 15, titleBaseY,
					*clientHandler->tile( TitleRight, active ) );
	}

	// Borders
	// -----------------------------------------------------------------------
	if ( updateRect.bottom() >= titleBarHeight &&
			updateRect.top() < height() - grabBarHeight )
	{
		int top    = QMAX( titleBarHeight, updateRect.top() );
		int bottom = QMIN( updateRect.bottom(), height() - grabBarHeight );
		
		// Left border
		if ( updateRect.x() < leftBorderWidth )
			p.drawTiledPixmap( 0, top, leftBorderWidth, bottom - top + 1,
					*clientHandler->tile( BorderLeft, active ) );

		// Right border
		if ( e->rect().right() > width() - rightBorderWidth - 1 )
			p.drawTiledPixmap( width() - rightBorderWidth, top, rightBorderWidth,
					bottom - top + 1, *clientHandler->tile( BorderRight, active ) );
	}
	
	// Bottom grab bar
	// -----------------------------------------------------------------------
	if ( updateRect.bottom() >= height() - grabBarHeight ) {
		// Bottom left corner
		if ( updateRect.x() < 9 )
			p.drawPixmap( 0, height() - grabBarHeight,
					*clientHandler->tile( GrabBarLeft, active ) );

		// Space between the left corner and the right corner
		if ( updateRect.x() < width() - 9 ) {
			int x1 = QMAX( 9, updateRect.x() );
			int x2 = QMIN( width() - 9, updateRect.right() );
			
			p.drawTiledPixmap( x1, height() - grabBarHeight, x2 - x1 + 1,
					grabBarHeight, *clientHandler->tile( GrabBarCenter, active ) );
		}

		// Bottom right corner
		if ( updateRect.right() > width() - 9 )
			p.drawPixmap( width() - 9, height() - grabBarHeight,
					*clientHandler->tile( GrabBarRight, active ) );
	}

	// Extra drawline for the 1 pixel empty space QLayout leaves when a window is shaded.
	p.setPen( options->color( Options::TitleBlend, active ) );
	p.drawLine( leftBorderWidth, height() - grabBarHeight - 1, 
				width() - rightBorderWidth - 1, height() - grabBarHeight - 1 );
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
			update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4,
							titlebar->geometry().bottom() ) ) );
			// Titlebar needs no paint event
			QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
		}
	}
}


void KeramikClient::mouseDoubleClickEvent( QMouseEvent *e )
{
	if ( QRect( 0, 0, width(), clientHandler->titleBarHeight( largeTitlebar ) ).contains( e->pos() ) )
		workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
}


Client::MousePosition KeramikClient::mousePosition( const QPoint &p ) const
{
	int titleBaseY = (largeTitlebar ? 3 : 0);

	int leftBorder   = clientHandler->tile( BorderLeft, true )->width();
	int rightBorder  = width() - clientHandler->tile( BorderRight, true )->width() - 1;
	int bottomBorder = height() - clientHandler->grabBarHeight() - 1;

	// Test if the mouse is over the titlebar area
	if ( p.y() < titleBaseY + 11 ) {
		// Test for the top left corner
		if ( p.x() < leftBorder + 11 ) {
			if ( (p.y() < titleBaseY + 3 && p.x() < leftBorder + 11) ||
					(p.y() < titleBaseY + 6 && p.x() < leftBorder + 6) ||
					(p.y() < titleBaseY + 11 && p.x() < leftBorder + 3) )
				return TopLeft;
		}

		// Test for the top right corner
		if ( p.x() > rightBorder - 11 ) {
			if ( (p.y() < titleBaseY + 3 && p.x() > rightBorder - 11) ||
					(p.y() < titleBaseY + 6 && p.x() > rightBorder - 6) ||
					(p.y() < titleBaseY + 11 && p.x() > rightBorder - 3) )
				return TopRight;
		}

		// Test for the top border
		if ( p.y() <= 3 || (p.y() <= titleBaseY+3 &&
					(p.x() < captionRect.left() || p.x() > captionRect.right()) ) )
			return Top;

		// The cursor must be over the center of the titlebar.
		return Center;
	}

	// Test the sides
	else if ( p.y() < bottomBorder ) {
		// Test for the left side
		if ( p.x() < leftBorder ) {
			if ( p.y() < height() - bottomCornerSize )
				return Left;
			else
				return BottomLeft;
		}

		// Test for the right side
		else if ( p.x() > rightBorder ) {
			if ( p.y() < height() - bottomCornerSize )
				return Right;
			else
				return BottomRight;
		}

		// The cursor must be over the center of the window
		return Center;
	}

	// Test the grab bar / bottom border
	else {
		// Test for the bottom left corner
		if ( p.x() < bottomCornerSize )
			return BottomLeft;

		// Test for the bottom right corner
		else if ( p.x() > width() - bottomCornerSize - 1 )
			return BottomRight;

		// The cursor must be over the bottom border
		return Bottom;
	}

	// We should never get here
	return Nowhere;
}

} // namespace Keramik



// -------------------------------------------------------------------------------------------



extern "C"
{
	Client *allocate( Workspace *ws, WId w )
	{
		return new Keramik::KeramikClient( ws, w );
	}
	
	void init()
	{
		Keramik::clientHandler = new Keramik::KeramikHandler();
	}
	
	void reset()
	{
		Keramik::clientHandler->reset();
	}
	
	void deinit()
	{
		delete Keramik::clientHandler;
	}
}



// vim: set noet ts=4 sw=4:
