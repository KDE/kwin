// $Id$
#ifndef __MODSYSTEMCLIENT_H
#define __MODSYSTEMCLIENT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include <qbutton.h>
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QLabel;
class QSpacerItem;

namespace ModernSystem {

class ModernSys;

class ModernButton : public QButton
{
    Q_OBJECT
public:
    ModernButton( ModernSys *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL,
                 const QString& tip=NULL, const int realizeBtns = LeftButton);
    void setBitmap(const unsigned char *bitmap);
    void reset();
    QSize sizeHint() const;
protected:
    void mousePressEvent( QMouseEvent* e );
    void mouseReleaseEvent( QMouseEvent* e );

    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    QBitmap deco;
    ModernSys* client;

    int realizeButtons;
public:
    int last_button;
};

class ModernSys : public KDecoration
{
    Q_OBJECT
public:
    ModernSys( KDecorationBridge* b, KDecorationFactory* f );
    ~ModernSys(){;}
    void init();
protected:
    bool eventFilter( QObject* o, QEvent* e );
    void drawRoundFrame(QPainter &p, int x, int y, int w, int h);
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void captionChange();
    void maximizeChange();
    void doShape();
    void recalcTitleBuffer();
    void activeChange();
    Position mousePosition( const QPoint& ) const;
    void desktopChange();
    void shadeChange();
    void iconChange();
    QSize minimumSize() const;
    void resize( const QSize& );
    void borders( int&, int&, int&, int& ) const;
    void reset( unsigned long );
protected slots:
    void maxButtonClicked();
private:
	enum Buttons{ BtnClose = 0, BtnSticky, BtnMinimize, BtnMaximize, BtnHelp };
    ModernButton* button[5];
    QSpacerItem* titlebar;
    QPixmap titleBuffer;
    QString oldTitle;
};

class ModernSysFactory : public QObject, public KDecorationFactory
{
Q_OBJECT
public:
    ModernSysFactory();
    virtual ~ModernSysFactory();
    virtual KDecoration* createDecoration( KDecorationBridge* );
    virtual bool reset( unsigned long changed );
    QValueList< BorderSize > borderSizes() const;
private:
    bool read_config();
};

}

#endif
