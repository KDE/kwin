/*
 * $Id$
 *
 * Gallium-Win2k KWin client
 *
 * Copyright 2001
 *   Karol Szwed <gallium@kde.org>
 *   http://gallium.n3.net/
 *
 * Based on the default KWin client.
 *
 * Updated to support toolwindows 3/2001 (KS)
 *
 */

#ifndef __KDEGALLIUM_WIN2K_H
#define __KDEGALLIUM_WIN2K_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;
class QHBoxLayout;

namespace KWinInternal {

class GalliumButton : public KWinInternal::KWinButton
{

public:
    GalliumButton(Client *parent=0, const char *name=0, 
                  const unsigned char *bitmap=NULL,
                  bool menuButton=false, bool isMini=false,
                  const QString& tip=NULL);
    void setBitmap(const unsigned char *bitmap);
    void setPixmap(const QPixmap &p);
    void reset();

    QSize sizeHint() const;
    int   last_button;

protected:
    void mousePressEvent( QMouseEvent* e )
    {
       last_button = e->button();
       QMouseEvent me ( e->type(), e->pos(), e->globalPos(),
                        LeftButton, e->state() );
       KWinButton::mousePressEvent( &me );
    }

    void mouseReleaseEvent( QMouseEvent* e )
    {
       last_button = e->button();
       QMouseEvent me ( e->type(), e->pos(), e->globalPos(), 
                        LeftButton, e->state() );
       KWinButton::mouseReleaseEvent( &me );
    }

    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}

    QBitmap  deco;
    QPixmap  pix;
    bool     menuBtn;
    bool     miniBtn;
    Client*  client;
};


class GalliumClient : public KWinInternal::Client
{
    Q_OBJECT

public:
    GalliumClient( Workspace *ws, WId w, QWidget *parent=0,
		   const char *name=0 );
    ~GalliumClient() {;}

protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void captionChange( const QString& name );
    void maximizeChange(bool m);
    void activeChange(bool);
    void iconChange();

    void calcHiddenButtons();

protected slots:
    void slotReset();
    void slotMaximize();
    void menuButtonPressed();

private:
    enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose,
		  BtnMenu, BtnCount };

    GalliumButton* button[ GalliumClient::BtnCount ];
    int            lastButtonWidth;
    int            titleHeight;
    QSpacerItem*   titlebar;
    bool           hiddenItems;
    QHBoxLayout*   hb;
    bool           smallButtons;
};

};

#endif
