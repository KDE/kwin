/*
 * Laptop KWin Client
 *
 * Ported to the kde3.2 API by Luciano Montanaro <mikelima@cirulla.net>
 */
#ifndef __KDECLIENT_H
#define __KDECLIENT_H

#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QLabel;
class QSpacerItem;
class QBoxLayout;
class QGridLayout;

namespace Laptop {

class LaptopClient;

class LaptopButton : public QButton
{
public:
    LaptopButton(int w, int h, LaptopClient *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL, const QString& tip=NULL, const int realizeBtns = LeftButton);
    void setBitmap(const unsigned char *bitmap);
    void reset();
    QSize sizeHint() const;
    ButtonState last_button;

protected:
    void mousePressEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), (e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mousePressEvent( &me );
    }
    void mouseReleaseEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), (e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
	QButton::mouseReleaseEvent( &me );
    }
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *) {}
    LaptopClient *client;
    QSize defaultSize;
    QBitmap deco;
    int realizeButtons;
};

class LaptopClient : public KDecoration
{
    Q_OBJECT
public:
    enum Buttons{BtnHelp=0, BtnSticky, BtnMax, BtnIconify, BtnClose};
    LaptopClient( KDecorationBridge* b, KDecorationFactory* f );
    ~LaptopClient() {}
    void init();
protected:
    bool eventFilter( QObject* o, QEvent* e );
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent* );
    void captionChange();
    void maximizeChange();
    void doShape();
    void activeChange();
    Position mousePosition(const QPoint &) const;
    void desktopChange();
    void shadeChange();
    void iconChange();
    QSize minimumSize() const;
    void resize( const QSize& );
    void borders( int&, int&, int&, int& ) const;
    void reset( unsigned long );
    void calcHiddenButtons();
    void updateActiveBuffer();
private:
    bool isTool() const;
    bool isTransient() const;
protected slots:
    void slotMaximize();
private:
    LaptopButton* button[5];
    QGridLayout *g;
    QBoxLayout* hb;
    QSpacerItem* titlebar;
    QSpacerItem* spacer;
    KPixmap activeBuffer;
    int lastButtonWidth;
    int lastBufferWidth;
    bool hiddenItems;
    bool bufferDirty;
};

class LaptopClientFactory : public QObject, public KDecorationFactory
{
public:
    LaptopClientFactory();
    virtual ~LaptopClientFactory();
    virtual KDecoration* createDecoration( KDecorationBridge* );
    virtual bool reset( unsigned long changed );
    virtual QValueList< BorderSize > borderSizes() const;
private:
    void findPreferredHandleSize();
};

}

#endif
