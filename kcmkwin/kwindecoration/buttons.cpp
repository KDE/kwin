/*
	This is the new kwindecoration kcontrol module

	Copyright (c) 2001
		Karol Szwed <gallium@kde.org>
		http://gallium.n3.net/

	Supports new kwin configuration plugins, and titlebar button position
	modification via dnd interface.

	Based on original "kwintheme" (Window Borders)
	Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <qpainter.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include "buttons.h"
#include "pixmaps.h"


// General purpose button globals (I know I shouldn't use them :)
//===============================================================

enum Buttons{ BtnMenu=0, BtnOnAllDesktops, BtnSpacer, BtnHelp,
			  BtnMinimize, BtnMaximize, BtnClose, 
			  BtnAboveOthers, BtnBelowOthers, 
			  BtnShade, BtnResize, BtnCount };
QListBoxPixmap* buttons[ BtnCount ];
QPixmap*	 	pixmaps[ BtnCount ];
QPixmap* 		miniSpacer;


//==============================================================

ButtonDrag::ButtonDrag( char btn, QWidget* parent, const char* name)
	: QStoredDrag( "kcontrol/kwindecoration_buttons", parent, name)
{
	QByteArray payload(1);
	payload[0] = btn;
	setEncodedData( payload );
}


bool ButtonDrag::canDecode( QDragMoveEvent* e )
{
	return e->provides( "kcontrol/kwindecoration_buttons" );
}


bool ButtonDrag::decode( QDropEvent* e, char& btn )
{
	QByteArray payload = e->data( "kcontrol/kwindecoration_buttons" );
	if ( payload.size() )
	{
		e->accept();
		btn = payload[0];
		return TRUE;
	}
	return FALSE;
}



/////////////////////////////////////////////////////////////////////////
// Implements the button drag source list box
/////////////////////////////////////////////////////////////////////////

// Converts the button character value to its index
static int btnIndex( char btn )
{
	switch (btn)
	{
		case 'M':
			return BtnMenu;
			break;
		case 'S':
			return BtnOnAllDesktops;
			break;
		case '_':
			return BtnSpacer;
			break;
		case 'H':
			return BtnHelp;
			break;
		case 'I':
			return BtnMinimize;
			break;
		case 'A':
			return BtnMaximize;
			break;
		case 'X':
			return BtnClose;
			break;
		case 'F':
			return BtnAboveOthers;
			break;
		case 'B':
			return BtnBelowOthers;
			break;
		case 'L':
			return BtnShade;
			break;
		case 'R':
			return BtnResize;
			break;
		default:
			return -1;	// Not found...
	}
}


// Returns the pixmap of a button item
const QPixmap* btnPixmap( char btn )
{
	if (btn == '_')
		return miniSpacer;

	int btnindex = btnIndex( btn );
	if (btnindex == -1)
		return NULL;

	return buttons[btnindex]->pixmap();
}



ButtonSource::ButtonSource( QWidget* parent, const char* name )
  : QListBox( parent, name)
{
	// Create the listbox pixmaps
	pixmaps[ BtnMenu ]		= new QPixmap( button_menu_xpm );
	pixmaps[ BtnOnAllDesktops ]	= new QPixmap( button_on_all_desktops_xpm );
	pixmaps[ BtnSpacer ] 	= new QPixmap( button_spacer_xpm );
	pixmaps[ BtnHelp ] 		= new QPixmap( button_help_xpm );
	pixmaps[ BtnMinimize ] 	= new QPixmap( button_minimize_xpm );
	pixmaps[ BtnMaximize ] 	= new QPixmap( button_maximize_xpm );
	pixmaps[ BtnClose ] 	= new QPixmap( button_close_xpm );
	pixmaps[ BtnAboveOthers ]	= new QPixmap( button_above_others_xpm );
	pixmaps[ BtnBelowOthers ]	= new QPixmap( button_below_others_xpm );
	pixmaps[ BtnShade ]	= new QPixmap( button_shade_xpm );
	pixmaps[ BtnResize ]	= new QPixmap( button_resize_xpm );
	miniSpacer 				= new QPixmap( titlebarspacer_xpm );

	// Add all possible button/spacer types to the list box.
	buttons[ BtnMenu ]	 	= new QListBoxPixmap( this, *pixmaps[BtnMenu], i18n("Menu") );
	buttons[ BtnOnAllDesktops] 	= new QListBoxPixmap( this, *pixmaps[BtnOnAllDesktops], i18n("On All Desktops") );
	buttons[ BtnAboveOthers ]	= new QListBoxPixmap( this, *pixmaps[BtnAboveOthers], i18n("Keep Above Others") );
	buttons[ BtnBelowOthers ]	= new QListBoxPixmap( this, *pixmaps[BtnBelowOthers], i18n("Keep Below Others") );
	buttons[ BtnShade ]		= new QListBoxPixmap( this, *pixmaps[BtnShade], i18n("Shade") );
	buttons[ BtnResize ]	= new QListBoxPixmap( this, *pixmaps[BtnResize], i18n("Resize") );
	buttons[ BtnSpacer ] 	= new QListBoxPixmap( this, *pixmaps[BtnSpacer], i18n("Spacer") );
	buttons[ BtnHelp ]		= new QListBoxPixmap( this, *pixmaps[BtnHelp], i18n("Help") );
	buttons[ BtnMinimize ]	= new QListBoxPixmap( this, *pixmaps[BtnMinimize], i18n("Minimize") );
	buttons[ BtnMaximize ]	= new QListBoxPixmap( this, *pixmaps[BtnMaximize], i18n("Maximize") );
	buttons[ BtnClose ]		= new QListBoxPixmap( this, *pixmaps[BtnClose], i18n("Close") );

	spacerCount = 0;	// No spacers inserted yet
	setAcceptDrops( TRUE );
}


ButtonSource::~ButtonSource()
{
	for( int i = 0; i < BtnCount; i++)
		if (pixmaps[i])
			delete pixmaps[i];

	if (miniSpacer)
		delete miniSpacer;
}


void ButtonSource::hideAllButtons()
{
	// Hide all listbox items which are visible
	if (index( buttons[BtnMenu] ) != -1)
		takeItem( buttons[BtnMenu] );
	if (index( buttons[BtnOnAllDesktops] )!= -1)
		takeItem( buttons[BtnOnAllDesktops] );
	if (index( buttons[BtnAboveOthers] )!= -1)
		takeItem( buttons[BtnAboveOthers] );
	if (index( buttons[BtnBelowOthers] )!= -1)
		takeItem( buttons[BtnBelowOthers] );
	if (index( buttons[BtnResize] )!= -1)
		takeItem( buttons[BtnResize] );
	if (index( buttons[BtnShade] )!= -1)
		takeItem( buttons[BtnShade] );
	if (index( buttons[BtnHelp] ) != -1)
		takeItem( buttons[BtnHelp] );
	if (index( buttons[BtnMinimize] ) != -1)
		takeItem( buttons[BtnMinimize] );
	if (index( buttons[BtnMaximize] ) != -1)
		takeItem( buttons[BtnMaximize] );
	if (index( buttons[BtnClose] ) != -1)
		takeItem( buttons[BtnClose] );
	if (index( buttons[BtnSpacer] ) != -1)
		takeItem( buttons[BtnSpacer] );

	spacerCount = 10;	// 10 inserted spacers (max)
}

void ButtonSource::showAllButtons()
{
	// Hide all listbox items which are visible
	if (index( buttons[BtnMenu] ) == -1)
		insertItem( buttons[BtnMenu] );
	if (index( buttons[BtnOnAllDesktops] )== -1)
		insertItem( buttons[BtnOnAllDesktops] );
	if (index( buttons[BtnAboveOthers] )== -1)
		insertItem( buttons[BtnAboveOthers] );
	if (index( buttons[BtnBelowOthers] )== -1)
		insertItem( buttons[BtnBelowOthers] );
	if (index( buttons[BtnResize] )== -1)
		insertItem( buttons[BtnResize] );
	if (index( buttons[BtnShade] )== -1)
		insertItem( buttons[BtnShade] );
	if (index( buttons[BtnHelp] ) == -1)
		insertItem( buttons[BtnHelp] );
	if (index( buttons[BtnMinimize] ) == -1)
		insertItem( buttons[BtnMinimize] );
	if (index( buttons[BtnMaximize] ) == -1)
		insertItem( buttons[BtnMaximize] );
	if (index( buttons[BtnClose] ) == -1)
		insertItem( buttons[BtnClose] );
	if (index( buttons[BtnSpacer] ) == -1)
		insertItem( buttons[BtnSpacer] );

	spacerCount = 0;	// No inserted spacers
}


void ButtonSource::showButton( char btn )
{
	// Ignore spacers (max 10)
	if (btn == '_')
		spacerCount--;

	int btnindex = btnIndex(btn);

	// Check if the item is already inserted...
	if ( (btnindex != -1) && (index( buttons[btnindex] ) == -1) )
	{
		setUpdatesEnabled( FALSE );
		insertItem( buttons[ btnindex ] );
		setUpdatesEnabled( TRUE );
		sort();
	}
}


void ButtonSource::hideButton( char btn )
{
	// Ignore spacers (max 10)
	if (btn == '_')
	{
		spacerCount++;
		if (spacerCount != 10)
		return;
	}

	int btnindex = btnIndex(btn);

	// Check if the item is already removed...
	if ( (btnindex != -1) && (index( buttons[btnindex] ) != -1) )
	{
		setUpdatesEnabled( FALSE );
		// De-select before removal
		setSelected( buttons[ btnindex ], false );
		takeItem( buttons[ btnindex ] );
		setUpdatesEnabled( TRUE );
		sort();
	}
}


char ButtonSource::convertToChar( QString s )
{
	// Convert the item to its character representation
	if (s == i18n("Menu"))
		return 'M';
	else if (s == i18n("On All Desktops"))
		return 'S';
	else if (s == i18n("Spacer"))
		return '_';
	else if (s == i18n("Help"))
		return 'H';
	else if (s == i18n("Minimize"))
		return 'I';
	else if (s == i18n("Maximize"))
		return 'A';
	else if (s == i18n("Close"))
		return 'X';
	else if (s == i18n("Keep Above Others"))
		return 'F';
	else if (s == i18n("Keep Below Others"))
		return 'B';
	else if (s == i18n("Shade"))
		return 'L';
	else if (s == i18n("Resize"))
		return 'R';
	else
		return '?';
}


void ButtonSource::mousePressEvent( QMouseEvent* e )
{
	// Make a selection before moving the mouse
	QListBox::mousePressEvent( e );

	// Make sure we have at laest 1 item in the listbox
	if ( count() > 0 )
	{
		// Obtain currently selected item
		char btn = convertToChar( currentText() );
		ButtonDrag* bd = new ButtonDrag( btn, this );
		bd->dragCopy();
	}
}


void ButtonSource::dragMoveEvent( QDragMoveEvent* /* e */ )
{
	// Do nothing...
}


void ButtonSource::dragEnterEvent( QDragEnterEvent* e )
{
	if ( ButtonDrag::canDecode( e ) )
		e->accept();
}


void ButtonSource::dragLeaveEvent( QDragLeaveEvent* /* e */ )
{
	// Do nothing...
}


void ButtonSource::dropEvent( QDropEvent* /* e */ )
{
	// Allow the button to be removed from the ButtonDropSite
	emit buttonDropped();
}


/////////////////////////////////////////////////////////////////////////
// This class renders and handles the demo titlebar dropsite
/////////////////////////////////////////////////////////////////////////

ButtonDropSite::ButtonDropSite( QWidget* parent, const char* name )
	: QFrame( parent, name )
{
	setAcceptDrops( TRUE );
	setFrameShape( WinPanel );
	setFrameShadow( Raised );
	setMinimumHeight( 26 );
	setMaximumHeight( 26 );
	setMinimumWidth( 250 );		// Ensure buttons will fit

	mouseClickPoint.setX(0);
	mouseClickPoint.setY(0);
}


ButtonDropSite::~ButtonDropSite()
{
	// Do nothing...
}


void ButtonDropSite::dragMoveEvent( QDragMoveEvent* /* e */ )
{
	// Do nothing...
}


void ButtonDropSite::dragEnterEvent( QDragEnterEvent* e )
{
	if ( ButtonDrag::canDecode( e ) )
		e->accept();
}


void ButtonDropSite::dragLeaveEvent( QDragLeaveEvent* /* e */ )
{
	// Do nothing...
}


void ButtonDropSite::dropEvent( QDropEvent* e )
{
	char btn;
	if ( ButtonDrag::decode(e, btn) )
	{
		bool isleft;
		int strPos;

		// If we are moving buttons around, remove the old item first.
		if (btn == '*')
		{
			btn = removeButtonAtPoint( mouseClickPoint );
			if (btn != '?')
				emit buttonRemoved( btn );
		}

		if (btn != '?')
		{
			// Add the button to our button strings
			buttonInsertedAtPoint( e->pos(), isleft, strPos );

			if (isleft)
				buttonsLeft.insert( strPos, btn );
			else
				buttonsRight.insert( strPos, btn );

			repaint(false);

			// Allow listbox to update itself
			emit buttonAdded( btn );
			emit changed();
		}
	}
}


// Starts dragging a button...
void ButtonDropSite::mousePressEvent( QMouseEvent* e )
{
	mouseClickPoint = e->pos();

	ButtonDrag* bd = new ButtonDrag( '*', this );
	bd->dragCopy();
}


int ButtonDropSite::buttonWidth( char btn )
{
	if (btn == '_')
		return 6;		// ensure this matches with the pixmap widths
	else
		return 20;		// Assume characters given are all valid
}


// Computes the total space the buttons will take in the titlebar
int ButtonDropSite::calcButtonStringWidth( const QString& s )
{
	QChar ch;
	unsigned int offset = 0;

	for(unsigned int i = 0; i < s.length(); i++)
	{
		ch = s[i];
		offset += buttonWidth( ch.latin1() );
	}
	return (int)offset;
}


// This slot is called after we drop on the item listbox...
void ButtonDropSite::removeClickedButton()
{
	if ( !mouseClickPoint.isNull() )
	{
		char btn = removeButtonAtPoint( mouseClickPoint );
		mouseClickPoint.setX(0);
		mouseClickPoint.setY(0);
		repaint(false);

		emit buttonRemoved( btn );
		emit changed();
	}
}


// Find the string and position at which to insert the new button...
void ButtonDropSite::buttonInsertedAtPoint( QPoint p, bool& isleft, int& strPos )
{
	int leftoffset = calcButtonStringWidth( buttonsLeft );
	int rightoffset = calcButtonStringWidth( buttonsRight );
	int posx = p.x() - 3;

	// The centre of the titlebar text tells us whether to add to the left or right
	if ( posx < ( leftoffset - rightoffset + ((geometry().width() - 6) / 2)))
		isleft = true;
	else
		isleft = false;

	QString s = isleft ? buttonsLeft : buttonsRight;
	int offset = isleft ? 0 : geometry().width() - 6 - rightoffset;
	QChar ch;

	strPos = s.length();

	for (unsigned int i = 0; i < s.length(); i++)
	{
		if ( posx < (offset + 5 ))
		{
			strPos = i;
			break;
		}
		ch = s[i];
		offset += buttonWidth( ch.latin1() );
	}
}


char ButtonDropSite::removeButtonAtPoint( QPoint p )
{
	int offset = -1;
	bool isleft = false;

	// Shrink contents rect by 1 to fit in the titlebar border
	QRect r = contentsRect();
	r.moveBy(1 , 1);
	r.setWidth( r.width() - 2 );
	r.setHeight( r.height() - 2 );

	// Bail out if the borders were clicked
	if ( !r.contains(p) )
		return '?';

	int posx = p.x();

	// Is the point in the LHS/RHS button area?
	if ( (!buttonsLeft.isEmpty()) && (posx <= (calcButtonStringWidth( buttonsLeft )+3)) )
	{
		offset = 3;
		isleft = true;
	}
	else if ( (!buttonsRight.isEmpty()) && (posx >= geometry().width() - calcButtonStringWidth(buttonsRight) - 3))
		{
			offset = geometry().width() - calcButtonStringWidth(buttonsRight) - 3;
			isleft = false;
		}

	// Step through the button strings and remove the appropriate button
	if (offset != -1)
	{
		QChar ch;
		QString s = isleft ? buttonsLeft : buttonsRight;

		// Step through the items, to find the appropriate one to remove.
		for (unsigned int i = 0; i < s.length(); i++)
		{
			ch = s[i];
			offset += buttonWidth( ch.latin1() );
			if (posx <= offset)
			{
				s.remove( i, 1 );		// Remove the current button item
				if (isleft)
					buttonsLeft = s;
				else
					buttonsRight = s;
				return ch.latin1();
			}
		}
	}

	return '?';
}


void ButtonDropSite::drawButtonString( QPainter* p, QString& s, int offset )
{
	QChar ch;

	for(unsigned int i = 0; i < s.length(); i++)
	{
		ch = s[i];
		p->drawPixmap( offset, 3, *btnPixmap(ch.latin1()) );
		offset += buttonWidth(ch.latin1());
	}
}


void ButtonDropSite::drawContents( QPainter* p )
{
	int leftoffset = calcButtonStringWidth( buttonsLeft );
	int rightoffset = calcButtonStringWidth( buttonsRight );
	int offset = 3;

	QRect r = contentsRect();

	// Shrink by 1
	r.moveBy(1 + leftoffset, 1);
	r.setWidth( r.width() - 2 - leftoffset - rightoffset );
	r.setHeight( r.height() - 2 );

	drawButtonString( p, buttonsLeft, offset );

	QColor c1( 0x0A, 0x5F, 0x89 );		// KDE 2 titlebar default colour
	p->fillRect( r, c1 );
	p->setPen( Qt::white );
	p->setFont( QFont( KGlobalSettings::generalFont().family(), 12, QFont::Bold) );
	p->drawText( r, AlignLeft | AlignVCenter, i18n("KDE") );

	offset = geometry().width() - 3 - rightoffset;
	drawButtonString( p, buttonsRight, offset );
}

#include "buttons.moc"
// vim: ts=4
