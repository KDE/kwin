#ifndef __MWMCLIENT_H
#define __MWMCLIENT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
class QLabel;
class QSpacerItem;


// get rid of autohide :P
class MwmButton : public QButton
{
public:
    MwmButton(QWidget *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL, int bw=0, int bh=0);
    void setBitmap(const unsigned char *bitmap, int bw, int bh);
    void reset();
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    KPixmap aBackground, iBackground;
    QBitmap deco;
};

class MwmClient : public Client
{
    Q_OBJECT
public:
    MwmClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~MwmClient(){;}
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
 
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
protected slots:
    void slotReset();
private:
    MwmButton* button[3];
    QSpacerItem* titlebar;
};                      




#endif

