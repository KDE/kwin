/*
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
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QSpacerItem;
class QBoxLayout;
class QGridLayout;

namespace Default {

class KDEDefaultClient;

class KDEDefaultHandler: public KDecorationFactory
{
	public:
		KDEDefaultHandler();
		~KDEDefaultHandler();
                KDecoration* createDecoration( KDecorationBridge* b );
		bool reset( unsigned long changed );
		virtual QValueList< BorderSize > borderSizes() const;

	private:
		unsigned long readConfig( bool update );
		void createPixmaps();
		void freePixmaps();
		void drawButtonBackground(KPixmap *pix,
				const QColorGroup &g, bool sunken);
};


class KDEDefaultButton : public QButton, public KDecorationDefines
{
	public:
		KDEDefaultButton(KDEDefaultClient *parent=0, const char *name=0,
			 bool largeButton=true, bool isLeftButton=true,
			 bool isStickyButton=false, const unsigned char *bitmap=NULL,
			 const QString& tip=NULL, const int realizeBtns = LeftButton);
		~KDEDefaultButton();

		ButtonState last_button;
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
		KDEDefaultClient* client;

		int realizeButtons;
};


class KDEDefaultClient : public KDecoration
{
	Q_OBJECT

	public:
		KDEDefaultClient( KDecorationBridge* b, KDecorationFactory* f );
		~KDEDefaultClient() {;}
                void init();
                void borders( int&, int&, int&, int& ) const;
                void resize( const QSize& );
                QSize minimumSize() const;
                void reset( unsigned long changed );

	protected:
                bool eventFilter( QObject*, QEvent* );
		void resizeEvent( QResizeEvent* );
		void paintEvent( QPaintEvent* );
		void showEvent( QShowEvent* );
		void mouseDoubleClickEvent( QMouseEvent * );
		void captionChange();
		void maximizeChange();
		void activeChange();
		void iconChange();
		void desktopChange();
                void shadeChange();
		Position mousePosition(const QPoint &) const;

	protected slots:
		void slotMaximize();
                void slotAbove();
                void slotBelow();
                void slotShade();
		void menuButtonPressed();
	        void menuButtonReleased();
                void keepAboveChange( bool );
                void keepBelowChange( bool );

	private:
		void doShape();
		void calcHiddenButtons();
		void addClientButtons( const QString& s, bool isLeft=true );
                bool isTool() const;

		enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose,
			BtnMenu, BtnSticky, BtnAbove, BtnBelow, BtnShade, BtnCount };
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
