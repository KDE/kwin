/*
  Gallium-Win2k KWin client
  
  Copyright 2001
    Karol Szwed <karlmail@usa.net>
    http://gallium.home.dhs.org/

  Based on the default KWin client.
*/

#ifndef __KDEGALLIUM_WIN2K_H
#define __KDEGALLIUM_WIN2K_H

#include <qtoolbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
class QLabel;
class QSpacerItem;
class QHBoxLayout;


class GalliumButton : public QButton
{

public:
    GalliumButton(Client *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL, bool large = false );
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
	QButton::mousePressEvent( &me );
    }
    void mouseReleaseEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), 
                         LeftButton, e->state() );
	QButton::mouseReleaseEvent( &me );
    }
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}

    QBitmap  deco;
    QPixmap  pix;
    Client*  client;
    bool     largeBtn;
};


class GalliumClient : public Client
{
    Q_OBJECT

public:
    enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose, BtnMenu, BtnCount };
    GalliumClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~GalliumClient() {;}

protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void captionChange( const QString& name );
    void maximizeChange(bool m);
    void activeChange(bool);

    void calcHiddenButtons();

protected slots:
    void slotReset();
    void slotMaximize();
    void menuButtonPressed();

private:
    GalliumButton* button[ GalliumClient::BtnCount ];
    int            lastButtonWidth;
    QSpacerItem*   titlebar;
    bool           hiddenItems;
    QHBoxLayout*   hb;
};


#endif
