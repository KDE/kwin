#ifndef __KWMTHEMECLIENT_H
#define __KWMTHEMECLIENT_H

#include <qbutton.h>
#include <qtoolbutton.h>
#include <kpixmap.h>
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;
class QGridLayout;

namespace KWinInternal{

class MyButton : public KWinInternal::KWinToolButton
{
public:
    MyButton(QWidget *parent=0, const char *name=0, const QString& tip=NULL)
        : KWinToolButton(parent, name, tip){setAutoRaise(true);}
protected:
    void drawButtonLabel(QPainter *p);
};

class KWMThemeClient : public KWinInternal::Client
{
    Q_OBJECT
public:
    KWMThemeClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~KWMThemeClient(){;}
protected:
    void doShape();
    void drawTitle(QPainter &p);
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void maximizeChange(bool m);
    void iconChange();
    MousePosition mousePosition(const QPoint &) const;
protected slots:
    void slotReset();
    void menuButtonPressed();
private:
    QPixmap buffer;
    KPixmap *aGradient, *iGradient;
    MyButton *maxBtn, *stickyBtn, *mnuBtn;
    QSpacerItem *titlebar;
    QGridLayout* layout;
};

};

#endif

