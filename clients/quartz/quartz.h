/*
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

#ifndef __KDEGALLIUM_QUARTZ_H
#define __KDEGALLIUM_QUARTZ_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../lib/kdecoration.h"
#include "../../lib/kdecorationfactory.h"

class QSpacerItem;
class QBoxLayout;

namespace Quartz {

class QuartzClient;

class QuartzHandler: public QObject, public KDecorationFactory
{
	Q_OBJECT
	public:
		QuartzHandler();
		~QuartzHandler();

		virtual KDecoration* createDecoration( KDecorationBridge* );
		virtual bool reset(unsigned long changed);
		virtual QValueList< BorderSize > borderSizes() const;

	private:
		void readConfig();
		void createPixmaps();
		void freePixmaps();
		void drawBlocks(KPixmap* pi, KPixmap &p, const QColor &c1, const QColor &c2);
};


class QuartzButton : public QButton
{
	public:
		QuartzButton(QuartzClient *parent=0, const char *name=0, bool largeButton=true,
					 bool isLeftButton=true, bool isOnAllDesktopsButton=false,
					 const unsigned char *bitmap=NULL, const QString& tip=NULL, const int realizeBtns = LeftButton);
		~QuartzButton();
		void setBitmap(const unsigned char *bitmap);
		void setTipText(const QString &tip);
		QSize sizeHint() const;
		ButtonState last_button;
		void turnOn( bool isOn );

	protected:
		void mousePressEvent( QMouseEvent* e );
		void mouseReleaseEvent( QMouseEvent* e );
		void drawButton(QPainter *p);
		void drawButtonLabel(QPainter*) {;}

		QBitmap* deco;
		bool     large;
		bool	 isLeft;
		bool 	 isOnAllDesktops;
		QuartzClient*  client;

		int realizeButtons;
};


class QuartzClient : public KDecoration
{
	Q_OBJECT

	public:
		QuartzClient(KDecorationBridge* bridge, KDecorationFactory* factory);
		~QuartzClient() {;}

		virtual void init();
		virtual void resize(const QSize&);
		virtual bool eventFilter( QObject* o, QEvent* e );

	protected:
		virtual void reset( unsigned long changed );
		void resizeEvent( QResizeEvent* );
		void paintEvent( QPaintEvent* );
		void showEvent( QShowEvent* );
		void mouseDoubleClickEvent( QMouseEvent * );
		virtual void captionChange();
		void maximizeChange();
		virtual void shadeChange() {};
		virtual void activeChange();
		virtual void iconChange();
		virtual void desktopChange();
		virtual QuartzClient::Position mousePosition(const QPoint &point) const;
		virtual void borders(int&, int&, int&, int&) const;
		virtual QSize minimumSize() const;

	protected slots:
		void slotMaximize();
		void menuButtonPressed();

	private:
		bool isTool();
		void calcHiddenButtons();
		void addClientButtons( const QString& s, bool isLeft=true );

		enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose,
					  BtnMenu, BtnOnAllDesktops, BtnCount };
		QuartzButton* button[ QuartzClient::BtnCount ];
		int           lastButtonWidth;
		int 		  titleHeight, borderSize;
		bool          largeButtons;
		QBoxLayout*   hb;
		QSpacerItem*  titlebar;
};

}

#endif
// vim: ts=4
