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
#include <qdatetime.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"


class QSpacerItem;
class QBoxLayout;

namespace Default {

using namespace KWinInternal;

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
		void drawButtonBackground(KPixmap *pix,
				const QColorGroup &g, bool sunken);
};


class KDEDefaultButton : public KWinButton
{
	public:
		KDEDefaultButton(Client *parent=0, const char *name=0,
			 bool largeButton=true, bool isLeftButton=true,
			 bool isStickyButton=false, const unsigned char *bitmap=NULL,
			 const QString& tip=NULL);
		~KDEDefaultButton();

		int last_button;
		void turnOn( bool isOn );
		void setBitmap(const unsigned char *bitmap);
		QSize sizeHint() const;

	protected:
		void enterEvent(QEvent *);
		void leaveEvent(QEvent *);
		void mousePressEvent( QMouseEvent* e );
		void mouseReleaseEvent( QMouseEvent* e );
		void drawButton(QPainter *p);
		void drawButtonLabel(QPainter*) {;}

		QBitmap* deco;
		bool    large;
		bool    isLeft;
		bool    isSticky;
		bool	isMouseOver;
		Client* client;
};


class KDEDefaultClient : public Client
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
	    void menuButtonReleased();

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
		QGridLayout*  g;
		QBoxLayout*   hb;
		QSpacerItem*  titlebar;
		QSpacerItem*  spacer;
		bool m_closing;
};

}

#endif
// vim: ts=4
