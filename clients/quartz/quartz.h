/*
  Gallium-Quartz KWin client

  Copyright 2001
    Karol Szwed <karlmail@usa.net>
    http://gallium.n3.net/

  Based upon the Win2K kwin client, which is based on the
  KDE default client.

  Includes mini titlebars for ToolWindow Support.
*/

#ifndef __KDEGALLIUM_QUARTZ_H
#define __KDEGALLIUM_QUARTZ_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
class QLabel;
class QSpacerItem;
class QHBoxLayout;

namespace KWinInternal {

class QuartzButton : public QButton
{

public:
    QuartzButton(Client *parent=0, const char *name=0, bool largeButton=true,
                 const unsigned char *bitmap=NULL );
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
    bool     large;
};


class QuartzClient : public KWinInternal::Client
{
    Q_OBJECT

public:
    enum Buttons{ BtnHelp=0, BtnMax, BtnIconify, BtnClose, BtnMenu, BtnCount };
    QuartzClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~QuartzClient() {;}
    int titleHeight;

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
    QuartzButton* button[ QuartzClient::BtnCount ];
    int           lastButtonWidth;
    QSpacerItem*  titlebar;
    bool          hiddenItems;
    QHBoxLayout*  hb;
    bool          largeButtons;
};

};

#endif
