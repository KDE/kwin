#ifndef __SYSTEMCLIENT_H
#define __SYSTEMCLIENT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "client.h"
class QLabel;
class QSpacerItem;


// get rid of autohide :P
class SystemButton : public QButton
{
public:
    SystemButton(const unsigned char *bitmap, QWidget *parent=0,
                 const char *name=0);
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    KPixmap aBackground, iBackground;
    QBitmap deco;
};

class SystemClient : public Client
{
    Q_OBJECT
public:
    SystemClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~SystemClient(){;}
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
 
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
private:
    SystemButton* button[3];
    QSpacerItem* titlebar;
};                      




#endif
