// $Id$
#ifndef __MODSYSTEMCLIENT_H
#define __MODSYSTEMCLIENT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
class QLabel;
class QSpacerItem;

namespace KWinInternal {

// get rid of autohide :P
class ModernButton : public QButton
{
    Q_OBJECT
public:
    ModernButton(Client *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL);
    void setBitmap(const unsigned char *bitmap);
    void reset();
    QSize sizeHint() const;
protected:
    void mousePressEvent( QMouseEvent* e );
    void mouseReleaseEvent( QMouseEvent* e );

    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    QBitmap deco;
    Client *client;
public:
    int last_button;
};

class ModernSys : public KWinInternal::Client
{
    Q_OBJECT
public:
    ModernSys( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~ModernSys(){;}
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
    MousePosition mousePosition( const QPoint& ) const;
protected slots:
    void slotReset();
    void maxButtonClicked();
private:
	enum Buttons{ BtnClose = 0, BtnSticky, BtnMinimize, BtnMaximize, BtnHelp };
    ModernButton* button[5];
    QSpacerItem* titlebar;
    QPixmap titleBuffer;
    QString oldTitle;
};

};

#endif
