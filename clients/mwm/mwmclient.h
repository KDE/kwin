#ifndef __MWMCLIENT_H
#define __MWMCLIENT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;

namespace KWinInternal {

class MwmClient;

enum Buttons { BtnMenu=0, BtnIconify=1, BtnMax=2 };

class MwmButton : public KWinInternal::KWinButton
{
public:
    MwmButton( MwmClient* parent=0, const char* name=0, int btnType=0,
               const QString& tip=NULL );
    void reset();

protected:
    virtual void drawButton( QPainter *p );

private:
    int m_btnType;
    Client* m_parent;
};

class MwmClient : public KWinInternal::Client
{
    Q_OBJECT
public:
    MwmClient( Workspace* ws, WId w, QWidget* parent=0, const char* name=0 );
    ~MwmClient() {};

protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
 
    void mouseDoubleClickEvent( QMouseEvent* );
    void init();
    void captionChange( const QString& );
    void maximizeChange( bool m );
    void activeChange( bool );
    MousePosition mousePosition( const QPoint& p ) const;

protected slots:
    void slotReset();
    void slotMaximize();
    void menuButtonPressed();

private:
    MwmButton* button[3];
    QSpacerItem* titlebar;
};                      

};

#endif

