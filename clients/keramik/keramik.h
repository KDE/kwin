/*
 * $Id$
 * 
 * Keramik KWin client (version 0.8)
 *
 * Copyright (C) 2002 Fredrik Höglund <fredrik@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __KERAMIK_H
#define __KERAMIK_H

#include <qdict.h>

#include "../../client.h"
#include "../../kwinbutton.h"

static QDict< QImage > imageDict;

#include "tiles.h"


class QSpacerItem;

using namespace KWinInternal;

namespace Keramik {

	enum TilePixmap  { TitleLeft=0, TitleCenter, TitleRight,
	                   CaptionSmallLeft, CaptionSmallCenter, CaptionSmallRight,
	                   CaptionLargeLeft, CaptionLargeCenter, CaptionLargeRight,
					   GrabBarLeft, GrabBarCenter, GrabBarRight,
	                   BorderLeft, BorderRight, NumTiles };

	enum Button      { MenuButton=0, StickyButton, HelpButton, MinButton,
	                   MaxButton, CloseButton, NumButtons };

	enum ButtonDeco  { Menu=0, Sticky, Unsticky, Help, Iconify, Maximize,
	                   Restore, Close, NumButtonDecos };

	struct SettingsCache
	{
		QColor  aTitleColor;
		QColor  iTitleColor;
		QColor  aTitleBlend;
		QColor  iTitleBlend;
		QColor  buttonColor;
		QString buttonsLeft;
		QString buttonsRight;
	};
					   
	class KeramikHandler : public QObject {

		Q_OBJECT
		
		public:
			KeramikHandler();
			~KeramikHandler();
			
			void reset();
			
			bool showAppIcons() const        { return showIcons; }
			bool useShadowedText() const     { return shadowedText; }
			bool largeCaptionBubbles() const { return !smallCaptionBubbles; }
			
			int titleBarHeight( bool large ) const {
				return ( large ? activeTiles[CaptionLargeCenter]->height()
						: activeTiles[CaptionSmallCenter]->height() );
			}
			
			const QPixmap *roundButton() const  { return titleButtonRound; }
			const QPixmap *squareButton() const { return titleButtonSquare; }
			const QBitmap *buttonDeco( ButtonDeco deco ) const
				{ return buttonDecos[ deco ]; }

			inline const QPixmap *tile( TilePixmap tilePix, bool active ) const;

		signals:
			void softReset();
			
		private:
			void readConfig();
			void createPixmaps();
			void destroyPixmaps();

			void flip( QPixmap *&, QPixmap *& );
			void flip( QPixmap *& );
			void pretile( QPixmap *&, int, Qt::Orientation );
			QPixmap *composite( QImage *, QImage * );
			QImage  *loadImage( const QString &, const QColor & );
			QPixmap *loadPixmap( const QString &, const QColor & );

		private:
			bool showIcons:1, shadowedText:1, smallCaptionBubbles:1;
			SettingsCache *settings_cache;
			
			QPixmap *activeTiles[ NumTiles ];
			QPixmap *inactiveTiles[ NumTiles ];
			QBitmap *buttonDecos[ NumButtonDecos ];

			QPixmap *titleButtonRound, *titleButtonSquare;
	
	}; // class KeramikHandler
	
	
	class KeramikButton : public KWinInternal::KWinButton
	{
		public:
			KeramikButton( Client *, const char *, Button, const QString & );
			~KeramikButton();
			
			int lastButton() const { return lastbutton; }
			
		private:
			void enterEvent( QEvent * );
			void leaveEvent( QEvent * );
			void mousePressEvent( QMouseEvent * );
			void mouseReleaseEvent( QMouseEvent * );
			void drawButton( QPainter * );
			
		private:
			Client *client;
			Button button;
			bool hover;
			int lastbutton;
	}; // class KeramikButton

	
	class KeramikClient : public KWinInternal::Client
	{
			Q_OBJECT
		
		public:
				
			KeramikClient( Workspace *, WId, QWidget *parent = 0L, const char *name = 0L );
			~KeramikClient();
		
		private:
			void addButtons( QHBoxLayout*, const QString & );
			void updateMask();
			void updateCaptionBuffer();
			void captionChange( const QString& );
			void iconChange();
			void activeChange( bool );
			void maximizeChange( bool );
			void stickyChange( bool );
			void resizeEvent( QResizeEvent *);
			void paintEvent( QPaintEvent *);
			void mouseDoubleClickEvent( QMouseEvent * );
			MousePosition mousePosition( const QPoint & ) const;

			void calculateCaptionRect();
			
			inline bool maximizedVertical() const { 
				return (isMaximized() && maximizeMode() != MaximizeHorizontal);
			}
			
			inline void setRectangle( XRectangle *r, int x, int y, int w, int h ) {
				r->x      = x;
				r->y      = y;
				r->width  = w;
				r->height = h;
			}
			
		private slots:
			void menuButtonPressed();	
			void slotMaximize();
			void reset();
			
		private:
			QSpacerItem   *topSpacer, *titlebar;
			KeramikButton *button[ NumButtons ];
			QRect          captionRect;
			QPixmap        captionBuffer;
			QPixmap       *activeIcon, *inactiveIcon;
			bool           captionBufferDirty:1, maskDirty:1;
			bool           largeCaption:1, largeTitlebar:1;	
	}; // class KeramikClient

} // namespace Keramik

#endif // ___KERAMIK_H

// vim: set noet ts=4 sw=4:
