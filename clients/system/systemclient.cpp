#include "systemclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

extern "C"
{
    Client *allocate(Workspace *ws, WId w)
    {
        return(new SystemClient(ws, w));
    }
}


static unsigned char iconify_bits[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18};
    
static unsigned char close_bits[] = {
    0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3, 0x00 };

static unsigned char maximize_bits[] = {
    0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0xff, 0xff };

static unsigned char minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};
    
static unsigned char unsticky_bits[] = {
    0x00, 0x18, 0x18, 0x7e, 0x7e, 0x18, 0x18, 0x00};

static unsigned char sticky_bits[] = {
    0x00, 0x00, 0x00, 0x7e, 0x7e, 0x00, 0x00, 0x00};

static unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};
    
static QPixmap *titlePix=0;
static KPixmap *aFramePix=0;
static KPixmap *iFramePix=0;
static KPixmap *aHandlePix=0;
static KPixmap *iHandlePix=0;
static bool pixmaps_created = false;

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    // titlebar
    QPainter p;
    QPainter maskPainter;
    int i, y;
    titlePix = new QPixmap(32, 14);
    QBitmap mask(32, 14);
    mask.fill(Qt::color0);
    
    p.begin(titlePix);
    maskPainter.begin(&mask);
    maskPainter.setPen(Qt::color1);
    for(i=0, y=2; i < 4; ++i, y+=3){
        p.setPen(options->color(Options::TitleBar, true).light(150));
        p.drawLine(0, y, 31, y);
        maskPainter.drawLine(0, y, 31, y);
        p.setPen(options->color(Options::TitleBar, true).dark(120));
        p.drawLine(0, y+1, 31, y+1);
        maskPainter.drawLine(0, y+1, 31, y+1);
    }
    p.end();
    maskPainter.end();
    titlePix->setMask(mask);

    // Bottom frame gradient
    aFramePix = new KPixmap();
    aFramePix->resize(32, 6);
    KPixmapEffect::gradient(*aFramePix,
                            options->color(Options::Frame, true).light(150),
                            options->color(Options::Frame, true).dark(120),
                            KPixmapEffect::VerticalGradient);
    iFramePix = new KPixmap();
    iFramePix->resize(32, 6);
    KPixmapEffect::gradient(*iFramePix,
                            options->color(Options::Frame, false).light(150),
                            options->color(Options::Frame, false).dark(120),
                            KPixmapEffect::VerticalGradient);

    // Handle gradient
    aHandlePix = new KPixmap();
    aHandlePix->resize(32, 6);
    KPixmapEffect::gradient(*aHandlePix,
                            options->color(Options::Handle, true).light(150),
                            options->color(Options::Handle, true).dark(120),
                            KPixmapEffect::VerticalGradient);
    iHandlePix = new KPixmap();
    iHandlePix->resize(32, 6);
    KPixmapEffect::gradient(*iHandlePix,
                            options->color(Options::Handle, false).light(150),
                            options->color(Options::Handle, false).dark(120),
                            KPixmapEffect::VerticalGradient);

}


SystemButton::SystemButton(QWidget *parent, const char *name,
                           const unsigned char *bitmap)
    : QButton(parent, name)
{
    QPainter p;

    aBackground.resize(14, 14);
    iBackground.resize(14, 14);

    QColor hColor(options->color(Options::ButtonBg, true));
    QColor lColor(options->color(Options::ButtonBlend, true));
    // only do this if we can dim/brighten equally
    if(hColor.red() < 226 && hColor.green() < 226 && hColor.blue() < 226)
        hColor.setRgb(hColor.red()+30, hColor.green()+30, hColor.blue()+30);
    if(lColor.red() > 29 && lColor.green() > 29 && lColor.blue() > 29)
        lColor.setRgb(lColor.red()-30, lColor.green()-30, lColor.blue()-30);
    KPixmapEffect::gradient(iBackground, hColor, lColor,
                            KPixmapEffect::DiagonalGradient);

    hColor = options->color(Options::ButtonBlend, false);
    lColor = options->color(Options::ButtonBg, false);
    if(hColor.red() > 29 && hColor.green() > 29 && hColor.blue() > 29)
        hColor.setRgb(hColor.red()-30, hColor.green()-30, hColor.blue()-30);
    if(lColor.red() < 226 && lColor.green() < 226 && lColor.blue() < 226)
        lColor.setRgb(lColor.red()+30, lColor.green()+30, lColor.blue()+30);
    KPixmapEffect::gradient(aBackground, hColor, lColor,
                            KPixmapEffect::DiagonalGradient);

    KPixmap aInternal;
    aInternal.resize(10, 10);
    KPixmap iInternal;
    iInternal.resize(10, 10);
    KPixmapEffect::gradient(iInternal,
                            options->color(Options::ButtonBlend, true),
                            options->color(Options::ButtonBg, true),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(aInternal,
                            options->color(Options::ButtonBg, false),
                            options->color(Options::ButtonBlend, false),
                            KPixmapEffect::DiagonalGradient);

    p.begin(&iBackground);
    p.drawPixmap(2, 2, iInternal);
    p.setPen(options->color(Options::ButtonBg, false));
    p.drawLine(0, 13, 13, 13);
    p.drawLine(13, 0, 13, 13);
    p.setPen(options->color(Options::ButtonBlend, false));
    p.drawLine(0, 0, 12, 0);
    p.drawLine(0, 0, 0, 12);
    p.end();

    p.begin(&aBackground);
    p.drawPixmap(2, 2, aInternal);
    p.setPen(options->color(Options::ButtonFg, true));
    p.drawRect(0, 0, 14, 14);
    
    p.end();

    resize(14, 14);

    if(bitmap)
        setBitmap(bitmap);
}

void SystemButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void SystemButton::drawButton(QPainter *p)
{
    if(isDown())
        p->drawPixmap(0, 0, aBackground);
    else
        p->drawPixmap(0, 0, iBackground);

    p->setPen(options->color(Options::ButtonFg, isDown()));
    p->drawPixmap(isDown() ? 4 : 3, isDown() ? 4 : 3, deco);
}

SystemClient::SystemClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    create_pixmaps();
    bool help = providesContextHelp();

    QGridLayout* g = new QGridLayout(this, 0, 0, 2);
    g->setRowStretch(1, 10);
    g->addWidget(windowWrapper(), 1, 1 );
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, 2);
    g->addColSpacing(2, 2);
    g->addRowSpacing(2, 6);

    button[0] = new SystemButton(this, "close", close_bits);
    button[1] = new SystemButton(this, "sticky");
    if(isSticky())
        button[1]->setBitmap(unsticky_bits);
    else
        button[1]->setBitmap(sticky_bits);
    button[2] = new SystemButton(this, "iconify", iconify_bits);
    button[3] = new SystemButton(this, "maximize", maximize_bits);
    if(help){
        button[4] = new SystemButton(this, "help", question_bits);
        connect( button[4], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }

    connect( button[0], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[2], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[3], SIGNAL( clicked() ), this, ( SLOT( maximize() ) ) );

    QHBoxLayout* hb = new QHBoxLayout();
    g->addLayout( hb, 0, 1 );
    hb->addSpacing(2);
    hb->addWidget( button[0] );
    titlebar = new QSpacerItem(10, 16, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(3);
    if(help){
        hb->addWidget( button[4] );
        hb->addSpacing(1);
    }
    hb->addWidget( button[1] );
    hb->addSpacing(1);
    hb->addWidget( button[2] );
    hb->addSpacing(1);
    hb->addWidget( button[3] );
    hb->addSpacing(3);

    for ( int i = 0; i < (help ? 5 : 4); i++) {
        button[i]->setMouseTracking( TRUE );
        button[i]->setFixedSize( 14, 14 );
    }
}

void SystemClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
    doShape();
    if ( isVisibleToTLW() && !testWFlags( WNorthWestGravity )) {
        QPainter p( this );
	QRect t = titlebar->geometry();
	t.setTop( 0 );
	QRegion r = rect();
	r = r.subtract( t );
	p.setClipRegion( r );
	p.eraseRect( rect() );
    }
}

void SystemClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}

void SystemClient::drawRoundFrame(QPainter &p, int x, int y, int w, int h)
{
    kDrawRoundButton(&p, x, y, w, h,
                     options->colorGroup(Options::Frame, isActive()), false);

    /*
    int x2=x+w-1, y2=y+h-1;
    QPointArray hPntArray, lPntArray;
    hPntArray.putPoints(0, 12, x+4,y+1, x+5,y+1, // top left
                        x+3,y+2, x+2,y+3, x+1,y+4, x+1,y+5,
                        x+1,y2-5, x+1,y2-4, x+2,y2-3, // half corners
                        x2-5,y+1, x2-4,y+1, x2-3,y+2);

    lPntArray.putPoints(0, 17, x2-5,y2-1, x2-4,y2-1, // btm right
                        x2-3,y2-2, x2-2,y2-3, x2-1,y2-5, x2-1,y2-4,
 
                        x+3,y2-2, x+4,y2-1, x+5,y2-1, //half corners
                        x2-2,y+3, x2-1,y+4, x2-1,y+5,

                        x2-5,y2-2, x2-4,y2-2, // testing
                        x2-3,y2-3,
                        x2-2,y2-5, x2-2,y2-4);

    p.setPen(options->colorGroup(Options::Frame, isActive()).light());
    p.drawLine(x+6, y, x2-6, y);
    p.drawLine(0, y+6, 0, y2-6);
    p.drawPoints(hPntArray);
    p.setPen(options->colorGroup(Options::Frame, isActive()).dark());
    p.drawLine(x+6, y2, x2-6, y2);
    p.drawLine(x+6, y2-1, x2-6, y2-1);
    p.drawLine(x2, y+6, x2, y2-6);
    p.drawLine(x2-1, y+6, x2-1, y2-6);
    p.drawPoints(lPntArray); */
}

void SystemClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect t = titlebar->geometry();
    t.setTop( 1 );

    QBrush fillBrush(colorGroup().brush(QColorGroup::Background).pixmap() ?
                     colorGroup().brush(QColorGroup::Background) :
                     options->colorGroup(Options::Frame, isActive()).
                     brush(QColorGroup::Button));

    p.fillRect(rect(), fillBrush);
    drawRoundFrame(p, 0, 0, width(), height());
    t.setTop( 2 );
    if(isActive())
        p.drawTiledPixmap(t, *titlePix);

    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    p.setClipping( FALSE );

    t.setTop( 2 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    if(isActive()){
        QFontMetrics fm(options->font(true));
        p.fillRect(t.x()+((t.width()-fm.width(caption()))/2)-4, t.y(),
                   fm.width(caption())+8, t.height(), fillBrush);
    }

    p.drawText( t, AlignCenter, caption() );

    p.setPen(options->colorGroup(Options::Frame, isActive()).light());
    p.drawLine(width()-20, height()-7, width()-10,  height()-7);
    p.drawLine(width()-20, height()-5, width()-10,  height()-5);
    p.setPen(options->colorGroup(Options::Frame, isActive()).dark());
    p.drawLine(width()-20, height()-6, width()-10,  height()-6);
    p.drawLine(width()-20, height()-4, width()-10,  height()-4);
}

#define QCOORDARRLEN(x) sizeof(x)/(sizeof(QCOORD)*2)

void SystemClient::doShape()
{
    // using a bunch of QRect lines seems much more efficent than bitmaps or
    // point arrays
    
    QRegion mask;
    kRoundMaskRegion(mask, 0, 0, width(), height());
    /*
    (QRect(6, 0, width()-12, height()));
    mask += QRegion(QRect(5, 1, 1, height()-2)); // left
    mask += QRegion(QRect(4, 1, 1, height()-2));
    mask += QRegion(QRect(3, 2, 1, height()-4));
    mask += QRegion(QRect(2, 3, 1, height()-6));
    mask += QRegion(QRect(1, 4, 1, height()-8));
    mask += QRegion(QRect(0, 6, 1, height()-12));
    int x2 = width()-1;
    mask += QRegion(QRect(x2-5, 1, 1, height()-2)); // right
    mask += QRegion(QRect(x2-4, 1, 1, height()-2));
    mask += QRegion(QRect(x2-3, 2, 1, height()-4));
    mask += QRegion(QRect(x2-2, 3, 1, height()-6));
    mask += QRegion(QRect(x2-1, 4, 1, height()-8));
    mask += QRegion(QRect(x2, 6, 1, height()-12)); */
    setMask(mask);
}

void SystemClient::showEvent(QShowEvent *ev)
{
    Client::showEvent(ev);
    doShape();
    repaint();
}

void SystemClient::windowWrapperShowEvent( QShowEvent* )
{
    doShape();
}                                                                               

void SystemClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        setShade( !isShade() );
    workspace()->requestFocus( this );
}

void SystemClient::stickyChange(bool on)
{
    button[1]->setBitmap(on ? unsticky_bits : sticky_bits);
}

void SystemClient::maximizeChange(bool m)
{
    button[3]->setBitmap(m ? minmax_bits : maximize_bits);
}

void SystemClient::init()
{
    //
}


