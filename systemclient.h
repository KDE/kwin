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
    SystemButton(QWidget *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL);
    void setBitmap(const unsigned char *bitmap);
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
    void drawRoundFrame(QPainter &p, int x, int y, int w, int h);
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void maximizeChange(bool m);
    void doShape();
protected slots:
    void slotContextHelp();
private:
    SystemButton* button[5];
    QSpacerItem* titlebar;
};                      




#endif
