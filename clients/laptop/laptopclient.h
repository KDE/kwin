#ifndef __KDECLIENT_H
#define __KDECLIENT_H

#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;
class QHBoxLayout;

namespace KWinInternal {

class LaptopClientButton : public KWinInternal::KWinButton
{
public:
    LaptopClientButton(int w, int h, Client *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL, const QString& tip=NULL);
    void setBitmap(const unsigned char *bitmap);
    void reset();
    QSize sizeHint() const;
    int last_button;

protected:
    void mousePressEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	KWinButton::mousePressEvent( &me );
    }
    void mouseReleaseEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	KWinButton::mouseReleaseEvent( &me );
    }
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *) {}
    QSize defaultSize;
    QBitmap deco;
    Client *client;
};

class LaptopClient : public KWinInternal::Client
{
    Q_OBJECT
public:
    enum Buttons{BtnHelp=0, BtnSticky, BtnMax, BtnIconify, BtnClose};
    LaptopClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~LaptopClient() {}
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent* );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void maximizeChange(bool m);
    void doShape();
    void activeChange(bool);

    void calcHiddenButtons();
    void updateActiveBuffer();

    MousePosition mousePosition(const QPoint &) const;

protected slots:
    void slotReset();
    void slotMaximize();
private:
    LaptopClientButton* button[5];
    int lastButtonWidth;
    QSpacerItem* titlebar;
    bool hiddenItems;
    QHBoxLayout* hb;
    KPixmap activeBuffer;
    bool bufferDirty;
    int lastBufferWidth;
};

};

#endif
