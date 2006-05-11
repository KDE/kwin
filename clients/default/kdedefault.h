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

#include <qbitmap.h>
#include <QDateTime>
#include <kcommondecoration.h>
#include <kdecorationfactory.h>

class QSpacerItem;
class QBoxLayout;
class QGridLayout;
class QPixmap;

namespace Default {

class KDEDefaultClient;

class KDEDefaultHandler: public KDecorationFactory
{
	public:
		KDEDefaultHandler();
		~KDEDefaultHandler();
                KDecoration* createDecoration( KDecorationBridge* b );
		bool reset( unsigned long changed );
		virtual QList< BorderSize > borderSizes() const;
		virtual bool supports( Ability ability );

	private:
		unsigned long readConfig( bool update );
		void createPixmaps();
		void freePixmaps();
		void drawButtonBackground(QPixmap *pix,
				const QPalette &g, bool sunken);
};


// class KDEDefaultButton : public QButton, public KDecorationDefines
class KDEDefaultButton : public KCommonDecorationButton
{
	public:
		KDEDefaultButton(ButtonType type, KDEDefaultClient *parent, const char *name);
		~KDEDefaultButton();

		void reset(unsigned long changed);

		void setBitmap(const unsigned char *bitmap);

	protected:
		void enterEvent(QEvent *);
		void leaveEvent(QEvent *);
		void paintEvent(QPaintEvent *);
		void drawButton(QPainter *p);
		void drawButtonLabel(QPainter*) {;}

		QBitmap* deco;
		bool    large;
		bool	isMouseOver;
};


class KDEDefaultClient : public KCommonDecoration
{
	public:
		KDEDefaultClient( KDecorationBridge* b, KDecorationFactory* f );
		~KDEDefaultClient() {;}

		virtual QString visibleName() const;
		virtual QString defaultButtonsLeft() const;
		virtual QString defaultButtonsRight() const;
		virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
		virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
		virtual KCommonDecorationButton *createButton(ButtonType type);

		virtual QRegion cornerShape(WindowCorner corner);

		void init();
		void reset( unsigned long changed );

	protected:
		void paintEvent( QPaintEvent* );

	private:
		bool mustDrawHandle() const;
		int           titleHeight;
};

}

#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
