/*
 * $Id$
 *
 * Gallium-Quartz KWin client
 *
 * Copyright 2001
 *   Karol Szwed <karlmail@usa.net>
 *   http://gallium.n3.net/
 *
 * Based upon the Win2K kwin client, which is based on the
 * KDE default client.
 *
 * Includes mini titlebars for ToolWindow Support.
 * Button positions are now customizable.
 *
 */

#ifndef __KDEGALLIUM_QUARTZ_H
#define __KDEGALLIUM_QUARTZ_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"

class QSpacerItem;
class QHBoxLayout;

namespace KWinInternal {


class QuartzHandler: public QObject
{
	Q_OBJECT
	public:
		QuartzHandler();
		~QuartzHandler();

	public slots:
		void slotReset();

	private:
		void readConfig();
		void createPixmaps();
		void freePixmaps();
		void drawBlocks(KPixmap* pi, KPixmap &p, const QColor &c1, const QColor &c2);
};


class QuartzButton : public KWinInternal::KWinButton
{
	public:
		QuartzButton(Client *parent=0, const char *name=0, bool largeButton=true,
					 bool isLeftButton=true, bool isStickyButton=false,
					 const unsigned char *bitmap=NULL, const QString& tip=NULL);
		~QuartzButton();
		void setBitmap(const unsigned char *bitmap);
		QSize sizeHint() const;
		int   last_button;
		void turnOn( bool isOn );

	protected:
		void mousePressEvent( QMouseEvent* e );
		void mouseReleaseEvent( QMouseEvent* e );
		void drawButton(QPainter *p);
		void drawButtonLabel(QPainter*) {;}

		QBitmap* deco;
		bool     large;
		bool	 isLeft;
		bool 	 isSticky;
		Client*  client;
};


class QuartzClient : public KWinInternal::Client
{
	Q_OBJECT

	public:
		QuartzClient( Workspace *ws, WId w, QWidget *parent=0, 
					  const char *name=0 );
		~QuartzClient() {;}

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

	protected slots:
		void slotMaximize();
		void menuButtonPressed();

	private:
		void calcHiddenButtons();
		void addClientButtons( const QString& s, bool isLeft=true );

		enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose, 
					  BtnMenu, BtnSticky, BtnCount };
		QuartzButton* button[ QuartzClient::BtnCount ];
		int           lastButtonWidth;
		int 		  titleHeight;
		bool          largeButtons;
		QHBoxLayout*  hb;
		QSpacerItem*  titlebar;
};

};

#endif
// vim: ts=4
