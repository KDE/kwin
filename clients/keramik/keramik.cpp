/*
 *
 * Keramik KWin client (version 0.8)
 *
 * Copyright (C) 2002 Fredrik H�lund <fredrik@kde.org>
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <kconfig.h>
#include <klocale.h>
#include <kiconeffect.h>

#include <qpainter.h>
#include <qlayout.h>
#include <qbitmap.h>
#include <qstyle.h>
#include <qtooltip.h>
#include <qwidget.h>
#include <qlabel.h>
#include <QEvent>
#include <QApplication>

#include <X11/Xlib.h>

#include "keramik.h"
#include "keramik.moc"



// -------------------------------------------------------------------------------------------



namespace Keramik
{

	const int buttonMargin     =  9;  // Margin between the window edge and the buttons
	const int buttonSpacing    =  4;  // Spacing between the titlebar buttons
	const int iconSpacing      =  5;  // Spacing between the icon and the text label

	// Default button layout
	const char default_left[]  = "M";
	const char default_right[] = "HIAX";

	// Titlebar button bitmaps
	const unsigned char menu_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0xf0, 0x07, 0x00,
	   0xe0, 0x03, 0x00, 0xc0, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char on_all_desktops_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char not_on_all_desktops_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char help_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00,
	   0xf0, 0x07, 0x00, 0x30, 0x06, 0x00, 0x00, 0x07, 0x00, 0x80, 0x03, 0x00,
	   0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char minimize_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char maximize_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x80, 0x01, 0x00, 0xc0, 0x03, 0x00, 0xe0, 0x07, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char restore_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
	   0xf0, 0x0f, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char close_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x30, 0x0c, 0x00, 0x70, 0x0e, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00,
	   0xc0, 0x03, 0x00, 0xe0, 0x07, 0x00, 0x70, 0x0e, 0x00, 0x30, 0x0c, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00};

	const unsigned char above_on_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x80, 0x01, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00 };

	const unsigned char above_off_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0xc0, 0x03, 0x00,
	   0xe0, 0x07, 0x00, 0x80, 0x01, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00 };

	const unsigned char below_on_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0xc0, 0x03, 0x00,
	   0xe0, 0x07, 0x00, 0x80, 0x01, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00 };

	const unsigned char below_off_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x80, 0x01, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00, 0x80, 0x01, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00 };

	const unsigned char shade_on_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x10, 0x08, 0x00, 0x10, 0x08, 0x00, 0x10, 0x08, 0x00, 0x10, 0x08, 0x00,
	   0x10, 0x08, 0x00, 0x10, 0x08, 0x00, 0x10, 0x08, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00 };

	const unsigned char shade_off_bits[] = {
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x0f, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00 };

	KeramikHandler *clientHandler = NULL;
	bool keramik_initialized = false;



// -------------------------------------------------------------------------------------------



KeramikHandler::KeramikHandler()
{
	for ( int i = 0; i < NumTiles; i++ ) {
		activeTiles[i]   = NULL;
		inactiveTiles[i] = NULL;
	}

	settings_cache = NULL;

	// Create the button deco bitmaps
	buttonDecos[ Menu ]             = new QBitmap( 17, 17, menu_bits,       true );
	buttonDecos[ OnAllDesktops ]    = new QBitmap( 17, 17, on_all_desktops_bits,  true );
	buttonDecos[ NotOnAllDesktops ] = new QBitmap( 17, 17, not_on_all_desktops_bits, true );
	buttonDecos[ Help ]             = new QBitmap( 17, 17, help_bits,       true );
	buttonDecos[ Minimize ]         = new QBitmap( 17, 17, minimize_bits,   true );
	buttonDecos[ Maximize ]         = new QBitmap( 17, 17, maximize_bits,   true );
	buttonDecos[ Restore ]          = new QBitmap( 17, 17, restore_bits,    true );
	buttonDecos[ Close ]            = new QBitmap( 17, 17, close_bits,      true );
	buttonDecos[ AboveOn ]          = new QBitmap( 17, 17, above_on_bits,   true );
	buttonDecos[ AboveOff ]         = new QBitmap( 17, 17, above_off_bits,  true );
	buttonDecos[ BelowOn ]          = new QBitmap( 17, 17, below_on_bits,   true );
	buttonDecos[ BelowOff ]         = new QBitmap( 17, 17, below_off_bits,  true );
	buttonDecos[ ShadeOn ]          = new QBitmap( 17, 17, shade_on_bits,   true );
	buttonDecos[ ShadeOff ]         = new QBitmap( 17, 17, shade_off_bits,  true );

	// Selfmask the bitmaps
	for ( int i = 0; i < NumButtonDecos; i++ )
		buttonDecos[i]->setMask( *buttonDecos[i] );

	// Flip the bitmaps horizontally in right-to-left mode
	if ( QApplication::isRightToLeft() ) {
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

	delete settings_cache;

        clientHandler = NULL;
}


void KeramikHandler::createPixmaps()
{
	int heightOffset;
	int widthOffset;
	switch(options()->preferredBorderSize(this)) {
	case BorderLarge:
		widthOffset = 4;
		heightOffset = 0;
		break;
	case BorderVeryLarge:
		widthOffset = 8;
		heightOffset = 0;
		break;
	case BorderHuge:
		widthOffset = 14;
		heightOffset = 0;
		break;
	case BorderVeryHuge:
		widthOffset = 23;
		heightOffset = 10;
		break;
	case BorderOversized:
		widthOffset = 36;
		heightOffset = 25;
		break;
	case BorderTiny:
	case BorderNormal:
	default:
		widthOffset = 0;
		heightOffset = 0;
	}
	int fontHeight = QFontMetrics(options()->font(true)).height();
	if (fontHeight > heightOffset + 20)
		heightOffset = fontHeight - 20;

	QString size = (heightOffset < 8) ? "" : (heightOffset < 20) ? "-large" : "-huge";

	QColor titleColor, captionColor, buttonColor;
	QImage *titleCenter = NULL, *captionLeft = NULL,
		*captionRight = NULL, *captionCenter = NULL;


	// Active tiles
	// -------------------------------------------------------------------------
	captionColor = KDecoration::options()->color( ColorTitleBar,   true );
	titleColor   = KDecoration::options()->color( ColorTitleBlend, true );

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
	captionColor = KDecoration::options()->color( ColorTitleBar,   false );
	titleColor   = KDecoration::options()->color( ColorTitleBlend, false );

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
	buttonColor  = QColor(); //KDecoration::options()->color( ButtonBg, true );

	titleButtonRound  = loadPixmap( "titlebutton-round"+size,  buttonColor );
	titleButtonSquare = loadPixmap( "titlebutton-square"+size, buttonColor );


	// Prepare the tiles for use
	// -------------------------------------------------------------------------
	if ( QApplication::isRightToLeft() ) {

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

	if (heightOffset > 0) {
		addHeight (heightOffset, activeTiles[TitleLeft]);
		addHeight (heightOffset, activeTiles[TitleCenter]);
		addHeight (heightOffset, activeTiles[TitleRight]);
		addHeight (heightOffset, activeTiles[CaptionSmallLeft]);
		addHeight (heightOffset, activeTiles[CaptionSmallCenter]);
		addHeight (heightOffset, activeTiles[CaptionSmallRight]);
		addHeight (heightOffset, activeTiles[CaptionLargeLeft]);
		addHeight (heightOffset, activeTiles[CaptionLargeCenter]);
		addHeight (heightOffset, activeTiles[CaptionLargeRight]);

		addHeight (heightOffset, inactiveTiles[TitleLeft]);
		addHeight (heightOffset, inactiveTiles[TitleCenter]);
		addHeight (heightOffset, inactiveTiles[TitleRight]);
		addHeight (heightOffset, inactiveTiles[CaptionSmallLeft]);
		addHeight (heightOffset, inactiveTiles[CaptionSmallCenter]);
		addHeight (heightOffset, inactiveTiles[CaptionSmallRight]);
	}

	if (widthOffset > 0) {
		addWidth (widthOffset, activeTiles[BorderLeft], true, activeTiles[GrabBarCenter]);
		addWidth (widthOffset, activeTiles[BorderRight], false, activeTiles[GrabBarCenter]);
		addWidth (widthOffset, inactiveTiles[BorderLeft], true, inactiveTiles[GrabBarCenter]);
		addWidth (widthOffset, inactiveTiles[BorderRight], false, inactiveTiles[GrabBarCenter]);

		if (largeGrabBars)
			widthOffset = widthOffset*3/2;

		addHeight (widthOffset, activeTiles[GrabBarLeft]);
		addHeight (widthOffset, activeTiles[GrabBarCenter]);
		addHeight (widthOffset, activeTiles[GrabBarRight]);
		addHeight (widthOffset, inactiveTiles[GrabBarLeft]);
		addHeight (widthOffset, inactiveTiles[GrabBarCenter]);
		addHeight (widthOffset, inactiveTiles[GrabBarRight]);
	}
}



void KeramikHandler::destroyPixmaps()
{
	for ( int i = 0; i < NumTiles; i++ ) {
		delete activeTiles[i];
		delete inactiveTiles[i];
		activeTiles[i] = NULL;
		inactiveTiles[i] = NULL;
	}

	delete titleButtonRound;
	delete titleButtonSquare;
}


void KeramikHandler::addWidth (int width, QPixmap *&pix, bool left, QPixmap *bottomPix) {
	int w = pix->width()+width;
	int h = pix->height();

	QPixmap *tmp = new QPixmap (w, h);
	tmp->fill ();
	QPainter p;
	p.begin (tmp);

	for (int i = 0; i < h; i++)
		p.drawPixmap (0, i, *bottomPix, i%2, 0, w,1);

	if (left)
		p.drawPixmap(0, 0, *pix);
	else
		p.drawPixmap(width, 0, *pix);

	p.end();

	delete pix;
	pix = tmp;
}


void KeramikHandler::addHeight (int height, QPixmap *&pix) {
	int w = pix->width();
	int h = pix->height()+height;

	QPixmap *tmp = new QPixmap (w, h);
	QPainter p;
	p.begin (tmp);
	if (pix->height() > 10) {
		p.drawPixmap(0, 0, *pix, 0, 0, w, 11);
		for (int i = 0; i < height; i+=2)
			p.drawPixmap(0, 11+i, *pix, 0, 11, w, 2);
		p.drawPixmap(0, 11+height, *pix, 0, 11, w, -1);
	}
	else {
		int lines  = h-3;
		int factor = pix->height()-3;
		for (int i = 0; i < lines; i++)
			p.drawPixmap(0, i, *pix, 0, i*factor/lines, w, 1);
		p.drawPixmap(0, lines, *pix, 0, factor, w, 3);
	}
	p.end();

	delete pix;
	pix = tmp;
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
		settings_cache->largeGrabBars = largeGrabBars;
		settings_cache->smallCaptionBubbles = smallCaptionBubbles;
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
		QImage *img = new QImage( ":/pics/" + name + ".png" );
		KIconEffect::colorize( *img, col, 1.0 );
		return img;
	} else
		return new QImage( ":/pics/" + name + ".png" );
}


QPixmap *KeramikHandler::loadPixmap( const QString &name, const QColor &col )
{
	QImage *img = loadImage( name, col );
	QPixmap *pix = new QPixmap( *img );
	delete img;

	return pix;
}


bool KeramikHandler::reset( unsigned long changed )
{
	keramik_initialized = false;

	bool needHardReset  = false;
	bool pixmapsInvalid = false;

	// Re-read the config file
	readConfig();

	if ( changed & SettingBorder )
	{
		pixmapsInvalid = true;
		needHardReset = true;
	}
	if ( changed & SettingFont )
	{
		pixmapsInvalid = true;
		needHardReset = true;
	}
	// Check if the color scheme has changed
	if ( changed & SettingColors )
	{
		pixmapsInvalid = true;
	}
	// Check if button positions have changed

	if ( changed & SettingButtons ) {
		needHardReset = true;
	}

	// Check if tooltips options have changed
	if ( changed & SettingTooltips ) {
		needHardReset = true;
	}

	if ( (settings_cache->largeGrabBars != largeGrabBars) ) {
		pixmapsInvalid = true;
		needHardReset = true;
	}

	if ( (settings_cache->smallCaptionBubbles != smallCaptionBubbles) ) {
		needHardReset = true;
	}

	// Update our config cache
	settings_cache->largeGrabBars       = largeGrabBars;
	settings_cache->smallCaptionBubbles = smallCaptionBubbles;

	// Do we need to recreate the pixmaps?
	if ( pixmapsInvalid ) {
		destroyPixmaps();
		createPixmaps();
	}

	keramik_initialized = true;

	// Do we need to "hit the wooden hammer" ?
	if ( !needHardReset )
		resetDecorations( changed );
        return needHardReset;
}


bool KeramikHandler::supports( Ability ability )
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


const QPixmap *KeramikHandler::tile( TilePixmap tilePix, bool active ) const
{
	return ( active ? activeTiles[ tilePix ] : inactiveTiles[ tilePix ] );
}

KDecoration* KeramikHandler::createDecoration( KDecorationBridge* bridge )
{
        return new KeramikClient( bridge, this );
}

QList< KeramikHandler::BorderSize > KeramikHandler::borderSizes() const
{ // the list must be sorted
  return QList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}


// -------------------------------------------------------------------------------------------



KeramikButton::KeramikButton( KeramikClient* c, const char *name, Button btn, const QString &tip, const int realizeBtns )
		: Q3Button( c->widget(), name ),
		client( c ), button( btn ), hover( false ), lastbutton( Qt::NoButton )
{
	realizeButtons = realizeBtns;

	this->setToolTip( tip ); // FRAME
	setBackgroundMode( Qt::NoBackground );
	setCursor( Qt::ArrowCursor );
	int size = clientHandler->roundButton()->height();
	setFixedSize( size, size );

	setToggleButton( (button == OnAllDesktopsButton) );
}


KeramikButton::~KeramikButton()
{
	// Empty.
}


void KeramikButton::enterEvent( QEvent *e )
{
	Q3Button::enterEvent( e );

	hover = true;
	repaint( false );
}


void KeramikButton::leaveEvent( QEvent *e )
{
	Q3Button::leaveEvent( e );

	hover = false;
	repaint( false );
}


void KeramikButton::mousePressEvent( QMouseEvent *e )
{
	lastbutton = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(), (e->button()&realizeButtons)?Qt::LeftButton : Qt::NoButton, e->state() );
	Q3Button::mousePressEvent( &me );
}


void KeramikButton::mouseReleaseEvent( QMouseEvent *e )
{
	lastbutton = e->button();
	QMouseEvent me( e->type(), e->pos(), e->globalPos(), (e->button()&realizeButtons)?Qt::LeftButton : Qt::NoButton, e->state() );
	Q3Button::mouseReleaseEvent( &me );
}


void KeramikButton::drawButton( QPainter *p )
{
	const QPixmap *pix;
	const QBitmap *deco;
	int size = clientHandler->roundButton()->height();

	// Get the bevel from the client handler
	if ( button == MenuButton || button == OnAllDesktopsButton || button == HelpButton )
		pix = clientHandler->roundButton();
	else
		pix = clientHandler->squareButton();

	// Draw the button background
	const QPixmap *background = clientHandler->tile( TitleCenter, client->isActive() );
	p->drawPixmap( 0, 0, *background,
			0, (background->height()-size+1)/2, size, size );

	if ( isDown() ) {
		// Pressed
		p->drawPixmap( QPoint(), *pix, QStyle::visualRect( QApplication::isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, QRect(2*size, 0, size, size), pix->rect() ) );
		p->translate( QApplication::isRightToLeft() ? -1 : 1,  1 );
	} else if ( hover )
		// Mouse over
		p->drawPixmap( QPoint(), *pix, QStyle::visualRect( QApplication::isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, QRect(size, 0, size, size), pix->rect() ) );
	else
		// Normal
		p->drawPixmap( QPoint(), *pix, QStyle::visualRect( QApplication::isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, QRect(0, 0, size, size), pix->rect() ) );


	// Draw the button deco on the bevel
	switch ( button ) {
		case MenuButton:
			deco = clientHandler->buttonDeco( Menu );
			break;

		case OnAllDesktopsButton:
			deco = clientHandler->buttonDeco( client->isOnAllDesktops() ? NotOnAllDesktops : OnAllDesktops );
			break;

		case HelpButton:
			deco = clientHandler->buttonDeco( Help );
			// The '?' won't be flipped around in the ctor, so we need to
			//  shift it to the right to compensate for the button shadow
			//  being on the left side of the button in RTL mode.
			if ( QApplication::isRightToLeft() )
				p->translate( 2, 0 );
			break;

		case MinButton:
			deco = clientHandler->buttonDeco( Minimize );
			break;

		case MaxButton:
			deco = clientHandler->buttonDeco( client->maximizeMode() == KeramikClient::MaximizeFull ? Restore : Maximize );
			break;

		case CloseButton:
			deco = clientHandler->buttonDeco( Close );
			break;

		case AboveButton:
			deco = clientHandler->buttonDeco( client->keepAbove() ? AboveOn : AboveOff );
			break;

		case BelowButton:
			deco = clientHandler->buttonDeco( client->keepBelow() ? BelowOn : BelowOff );
			break;

		case ShadeButton:
			deco = clientHandler->buttonDeco( client->isSetShade() ? ShadeOn : ShadeOff );
			break;

		default:
			deco = NULL;
	}

	p->setPen( Qt::black ); // ### hardcoded color
	p->drawPixmap( (size-17)/2, (size-17)/2, *deco );
}



// ------------------------------------------------------------------------------------------



KeramikClient::KeramikClient( KDecorationBridge* bridge, KDecorationFactory* factory )
		: KDecoration( bridge, factory ),
		activeIcon( NULL ), inactiveIcon( NULL ), captionBufferDirty( true ), maskDirty( true )
{
}

void KeramikClient::init()
{
	connect( this, SIGNAL( keepAboveChanged( bool )), SLOT( keepAboveChange( bool )));
	connect( this, SIGNAL( keepBelowChanged( bool )), SLOT( keepBelowChange( bool )));

	createMainWidget( Qt::WResizeNoErase );
	widget()->setAttribute( Qt::WA_StaticContents );
	widget()->installEventFilter( this );

	// Minimize flicker
	widget()->setBackgroundMode( Qt::NoBackground );

	for ( int i=0; i < NumButtons; i++ )
		button[i] = NULL;

	createLayout();
}

void KeramikClient::createLayout()
{

	QVBoxLayout *mainLayout   = new QVBoxLayout( widget() );
	QBoxLayout *titleLayout   = new QBoxLayout( 0, QBoxLayout::LeftToRight, 0, 0, 0 );
	QHBoxLayout *windowLayout = new QHBoxLayout();

	largeTitlebar = ( !maximizedVertical() && clientHandler->largeCaptionBubbles() );
	largeCaption = ( isActive() && largeTitlebar );

	int grabBarHeight = clientHandler->grabBarHeight();
	int topSpacing = ( largeTitlebar ? 4 : 1 );
	int leftBorderWidth    = clientHandler->tile( BorderLeft, true )->width();
	int rightBorderWidth   = clientHandler->tile( BorderRight, true )->width();
	topSpacer = new QSpacerItem( 10, topSpacing,
				QSizePolicy::Expanding, QSizePolicy::Minimum );

	mainLayout->addItem( topSpacer );

	mainLayout->addLayout( titleLayout );     // Titlebar
	mainLayout->addLayout( windowLayout, 1 ); // Left border + window + right border
	mainLayout->addSpacing( grabBarHeight );  // Bottom grab bar

	titleLayout->setSpacing( buttonSpacing );

	titleLayout->addSpacing( buttonMargin );      // Left button margin
	addButtons( titleLayout, options()->customButtonPositions() ?
			options()->titleButtonsLeft() : QString(default_left) );

	titlebar = new QSpacerItem( 10, clientHandler->titleBarHeight(largeTitlebar)
			- topSpacing, QSizePolicy::Expanding, QSizePolicy::Minimum );
	titleLayout->addItem( titlebar );

	titleLayout->addSpacing( buttonSpacing );
	addButtons( titleLayout, options()->customButtonPositions() ?
				options()->titleButtonsRight() : QString(default_right) );
	titleLayout->addSpacing( buttonMargin - 1 );  // Right button margin

	windowLayout->addSpacing( leftBorderWidth );                // Left border
        if( isPreview())
	    windowLayout->addWidget( new QLabel( i18n( "<center><b>Keramik preview</b></center>" ), widget()));
        else
            windowLayout->addItem( new QSpacerItem( 0, 0 )); //no widget in the middle
	windowLayout->addSpacing( rightBorderWidth );                // Right border
}


KeramikClient::~KeramikClient()
{
	delete activeIcon;
	delete inactiveIcon;

	activeIcon = inactiveIcon = NULL;
}


void KeramikClient::reset( unsigned long )
{
	if ( clientHandler->largeCaptionBubbles() && !largeTitlebar )
	{
		// We're switching from small caption bubbles to large
		if ( !maximizedVertical() ) {
			topSpacer->changeSize( 10, 4, QSizePolicy::Expanding, QSizePolicy::Minimum );
			largeTitlebar = true;
			largeCaption = isActive();

			widget()->layout()->activate();

			// Compensate for the titlebar size change

			// TODO This is wrong, this may break size increments (see bug #53784).
			// FRAME
			widget()->setGeometry( widget()->x(), widget()->y() - 3, width(), height() + 3 );
		}
	}
	else if ( !clientHandler->largeCaptionBubbles() && largeTitlebar )
	{
		// We're switching from large caption bubbles to small
		topSpacer->changeSize( 10, 1, QSizePolicy::Expanding, QSizePolicy::Minimum );
		largeTitlebar = largeCaption = false;

		widget()->layout()->activate();

		// Compensate for the titlebar size change
		// FRAME
		widget()->setGeometry( widget()->x(), widget()->y() + 3, width(), height() - 3 );
	}

	calculateCaptionRect();

	captionBufferDirty = maskDirty = true;

	// Only repaint the window if it's visible
	// (i.e. not minimized and on the current desktop)
	if ( widget()->isVisible() ) {
		widget()->repaint( false );

		for ( int i = 0; i < NumButtons; i++ )
			if ( button[i] ) button[i]->repaint( false );
	}
}


void KeramikClient::addButtons( QBoxLayout *layout, const QString &s )
{
	for ( int i=0; i < s.length(); i++ )
	{
		switch ( s[i].latin1() )
		{
			// Menu button
			case 'M' :
				if ( !button[MenuButton] ) {
					button[MenuButton] = new KeramikButton( this, "menu", MenuButton, i18n("Menu"), Qt::LeftButton|Qt::RightButton );
					connect( button[MenuButton], SIGNAL( pressed() ), SLOT( menuButtonPressed() ) );
					layout->addWidget( button[MenuButton] );
				}
				break;

			// OnAllDesktops button
			case 'S' :
				if ( !button[OnAllDesktopsButton] ) {
					button[OnAllDesktopsButton] = new KeramikButton( this, "on_all_desktops",
							OnAllDesktopsButton, isOnAllDesktops()?i18n("Not on all desktops"):i18n("On all desktops") );
					if(isOnAllDesktops())
						button[OnAllDesktopsButton]->toggle();
					connect( button[OnAllDesktopsButton], SIGNAL( clicked() ), SLOT( toggleOnAllDesktops() ) );
					layout->addWidget( button[OnAllDesktopsButton] );
				}
				break;

			// Help button
			case 'H' :
				if ( !button[HelpButton] && providesContextHelp() ) {
					button[HelpButton] = new KeramikButton( this, "help", HelpButton, i18n("Help") );
					connect( button[HelpButton], SIGNAL( clicked() ), SLOT( showContextHelp() ) );
					layout->addWidget( button[HelpButton] );
				}
				break;

			// Minimize button
			case 'I' :
				if ( !button[MinButton] && isMinimizable() ) {
					button[MinButton] = new KeramikButton( this, "minimize", MinButton, i18n("Minimize") );
					connect( button[MinButton], SIGNAL( clicked() ), SLOT( minimize() ) );
					layout->addWidget( button[MinButton] );
				}
				break;

			// Maximize button
			case 'A' :
				if ( !button[MaxButton] && isMaximizable() ) {
					button[MaxButton] = new KeramikButton( this, "maximize", MaxButton, i18n("Maximize"), Qt::LeftButton|Qt::MidButton|Qt::RightButton );
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

			// Above button
			case 'F' :
				if ( !button[AboveButton]) {
					button[AboveButton] = new KeramikButton( this, "above", AboveButton, i18n("Keep Above Others") );
					connect( button[AboveButton], SIGNAL( clicked() ), SLOT( slotAbove() ) );
					layout->addWidget( button[AboveButton] );
				}
				break;

			// Below button
			case 'B' :
				if ( !button[BelowButton]) {
					button[BelowButton] = new KeramikButton( this, "below", BelowButton, i18n("Keep Below Others") );
					connect( button[BelowButton], SIGNAL( clicked() ), SLOT( slotBelow() ) );
					layout->addWidget( button[BelowButton] );
				}
				break;

			// Shade button
			case 'L' :
				if ( !button[ShadeButton] && isShadeable() ) {
					button[ShadeButton] = new KeramikButton( this, "shade", ShadeButton,
                                            isSetShade() ? i18n("Unshade") : i18n( "Shade" ));
					connect( button[ShadeButton], SIGNAL( clicked() ), SLOT( slotShade() ) );
					layout->addWidget( button[ShadeButton] );
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

	QRegion r;
	register int w, y = 0;
	int nrects;

	if ( QApplication::isRightToLeft() ) {

		// If the caption bubble is visible and extends above the titlebar
		if ( largeCaption && captionRect.width() >= 25 ) {
			register int x = captionRect.left();
			w = captionRect.width();
			r += QRegion( x + 11, y++, w - 19, 1 );
			r += QRegion( x + 9,  y++, w - 15, 1 );
			r += QRegion( x + 7,  y++, w - 12, 1 );
		} else {
			nrects = 8;

			// Do we have a large titlebar with a retracted caption bubble?
			// (i.e. the style is set to use large caption bubbles, we're
			//       not maximized and not active)
			if ( largeTitlebar )
				y = 3;
		}

		w = width(); // FRAME

		// The rounded titlebar corners
		r += QRegion( 9, y++, w - 17, 1 );
		r += QRegion( 7, y++, w - 13, 1 );
		r += QRegion( 5, y++, w - 9,  1 );
		r += QRegion( 4, y++, w - 7,  1 );
		r += QRegion( 3, y++, w - 5,  1 );
		r += QRegion( 2, y++, w - 4,  1 );
		r += QRegion( 1, y++, w - 2,  2 );
	} else {

		// If the caption bubble is visible and extends above the titlebar
		if ( largeCaption && captionRect.width() >= 25 ) {
			nrects = 11;
			register int x = captionRect.left();
			w = captionRect.width();
			r += QRegion( x + 8, y++, w - 19, 1 );
			r += QRegion( x + 6, y++, w - 15, 1 );
			r += QRegion( x + 5, y++, w - 12, 1 );
		} else {
			nrects = 8;

			// Do we have a large titlebar with a retracted caption bubble?
			// (i.e. the style is set to use large caption bubbles, we're
			//       not maximized and not active)
			if ( largeTitlebar )
				y = 3;
		}

		w = width(); // FRAME

		// The rounded titlebar corners
		r += QRegion( 8, y++, w - 17, 1 );
		r += QRegion( 6, y++, w - 13, 1 );
		r += QRegion( 4, y++, w - 9,  1 );
		r += QRegion( 3, y++, w - 7,  1 );
		r += QRegion( 2, y++, w - 5,  1 );
		r += QRegion( 2, y++, w - 4,  1 );
		r += QRegion( 1, y++, w - 2,  2 );
	}

	y++;

	// The part of the window below the titlebar
	r += QRegion( 0, y, w, height() - y );

        setMask( r, YXBanded );

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

	if ( captionBuffer.isNull() )
		return;

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
				activeIcon = new QPixmap( this->icon().pixmap( QIcon::Small, QIcon::Normal )); // FRAME
			icon = activeIcon;
		} else {
			if ( ! inactiveIcon ) {
				QImage img = this->icon().pixmap( QIcon::Small, QIcon::Normal ).convertToImage();
				KIconEffect::semiTransparent( img );
				inactiveIcon = new QPixmap( img );
			}
			icon = inactiveIcon;
		}
	}

	p.setFont( options()->font( active ) );
	int tw = p.fontMetrics().width( caption() ) +
		( clientHandler->showAppIcons() ? 16 + iconSpacing : 0 );

	int xpos = QMAX( (captionRect.width() - tw) / 3, 8 );
	QRect tr = QStyle::visualRect( QApplication::isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, QRect(xpos, 1, captionRect.width() - xpos - 10,
				captionRect.height() - 4), captionBuffer.rect() );

	//p.setPen( Qt::red ); // debug
	//p.drawRect( tr );    // debug

	// Application icon
	if ( clientHandler->showAppIcons() )
	{
		QRect iconRect = QStyle::visualRect( QApplication::isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, QRect(tr.x(),
					1 + (captionRect.height() - 4 - 16) / 2, 16, 16), tr );
		QRect r( icon->rect() );
		r.moveCenter( iconRect.center() );

		if ( tr.width() > 16 ) {
			p.drawPixmap( r, *icon );
		} else {
			QRect sr( 0, 0, icon->width(), icon->height() );

			if ( QApplication::isRightToLeft() )
				sr.addCoords( icon->width() - tr.width(), 0, 0, 0 );
			else
				sr.addCoords( 0, 0, -( icon->width() - tr.width() ), 0 );

			p.drawPixmap( r.x() + sr.x(), r.y() + sr.y(), *icon,
					sr.x(), sr.y(), sr.width(), sr.height() );
		}

		//p.drawRect( r ); // debug

		if ( QApplication::isRightToLeft() )
			tr.addCoords( 0, 0, -(16 + iconSpacing), 0 );
		else
			tr.addCoords( (16 + iconSpacing), 0, 0, 0 );
	}

	// Draw the titlebar text
	int flags = Qt::AlignVCenter | Qt::TextSingleLine;
	flags |= ( QApplication::isRightToLeft() ? Qt::AlignRight : Qt::AlignLeft );

	if ( clientHandler->useShadowedText() )
	{
		p.translate( QApplication::isRightToLeft() ? -1 : 1, 1 );
		//p.setPen( options()->color(ColorTitleBar, active).dark() );
                if (qGray(options()->color(ColorFont, active).rgb()) < 100)
                    p.setPen( QColor(200,200,200) );
                else
                    p.setPen( Qt::black );
		p.drawText( tr, flags, caption() );
		p.translate( QApplication::isRightToLeft() ? 1 : -1, -1 );
	}

	p.setPen( options()->color( ColorFont, active ) );
	p.drawText( tr, flags, caption() );

	captionBufferDirty = false;
}


void KeramikClient::calculateCaptionRect()
{
	QFontMetrics fm( options()->font(isActive()) );
	int cw = fm.width( caption() ) + 95;
	int titleBaseY = ( largeTitlebar ? 3 : 0 );

	if ( clientHandler->showAppIcons() )
		cw += 16 + 4; // icon width + space

	cw = QMIN( cw, titlebar->geometry().width() );
	captionRect = QStyle::visualRect( QApplication::isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, QRect(titlebar->geometry().x(), (largeCaption ? 0 : titleBaseY),
				cw, clientHandler->titleBarHeight(largeCaption) ),
				titlebar->geometry() );
}


void KeramikClient::captionChange()
{
	QRect r( captionRect );
	calculateCaptionRect();

	if ( r.size() != captionRect.size() )
		maskDirty = true;

	captionBufferDirty = true;

	widget()->repaint( r | captionRect, false );
}


void KeramikClient::iconChange()
{
	if ( clientHandler->showAppIcons() ) {

		// Force updateCaptionBuffer() to recreate the cached icons
            delete activeIcon;

            delete inactiveIcon;

		activeIcon = inactiveIcon = NULL;

		captionBufferDirty = true;
		widget()->repaint( captionRect, false );
	}
}


void KeramikClient::activeChange()
{
        bool active = isActive();
	// Note: It's assumed that the same font will always be used for both active
	//       and inactive windows, since the fonts kcm hasn't supported setting
	//       different fonts for different window states for some time.
	if ( largeTitlebar ) {
		largeCaption = ( active && !maximizedVertical() );
		calculateCaptionRect();
		maskDirty = true;
	}

	captionBufferDirty = true;

	widget()->repaint( false );

	for ( int i=0; i < NumButtons; i++ )
		if ( button[i] ) button[i]->repaint( false );
}


void KeramikClient::maximizeChange()
{
	if ( clientHandler->largeCaptionBubbles() )
	{
		if ( maximizeMode() & MaximizeVertical ) {
			// We've been maximized - shrink the titlebar by 3 pixels
			topSpacer->changeSize( 10, 1, QSizePolicy::Expanding, QSizePolicy::Minimum );
			largeCaption = largeTitlebar = false;

			calculateCaptionRect();
			captionBufferDirty = maskDirty = true;

			widget()->layout()->activate();
			widget()->repaint( false );
		} else if (( maximizeMode() & MaximizeVertical ) == 0 && !largeTitlebar ) {
			// We've been restored - enlarge the titlebar by 3 pixels
			topSpacer->changeSize( 10, 4, QSizePolicy::Expanding, QSizePolicy::Minimum );
			largeCaption = largeTitlebar = true;

			calculateCaptionRect();
			captionBufferDirty = maskDirty = true;

			widget()->layout()->activate();
			widget()->repaint( false );
		}
	}

	if ( button[ MaxButton ] ) {
		QToolTip::remove( button[ MaxButton ] );
		button[ MaxButton ]->setToolTip( maximizeMode() == MaximizeFull ? i18n("Restore") : i18n("Maximize") );
		button[ MaxButton ]->repaint();
	}
}


void KeramikClient::desktopChange()
{
	if ( button[ OnAllDesktopsButton ] )
		{
                button[ OnAllDesktopsButton ]->repaint( true );
		QToolTip::remove( button[ OnAllDesktopsButton ] );
		button[ OnAllDesktopsButton ]->setToolTip( isOnAllDesktops() ? i18n("Not on all desktops") : i18n("On all desktops") );
		}
}


void KeramikClient::shadeChange()
{
	if ( button[ ShadeButton ] )
		{
                button[ ShadeButton ]->repaint( true );
		QToolTip::remove( button[ ShadeButton ] );
		button[ ShadeButton ]->setToolTip( isSetShade() ? i18n("Unshade") : i18n("Shade") );
		}
}


void KeramikClient::keepAboveChange( bool )
{
	if ( button[ AboveButton ] )
                button[ AboveButton ]->repaint( true );
}


void KeramikClient::keepBelowChange( bool )
{
	if ( button[ BelowButton ] )
                button[ BelowButton ]->repaint( true );
}


void KeramikClient::menuButtonPressed()
{
	QPoint menuTop ( button[MenuButton]->rect().topLeft() );
	QPoint menuBottom ( button[MenuButton]->rect().bottomRight() );
	menuTop += QPoint(-6, -3);
	menuBottom += QPoint(6, 3);
	KDecorationFactory* f = factory();
	showWindowMenu( QRect( button[MenuButton]->mapToGlobal( menuTop ), 
				button[MenuButton]->mapToGlobal( menuBottom )) );
	if( !f->exists( this )) // 'this' was destroyed
		return;
	button[MenuButton]->setDown(false);
}


void KeramikClient::slotMaximize()
{
	maximize( button[ MaxButton ]->lastButton() );
}


void KeramikClient::slotAbove()
{
    setKeepAbove( !keepAbove());
    button[ AboveButton ]->repaint( true );
}


void KeramikClient::slotBelow()
{
    setKeepBelow( !keepBelow());
    button[ BelowButton ]->repaint( true );
}


void KeramikClient::slotShade()
{
    setShade( !isSetShade());
    button[ ShadeButton ]->repaint( true );
}


void KeramikClient::paintEvent( QPaintEvent *e )
{
	if ( !keramik_initialized )
		return;

	QPainter p( widget());
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
		if ( updateRect.right() > captionRect.right() && updateRect.x() < width() - 15 ) { // FRAME
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
	p.setPen( options()->color( ColorTitleBlend, active ) );
	p.drawLine( leftBorderWidth, height() - grabBarHeight - 1,
				width() - rightBorderWidth - 1, height() - grabBarHeight - 1 );
}


void KeramikClient::resizeEvent( QResizeEvent *e )
{
// FRAME	Client::resizeEvent( e );

	QRect r( captionRect );
	calculateCaptionRect();

	if ( r.size() != captionRect.size() )
		captionBufferDirty = true;

	maskDirty = true;

	if ( widget()->isVisible() )
	{
		widget()->update( widget()->rect() );
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
			widget()->update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4,
							titlebar->geometry().bottom() ) ) );
			// Titlebar needs no paint event
			QApplication::postEvent( this, new QPaintEvent( titlebar->geometry() ) );
		}
	}
}


void KeramikClient::mouseDoubleClickEvent( QMouseEvent *e )
{
	if ( e->button() == Qt::LeftButton
            && QRect( 0, 0, width(), clientHandler->titleBarHeight( largeTitlebar ) ).contains( e->pos() ) )
		titlebarDblClickOperation();
}


KeramikClient::Position KeramikClient::mousePosition( const QPoint &p ) const
{
	int titleBaseY = (largeTitlebar ? 3 : 0);

	int leftBorder   = clientHandler->tile( BorderLeft, true )->width();
	int rightBorder  = width() - clientHandler->tile( BorderRight, true )->width() - 1;
	int bottomBorder = height() - clientHandler->grabBarHeight() - 1;
	int bottomCornerSize = 3*clientHandler->tile( BorderRight, true )->width()/2 + 24;

	// Test if the mouse is over the titlebar area
	if ( p.y() < titleBaseY + 11 ) {
		// Test for the top left corner
		if ( p.x() < leftBorder + 11 ) {
			if ( (p.y() < titleBaseY + 3 && p.x() < leftBorder + 11) ||
					(p.y() < titleBaseY + 6 && p.x() < leftBorder + 6) ||
					(p.y() < titleBaseY + 11 && p.x() < leftBorder + 3) )
				return PositionTopLeft;
		}

		// Test for the top right corner
		if ( p.x() > rightBorder - 11 ) {
			if ( (p.y() < titleBaseY + 3 && p.x() > rightBorder - 11) ||
					(p.y() < titleBaseY + 6 && p.x() > rightBorder - 6) ||
					(p.y() < titleBaseY + 11 && p.x() > rightBorder - 3) )
				return PositionTopRight;
		}

		// Test for the top border
		if ( p.y() <= 3 || (p.y() <= titleBaseY+3 &&
					(p.x() < captionRect.left() || p.x() > captionRect.right()) ) )
			return PositionTop;

		// The cursor must be over the center of the titlebar.
		return PositionCenter;
	}

	// Test the sides
	else if ( p.y() < bottomBorder ) {
		// Test for the left side
		if ( p.x() < leftBorder ) {
			if ( p.y() < height() - bottomCornerSize )
				return PositionLeft;
			else
				return PositionBottomLeft;
		}

		// Test for the right side
		else if ( p.x() > rightBorder ) {
			if ( p.y() < height() - bottomCornerSize )
				return PositionRight;
			else
				return PositionBottomRight;
		}

		// The cursor must be over the center of the window
		return PositionCenter;
	}

	// Test the grab bar / bottom border
	else {
		// Test for the bottom left corner
		if ( p.x() < bottomCornerSize )
			return PositionBottomLeft;

		// Test for the bottom right corner
		else if ( p.x() > width() - bottomCornerSize - 1 )
			return PositionBottomRight;

		// The cursor must be over the bottom border
		return PositionBottom;
	}

	// We should never get here
	return PositionCenter;
}


void KeramikClient::resize( const QSize& s )
{
	widget()->resize( s );
}


void KeramikClient::borders( int& left, int& right, int& top, int& bottom ) const
{
	int titleBarHeight     = clientHandler->titleBarHeight( clientHandler->largeCaptionBubbles() );
	int grabBarHeight      = clientHandler->grabBarHeight();
	int leftBorderWidth    = clientHandler->tile( BorderLeft, isActive() )->width();
	int rightBorderWidth   = clientHandler->tile( BorderRight, isActive() )->width();

	left   = leftBorderWidth;
	right  = rightBorderWidth;
	top    = titleBarHeight;
	bottom = grabBarHeight;

	if ( ( maximizeMode() & MaximizeHorizontal ) && !options()->moveResizeMaximizedWindows())
		left = right = 0;
	if( maximizeMode() & MaximizeVertical)
	{
		top = clientHandler->titleBarHeight( false );
		if( !options()->moveResizeMaximizedWindows())
			bottom = 0;
	}
}


QSize KeramikClient::minimumSize() const
{
	return widget()->minimumSize();
}


bool KeramikClient::eventFilter( QObject* o, QEvent* e )
{
	if ( o != widget() )
		return false;

	switch ( e->type() )
	{
		case QEvent::Resize:
			resizeEvent( static_cast< QResizeEvent* >( e ) );
			return true;

		case QEvent::Paint:
			paintEvent( static_cast< QPaintEvent* >( e ) );
			return true;

		case QEvent::MouseButtonDblClick:
			mouseDoubleClickEvent( static_cast< QMouseEvent* >( e ) );
			return true;

		case QEvent::MouseButtonPress:
			processMousePressEvent( static_cast< QMouseEvent* >( e ) );
			return true;

		default:
	    		return false;
	}
}

} // namespace Keramik



// -------------------------------------------------------------------------------------------



extern "C"
{
	KDE_EXPORT KDecorationFactory *create_factory()
	{
		Keramik::clientHandler = new Keramik::KeramikHandler();
                return Keramik::clientHandler;
	}
}



// vim: set noet ts=4 sw=4:
