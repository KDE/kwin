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

#ifndef __BUTTONS_H_
#define __BUTTONS_H_

#include <qevent.h>
#include <qdragobject.h>
#include <qlistbox.h>


/////////////////////////////////////////////////////////////////////////

class ButtonDrag: public QStoredDrag
{
	public:
		ButtonDrag( char btn, QWidget* parent, const char* name=0 );
		~ButtonDrag() {};

		static bool canDecode( QDragMoveEvent* e );
		static bool decode( QDropEvent* e, char& btn );
};


/////////////////////////////////////////////////////////////////////////

class ButtonSource: public QListBox
{
	Q_OBJECT

	public:
		ButtonSource( QWidget* parent=0, const char* name=0 );
		~ButtonSource();

		void hideAllButtons();
		void showAllButtons();

	signals:
		void buttonDropped();

	public slots:
		void hideButton( char btn );
		void showButton( char btn );

	protected:
		void dragEnterEvent( QDragEnterEvent* e );
		void dragMoveEvent( QDragMoveEvent* e );
		void dragLeaveEvent( QDragLeaveEvent* e );
		void dropEvent( QDropEvent* e );
		void mousePressEvent( QMouseEvent* e );

	private:
		char convertToChar( QString s );
		QString convertToString( char btn );

		int spacerCount;
};


/////////////////////////////////////////////////////////////////////////

class ButtonDropSite: public QFrame
{
	Q_OBJECT

	public:
		ButtonDropSite( QWidget* parent=0, const char* name=0 );
		~ButtonDropSite();

		// Allow external classes access our buttons - ensure buttons are
		// not duplicated however.
		QString buttonsLeft;
		QString buttonsRight;

	signals:
		void buttonAdded( char c );
		void buttonRemoved( char c );
		void changed();

	public slots:
		void removeClickedButton();

	protected:
		void dragEnterEvent( QDragEnterEvent* e );
		void dragMoveEvent( QDragMoveEvent* e );
		void dragLeaveEvent( QDragLeaveEvent* e );
		void dropEvent( QDropEvent* e );
		void mousePressEvent( QMouseEvent* e );

		void drawContents( QPainter* p );
		int  buttonWidth( char btn );
		int  calcButtonStringWidth( const QString& s );
		char removeButtonAtPoint( QPoint p );
		void buttonInsertedAtPoint( QPoint p, bool& isleft, int& strPos );
		void drawButtonString( QPainter* p, QString& s, int offset );

		QPoint mouseClickPoint;
};

class ButtonPositionWidget : public QWidget
{
	Q_OBJECT

	public:
		ButtonPositionWidget(QWidget *parent = 0, const char* name = 0);
		~ButtonPositionWidget();

		QString buttonsLeft() const;
		QString buttonsRight() const;
		void setButtonsLeft(const QString &buttons);
		void setButtonsRight(const QString &buttons);

	signals:
		void changed();

	public: // TODO: make private
		ButtonDropSite* dropSite;
		ButtonSource* buttonSource;
};


#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
