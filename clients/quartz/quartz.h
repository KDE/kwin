/********************************************************************
  Gallium-Quartz KWin client
 
  Copyright (C) 2005 Sandro Giessl <sandro@giessl.com>
  Copyright 2001
    Karol Szwed <gallium@kde.org>
    http://gallium.n3.net/
 
  Based on the KDE default client.
 
  Includes mini titlebars for ToolWindow Support.
  Button positions are now customizable.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __KDEGALLIUM_QUARTZ_H
#define __KDEGALLIUM_QUARTZ_H

#include <QBitmap>
#include "../../lib/kcommondecoration.h"
#include "../../lib/kdecorationfactory.h"


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
		virtual bool supports( Ability ability ) const;
		virtual QList< BorderSize > borderSizes() const;

	private:
		void readConfig();
		void createPixmaps();
		void freePixmaps();
		void drawBlocks(QPixmap* pi, QPixmap &p, const QColor &c1, const QColor &c2);
};


class QuartzButton : public KCommonDecorationButton
{
	public:
		QuartzButton(ButtonType type, QuartzClient *parent, const char *name);
		~QuartzButton();
		void setBitmap(const unsigned char *bitmap);

		void reset(unsigned long changed);

	protected:
		void paintEvent(QPaintEvent *);
		void drawButton(QPainter *p);

		QBitmap* deco;
};


class QuartzClient : public KCommonDecoration
{
	public:
		QuartzClient(KDecorationBridge* bridge, KDecorationFactory* factory);
		~QuartzClient() {;}

		virtual QString visibleName() const;
		virtual QString defaultButtonsLeft() const;
		virtual QString defaultButtonsRight() const;
		virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
		virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
		virtual KCommonDecorationButton *createButton(ButtonType type);

		virtual void init();

	protected:
		virtual void reset( unsigned long changed );
		void paintEvent( QPaintEvent* );

	private:
		int 		  titleHeight, borderSize;
		bool          largeButtons;
};

}

#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
