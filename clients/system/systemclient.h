#ifndef __SYSTEMCLIENT_H
#define __SYSTEMCLIENT_H

#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"


class QLabel;
class QSpacerItem;

namespace KWinInternal {

class SystemButton : public KWinInternal::KWinButton
{
    Q_OBJECT
public:
    SystemButton(Client *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL, const QString& tip=NULL);
    void setBitmap(const unsigned char *bitmap);
    void reset();
    QSize sizeHint() const;
signals:
    void clicked( int );
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    QBitmap deco;

    void mousePressEvent( QMouseEvent* e );
    void mouseReleaseEvent( QMouseEvent* e );

private slots:
    void handleClicked();

private:
    int last_button;
    Client* client;
};

class SystemClient : public KWinInternal::Client
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
    void recalcTitleBuffer();
    void activeChange(bool);
protected slots:
    void slotReset();
private slots:
    void maxButtonClicked( int );

private:
    SystemButton* button[5];
    QSpacerItem* titlebar;
    QPixmap titleBuffer;
    QString oldTitle;
};

};

#endif
