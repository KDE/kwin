/*
  $Id$

  Gallium-IceWM themeable KWin client

  Copyright 2001
	Karol Szwed <gallium@kde.org>
	http://gallium.n3.net/

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  -----------------------------------------------------------------------------
  This client loads most icewm 1.0.X pixmap themes, without taking into account
  specific font settings for clients, or coloured mouse cursors. Titlebar
  fonts can be changed via the kde control center. Bi-colour mouse cursors
  may be added in future if requested by users, as well as theme font support.
  Any styles using inbuilt icewm titlebar drawing without using pixmaps (e.g.
  Warp4, win95 etc.) are not fully supported, and may cause drawing errors, 
  as these themes use in-built icewm drawing mechanisms.

  When a pixmap theme is not present (or a corrupt one is present) then very
  plain title decorations are painted instead, so that users don't see
  non-painted window areas where possible ;)

  At a later date, frame shaping may be added if really requested, and an 
  update to support the latest icewm 1.1.X theme format may be made.
*/

#ifndef __KDEGALLIUM_ICEWM_H
#define __KDEGALLIUM_ICEWM_H

#include <qbutton.h>
#include <qlayout.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;
class QBoxLayout;
class QGridLayout;

namespace IceWM {

using namespace KWinInternal;

// Pixmap group
enum { InActive=0, Active };
// Pixmap stretching mode
enum { Vertical=0, Horizontal=1 };


// Handles the resetClients() signal from the Options class,
// and manages the dynamic pixmaps, configuration and theme changing
class ThemeHandler: public QObject
{
	Q_OBJECT
	public:
		ThemeHandler();
		~ThemeHandler();

	public slots:
		void slotReset();

	private:
		bool initialized;
		QString themeName;

		void readConfig();
		QColor decodeColor( QString& s );
		bool isFrameValid();
		void initTheme();
		void freePixmaps();
		void freePixmapGroup( QPixmap* p[] );
		void setPixmap( QPixmap* p[], QString s1, QString s2, bool 
						stretch=false, bool stretchHoriz=true );
		QPixmap* stretchPixmap( QPixmap* src, bool stretchHoriz=true,
								int stretchSize=-1);
		QPixmap* duplicateValidPixmap( bool act, int size = -1 );
		void convertButtons( QString& s );
		QString reverseString( QString s );
};


class IceWMButton : public KWinButton
{
	public:
		IceWMButton( Client *parent=0, const char *name=0, 
					 QPixmap* (*p)[2]=0L, bool isToggle=false,
					 const QString& tip=NULL );
		void  usePixmap( QPixmap* (*p)[2] );
		QSize sizeHint() const;
		void  turnOn( bool isOn );
		int   last_button;

	protected:
		void mousePressEvent( QMouseEvent* e );
		void mouseReleaseEvent( QMouseEvent* e );

		void drawButton( QPainter *p );
		void drawButtonLabel( QPainter * ) {;}

	private:
		Client* client;
		QPixmap* (*pix)[2]; // Points to active/inactive pixmap array
};


class IceWMClient : public Client
{
    Q_OBJECT
	public:
	    IceWMClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
	    ~IceWMClient(); 

	protected:
	    void resizeEvent( QResizeEvent* );
	    void paintEvent( QPaintEvent* );
	    void showEvent( QShowEvent* );
	    void mouseDoubleClickEvent( QMouseEvent * );
	    void captionChange( const QString& name );
	    void maximizeChange(bool m);
	    void activeChange(bool);
	//	void shadeChange(bool);     /* KWin Client class doesn't provide this yet */
	    MousePosition mousePosition( const QPoint& ) const;
		void renderMenuIcons();
		void iconChange();
		void stickyChange(bool on);
 
	protected slots:
	    void slotMaximize();
	    void menuButtonPressed();

	private:
	    // These are all the icewm button types :)
	    enum Buttons{ BtnSysMenu=0, BtnClose, BtnMaximize, BtnMinimize,
					  BtnHide, BtnRollup, BtnDepth, BtnCount };

		QString shortenCaption( const QString* s );
		void calcHiddenButtons();
	    int  titleTextWidth( const QString& s );
		void addClientButtons( const QString& s );
		QSpacerItem* addPixmapSpacer( QPixmap* p[], 
			QSizePolicy::SizeType = QSizePolicy::Maximum, int hsize = -1 );

	    IceWMButton* button[ IceWMClient::BtnCount ];
	    QPixmap*     menuButtonWithIconPix[2]; 
	    QSpacerItem* titleSpacerJ;
	    QSpacerItem* titleSpacerL;
	    QSpacerItem* titleSpacerS;
	    QSpacerItem* titleSpacerP;
	    QSpacerItem* titlebar;
	    QSpacerItem* titleSpacerM;
	    QSpacerItem* titleSpacerB;
    	QSpacerItem* titleSpacerR;
	    QSpacerItem* titleSpacerQ;
	    QBoxLayout*  hb;
	    QGridLayout* grid;
};

};

#endif

// vim: ts=4
