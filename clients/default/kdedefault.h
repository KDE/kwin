/*
 *	$Id$
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

#ifndef _KDE_DEFAULT_H
#define _KDE_DEFAULT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"

class QSpacerItem;
class QHBoxLayout;

namespace KWinInternal {


class KDEDefaultHandler: public QObject
{
	public:
		KDEDefaultHandler();
		~KDEDefaultHandler();
		void reset();

	private:
		void readConfig();
		void createPixmaps();
		void freePixmaps();
		void drawButtonBackground(KPixmap *pix, const QColorGroup &g, bool sunken);
};


class KDEDefaultButton : public QButton
{
	public:
		KDEDefaultButton(Client *parent=0, const char *name=0, bool largeButton=true,
					 bool isLeftButton=true, bool isStickyButton=false,
					 const unsigned char *bitmap=NULL);
		~KDEDefaultButton();
		void setBitmap(const unsigned char *bitmap);
		QSize sizeHint() const;
		int   last_button;
		void turnOn( bool isOn );

	protected:
		void enterEvent(QEvent *){ isMouseOver=true; repaint(false); }
		void leaveEvent(QEvent *){ isMouseOver=false; repaint(false); }
		void mousePressEvent( QMouseEvent* e );
		void mouseReleaseEvent( QMouseEvent* e );
		void drawButton(QPainter *p);
		void drawButtonLabel(QPainter*) {;}

        Client* client;
		QBitmap* deco;
		bool    large;
		bool    isLeft;
		bool    isSticky;
		bool	isMouseOver;
};


class KDEDefaultClient : public KWinInternal::Client
{
	Q_OBJECT

	public:
		KDEDefaultClient( Workspace *ws, WId w, QWidget *parent=0, 
						  const char *name=0 );
		~KDEDefaultClient() {;}

	protected:
		void resizeEvent( QResizeEvent* );
		void paintEvent( QPaintEvent* );
		void showEvent( QShowEvent* );
		void mouseDoubleClickEvent( QMouseEvent * );
		void captionChange( const QString& name );
		void maximizeChange(bool m);
		void activeChange(bool);
		void iconChange();
		void stickyChange(bool on);
		MousePosition mousePosition(const QPoint &) const;

	protected slots:
		void slotMaximize();
		void menuButtonPressed();

	private:
		void doShape();
		void calcHiddenButtons();
		void addClientButtons( const QString& s, bool isLeft=true );

		enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose, 
					  BtnMenu, BtnSticky, BtnCount };
		KDEDefaultButton* button[ KDEDefaultClient::BtnCount ];
		int           lastButtonWidth;
		int           titleHeight;
		bool          largeButtons;
		QHBoxLayout*  hb;
		QSpacerItem*  titlebar;
};

};

#endif
// vim: ts=4
