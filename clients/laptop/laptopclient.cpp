#include <kconfig.h> // up here to avoid X11 header conflict :P
#include "laptopclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <kglobal.h>
#include <kapp.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

extern "C"
{
    Client *allocate(Workspace *ws, WId w)
    {
        return(new LaptopClient(ws, w));
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
static KPixmap *aUpperGradient=0;
static KPixmap *iUpperGradient=0;
static bool pixmaps_created = false;

static int titleHeight = -1;

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    // titlebar
    QPainter p;
    QPainter maskPainter;
    int i, x, y;
    titlePix = new QPixmap(32, 12);
    QBitmap mask(32, 12);
    mask.fill(Qt::color0);
    
    p.begin(titlePix);
    maskPainter.begin(&mask);
    maskPainter.setPen(Qt::color1);
    for(i=0, y=2; i < 3; ++i, y+=4){
        for(x=1; x < 30; x+=4){
            p.setPen(options->color(Options::TitleBar, true).light(150));
            p.drawPoint(x, y);
            maskPainter.drawPoint(x, y);
            p.setPen(options->color(Options::TitleBar, true).dark(150));
            p.drawPoint(x+1, y+1);
            maskPainter.drawPoint(x+1, y+1);
        }
    }
    p.end();
    maskPainter.end();
    titlePix->setMask(mask);

    if(QPixmap::defaultDepth() > 8){
        aUpperGradient = new KPixmap;
        aUpperGradient->resize(32, titleHeight+2);
        iUpperGradient = new KPixmap;
        iUpperGradient->resize(32, titleHeight+2);
        //QColor bgColor = kapp->palette().normal().background();
        QColor bgColor = options->color(Options::TitleBar, true);
        KPixmapEffect::gradient(*aUpperGradient,
                                bgColor.light(120),
                                bgColor.dark(120),
                                KPixmapEffect::VerticalGradient);
        bgColor = options->color(Options::TitleBar, false);
        KPixmapEffect::gradient(*iUpperGradient,
                                bgColor.light(120),
                                bgColor.dark(120),
                                KPixmapEffect::VerticalGradient);
    }
}


SystemButton::SystemButton(int w, int h, QWidget *parent, const char *name,
                           const unsigned char *bitmap)
    : QButton(parent, name)
{
    setFixedSize(w, h);
    aBackground.resize(w, h);
    iBackground.resize(w, h);
    reset();
    //resize(22, 12);

    if(bitmap)
        setBitmap(bitmap);
}

/*
QSize SystemButton::sizeHint() const
{
    return(QSize(22, 12));
}*/

void SystemButton::reset()
{
    QPainter p;
    int w = width();
    int h = height();
    QColor hColor(options->color(Options::ButtonBg, true));
    QColor lColor(options->color(Options::ButtonBlend, true));

    KPixmapEffect::gradient(aBackground, hColor.light(150), lColor.dark(150),
                            KPixmapEffect::DiagonalGradient);
    p.begin(&aBackground);
    p.setPen(options->colorGroup(Options::ButtonBg, false).dark());
    p.drawLine(0, 0, w-1, 0);
    p.drawLine(0, 0, 0, w-1);
    p.setPen(options->colorGroup(Options::ButtonBg, false).light());
    p.drawLine(w-1, 0, w-1, h-1);
    p.drawLine(0, h-1, w-1, h-1);
    p.end();

    hColor = (options->color(Options::ButtonBg, false));
    lColor = (options->color(Options::ButtonBlend, false));
    KPixmapEffect::gradient(iBackground, hColor.light(150), lColor.dark(150),
                            KPixmapEffect::DiagonalGradient);
    p.begin(&iBackground);
    p.setPen(options->colorGroup(Options::ButtonBg, false).light());
    p.drawLine(0, 0, w-1, 0);
    p.drawLine(0, 0, 0, h-1);
    p.setPen(options->colorGroup(Options::ButtonBg, false).dark());
    p.drawLine(w-1, 0, w-1, h-1);
    p.drawLine(0, h-1, w-1, h-1);
    p.end();


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
    int xOff = (width()-8)/2;
    int yOff = (height()-8)/2;
    p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, deco);
}

void LaptopClient::slotReset()
{
    delete titlePix;
    if(aUpperGradient){
        delete aUpperGradient;
        delete iUpperGradient;
    }
    pixmaps_created = false;
    create_pixmaps();
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
    button[3]->reset();
    if(button[4])
        button[4]->reset();
    repaint();
}

LaptopClient::LaptopClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    if(titleHeight == -1){
        KConfig *config = KGlobal::config();
        config->setGroup("Laptop");
        titleHeight = config->readNumEntry("TitleHeight", 14);
        if(titleHeight < 15)
            titleHeight = 15;
        if(titleHeight > 32)
            titleHeight = 32;
    }
    
    create_pixmaps();
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    bool help = providesContextHelp();

    
    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->addRowSpacing(0, 3);
    g->addRowSpacing(2, 1);
    g->addWidget(windowWrapper(), 3, 1);
    g->setRowStretch(3, 10);
    g->addRowSpacing(4, 12);
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);
    g->addColSpacing(2, 12);
    
    button[0] = new SystemButton(28, titleHeight-2, this, "close", close_bits);
    button[1] = new SystemButton(18, titleHeight-2, this, "sticky");
    if(isSticky())
        button[1]->setBitmap(unsticky_bits);
    else
        button[1]->setBitmap(sticky_bits);
    button[2] = new SystemButton(28, titleHeight-2, this, "iconify",
                                 iconify_bits);
    button[3] = new SystemButton(28, titleHeight-2, this, "maximize",
                                 maximize_bits);
    if(help){
        button[4] = new SystemButton(18, titleHeight-2, this, "help",
                                     question_bits);
        connect( button[4], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }
    else
        button[4] = NULL;

    connect( button[0], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[2], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[3], SIGNAL( clicked() ), this, ( SLOT( maximize() ) ) );

    QHBoxLayout* hb = new QHBoxLayout();
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 1, 1 );
    hb->addWidget( button[0]);
    hb->addSpacing(3);
    titlebar = new QSpacerItem(10, titleHeight, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(3);
    if(help){
        hb->addWidget( button[4]);
    }
    hb->addWidget( button[1]);
    hb->addWidget( button[2]);
    hb->addWidget( button[3]);

    /*
    for ( int i = 0; i < (help ? 5 : 4); i++) {
        button[i]->setFixedSize(22, 12);
    }*/
}

void LaptopClient::resizeEvent( QResizeEvent* e)
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

void LaptopClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}

void LaptopClient::paintEvent( QPaintEvent* )
{
    QPainter p(this);
    QColorGroup g = options->colorGroup(Options::Frame, isActive());

    // bottom handle
    QRect r(width()-33, height()-33, 33, 33);
    p.setPen(Qt::black);
    p.drawRect(r);
    qDrawShadePanel(&p, r.x()+1, r.y()+1, r.width()-2, r.height()-2, g, false, 1,
                    &g.brush(QColorGroup::Background));
    p.setPen(g.light());
    p.drawLine(r.right()-7, r.y()+1, r.right()-7, r.bottom()-8);
    p.drawLine(r.x()+1, r.bottom()-7, r.right()-7, r.bottom()-7);

    // frame
    r = QRect(rect());
    r.setHeight(r.height()-8);
    r.setWidth(r.width()-8);
    
    p.setPen(Qt::black);
    p.drawRect(r);
    qDrawShadeRect(&p, r.x()+1, r.y()+1, r.width()-2, r.height()-2, g, false,
                   1, 0, &g.brush(QColorGroup::Background));
    //qDrawShadePanel(&p, r.x()+3, r.y()+17, r.width()-6, r.height()-20, g,
    //                true, 1);
    qDrawShadePanel(&p, r.x()+3, r.y()+titleHeight+3, r.width()-6,
                    r.height()-titleHeight-6, g, true, 1);

    r = titlebar->geometry();
    KPixmap *grPix = isActive() ? aUpperGradient : iUpperGradient;
    
    if(grPix){
        p.drawTiledPixmap(r.x(), r.y(), r.width(), r.height()-1, *grPix);
    }
    else
        p.fillRect(r.x(), r.y(), r.width(), r.height()-1,
                   options->color(Options::TitleBar, isActive()));

    if(titlePix && isActive())
        p.drawTiledPixmap(r.x(), r.y(), r.width(), r.height()-1, *titlePix);
    

    QButton *rBtn = providesContextHelp() ? button[4] : button[1];
    int x2 = button[3]->x()+button[3]->width();

    int h = titleHeight-2;
    g = options->colorGroup(Options::Frame, isActive());
    p.setPen(g.dark());
    p.drawLine(rBtn->x()-1, rBtn->y()-1, x2, rBtn->y()-1);
    p.drawLine(rBtn->x()-1, rBtn->y()-1, rBtn->x()-1, rBtn->y()+h);
    p.setPen(g.midlight());
    p.drawLine(x2, rBtn->y()-1, x2, rBtn->y()+h);
    p.drawLine(rBtn->x()-1, rBtn->y()+h, x2, rBtn->y()+h);

    rBtn = button[0];
    x2 = button[0]->x()+28;
    p.setPen(g.dark());
    p.drawLine(rBtn->x()-1, rBtn->y()-1, x2, rBtn->y()-1);
    p.drawLine(rBtn->x()-1, rBtn->y()-1, rBtn->x()-1, rBtn->y()+h);
    p.setPen(g.midlight());
    p.drawLine(x2, rBtn->y()-1, x2, rBtn->y()+h);
    p.drawLine(rBtn->x()-1, rBtn->y()+h, x2, rBtn->y()+h);

    p.setFont(options->font(isActive()));
    QFontMetrics fm(options->font(true));
    g = options->colorGroup(Options::TitleBar, isActive());
    if(isActive()){
        if(grPix)
            p.drawTiledPixmap(r.x()+((r.width()-fm.width(caption()))/2)-4,
                              r.y(), fm.width(caption())+8, r.height()-1,
                              *grPix);
        else
            p.fillRect(r.x()+((r.width()-fm.width(caption()))/2)-4, r.y(),
                       fm.width(caption())+8, r.height()-1,
                       g.brush(QColorGroup::Background));
    }
    /*
    qDrawShadePanel(&p, r.x(), r.y(), r.width(), r.height()-1,
                    options->colorGroup(Options::TitleBar, isActive()),
                    true, 1);
                    */
    p.setPen(g.mid());
    p.drawLine(r.x(), r.y(), r.right(), r.y());
    p.drawLine(r.x(), r.y(), r.x(), r.bottom()-1);
    p.setPen(g.button());
    p.drawLine(r.right(), r.y(), r.right(), r.bottom()-1);
    p.drawLine(r.x(), r.bottom()-1, r.right(), r.bottom()-1);
    p.setPen(options->color(Options::Font, isActive()));
    p.drawText(r.x(), r.y(), r.width(), r.height()-1,
               AlignCenter, caption() );
}

#define QCOORDARRLEN(x) sizeof(x)/(sizeof(QCOORD)*2)

void LaptopClient::doShape()
{
    QRegion mask(QRect(0, 0, width()-8, height()-8));
    mask += QRect(width()-33, height()-33, 33, 33);
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-9, 0, 1, 1);
    mask -= QRect(0, height()-9, 1, 1);

    // handle
    mask -= QRect(width()-1, height()-33, 1, 1);
    mask -= QRect(width()-33, height()-1, 1, 1);
    mask -= QRect(width()-1, height()-1, 1, 1);
    setMask(mask);
}

void LaptopClient::showEvent(QShowEvent *ev)
{
    Client::showEvent(ev);
    doShape();
    repaint();
}

void LaptopClient::windowWrapperShowEvent( QShowEvent* )
{
    doShape();
}                                                                               

void LaptopClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        setShade( !isShade() );
    workspace()->requestFocus( this );
}

void LaptopClient::stickyChange(bool on)
{
    button[1]->setBitmap(on ? unsticky_bits : sticky_bits);
}

void LaptopClient::maximizeChange(bool m)
{
    button[3]->setBitmap(m ? minmax_bits : maximize_bits);
}

void LaptopClient::init()
{
    //
}


