/*
	$Id$

	This is the new kwindecoration kcontrol module

	Copyright (c) 2001
		Karol Szwed <gallium@kde.org>
		http://gallium.n3.net/

	Supports new kwin configuration plugins, and titlebar button position
	modification via dnd interface.

	Based on original "kwintheme" (Window Borders) 
	Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>
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


#endif
// vim: ts=4
