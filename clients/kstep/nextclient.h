#ifndef __NEXTCLIENT_H
#define __NEXTCLIENT_H

#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;

namespace KWinInternal {

class NextButton : public KWinInternal::KWinButton
{
public:
    NextButton(Client *parent=0, const char *name=0,
               const unsigned char *bitmap=NULL, int bw=0, int bh=0,
               const QString& tip=NULL);
    void setBitmap(const unsigned char *bitmap, int bw, int bh);
    void reset();
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    KPixmap aBackground, iBackground;
    QBitmap deco;
    Client *client;
};

class NextClient : public KWinInternal::Client
{
    Q_OBJECT
public:
    NextClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~NextClient(){;}
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
 
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void activeChange(bool);

    MousePosition mousePosition(const QPoint &) const;

protected slots:
    void slotReset();
private:
    NextButton* button[3];
    QSpacerItem* titlebar;
};                      

};

#endif
