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
    Client *allocate(Workspace *ws, WId w, int)
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
// buttons active, inactive, up, down, and 2 sizes :P
static KPixmap *btnPix1;
static KPixmap *btnDownPix1;
static KPixmap *iBtnPix1;
static KPixmap *iBtnDownPix1;
static KPixmap *btnPix2;
static KPixmap *btnDownPix2;
static KPixmap *iBtnPix2;
static KPixmap *iBtnDownPix2;
static QColor btnForeground;

static bool pixmaps_created = false;

static int titleHeight = -1;
static int btnWidth1 = 18;
static int btnWidth2 = 28;


static void drawButtonFrame(KPixmap *pix, const QColorGroup &g, bool sunken)
{
    QPainter p;
    int w = pix->width();
    int h = pix->height();
    p.begin(pix);
    p.setPen(sunken ? g.dark() : g.light());
    p.drawLine(0, 0, w-1, 0);
    p.drawLine(0, 0, 0, w-1);
    p.setPen(sunken ? g.light() : g.dark());
    p.drawLine(w-1, 0, w-1, h-1);
    p.drawLine(0, h-1, w-1, h-1);
    p.end();
}

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    // titlebar
    QPainter p;
    QPainter maskPainter;
    int i, x, y;
    titlePix = new QPixmap(33, 12);
    QBitmap mask(33, 12);
    mask.fill(Qt::color0);

    p.begin(titlePix);
    maskPainter.begin(&mask);
    maskPainter.setPen(Qt::color1);
    for(i=0, y=2; i < 3; ++i, y+=4){
        for(x=1; x <= 33; x+=3){
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
        // buttons (active, 2 sizes)
        QColorGroup g = options->colorGroup(Options::ButtonBg, true);
        QColor c = g.background();
        btnPix1 = new KPixmap;
        btnPix1->resize(btnWidth1, titleHeight-2);
        KPixmapEffect::gradient(*btnPix1, c.light(120), c.dark(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(btnPix1, g, false);
        btnDownPix1 = new KPixmap;
        btnDownPix1->resize(btnWidth1, titleHeight-2);
        KPixmapEffect::gradient(*btnDownPix1, c.dark(120), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(btnDownPix1, g, true);
        btnPix2 = new KPixmap;
        btnPix2->resize(btnWidth2, titleHeight-2);
        KPixmapEffect::gradient(*btnPix2, c.light(120), c.dark(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(btnPix2, g, false);
        btnDownPix2 = new KPixmap;
        btnDownPix2->resize(btnWidth2, titleHeight-2);
        KPixmapEffect::gradient(*btnDownPix2, c.dark(120), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(btnDownPix2, g, true);
        // inactive
        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        iBtnPix1 = new KPixmap;
        iBtnPix1->resize(btnWidth1, titleHeight-2);
        KPixmapEffect::gradient(*iBtnPix1, c.light(120), c.dark(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(iBtnPix1, g, false);
        iBtnDownPix1 = new KPixmap;
        iBtnDownPix1->resize(btnWidth1, titleHeight-2);
        KPixmapEffect::gradient(*iBtnDownPix1, c.dark(120), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(iBtnDownPix1, g, true);
        iBtnPix2 = new KPixmap;
        iBtnPix2->resize(btnWidth2, titleHeight-2);
        KPixmapEffect::gradient(*iBtnPix2, c.light(120), c.dark(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(iBtnPix2, g, false);
        iBtnDownPix2 = new KPixmap;
        iBtnDownPix2->resize(btnWidth2, titleHeight-2);
        KPixmapEffect::gradient(*iBtnDownPix2, c.dark(120), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        drawButtonFrame(iBtnDownPix2, g, true);
    }
    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 128)
        btnForeground = Qt::black;
    else
        btnForeground = Qt::white;
}


LaptopClientButton::LaptopClientButton(int w, int h, Client *parent, const char *name,
                           const unsigned char *bitmap)
    : QButton(parent, name)
{
    client = parent;
    setFixedSize(w, h);

    if(bitmap)
        setBitmap(bitmap);
}

/*
QSize LaptopClientButton::sizeHint() const
{
    return(QSize(22, 12));
}*/

void LaptopClientButton::reset()
{
    repaint(false);
}

void LaptopClientButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void LaptopClientButton::drawButton(QPainter *p)
{
    bool smallBtn = width() == btnWidth1;
    if(btnPix1){
        if(client->isActive()){
            if(isDown())
                p->drawPixmap(0, 0, smallBtn ? *btnDownPix1 : *btnDownPix2);
            else
                p->drawPixmap(0, 0, smallBtn ? *btnPix1 : *btnPix2);
        }
        else{
            if(isDown())
                p->drawPixmap(0, 0, smallBtn ? *iBtnDownPix1 : *iBtnDownPix2);
            else
                p->drawPixmap(0, 0, smallBtn ? *iBtnPix1 : *iBtnPix2);
        }
    }
    else{
        QColorGroup g = options->colorGroup(Options::ButtonBg,
                                            client->isActive());
        int w = width();
        int h = height();
        p->fillRect(1, 1, w-2, h-2, isDown() ? g.mid() : g.button());
        p->setPen(isDown() ? g.dark() : g.light());
        p->drawLine(0, 0, w-1, 0);
        p->drawLine(0, 0, 0, w-1);
        p->setPen(isDown() ? g.light() : g.dark());
        p->drawLine(w-1, 0, w-1, h-1);
        p->drawLine(0, h-1, w-1, h-1);
    }

    p->setPen(btnForeground);
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
        delete btnPix1;
        delete btnDownPix1;
        delete iBtnPix1;
        delete iBtnDownPix1;
        delete btnPix2;
        delete btnDownPix2;
        delete iBtnPix2;
        delete iBtnDownPix2;
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
    : Client( ws, w, parent, name, WResizeNoErase | WNorthWestGravity )
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

    button[0] = new LaptopClientButton(28, titleHeight-2, this, "close", close_bits);
    button[1] = new LaptopClientButton(18, titleHeight-2, this, "sticky");
    if(isSticky())
        button[1]->setBitmap(unsticky_bits);
    else
        button[1]->setBitmap(sticky_bits);
    button[2] = new LaptopClientButton(28, titleHeight-2, this, "iconify",
                                 iconify_bits);
    button[3] = new LaptopClientButton(28, titleHeight-2, this, "maximize",
                                 maximize_bits);
    if(help){
        button[4] = new LaptopClientButton(18, titleHeight-2, this, "help",
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
    if ( isVisibleToTLW() ) {
	int dx = 0;
	int dy = 0;
	if ( e->oldSize().width() != width() )
	    dx = 16 + QABS( e->oldSize().width() -  width() );
	if ( e->oldSize().height() != height() )
	    dy = 16 + QABS( e->oldSize().height() -  height() );
	if ( dy )
	    update( 0, height() - dy + 1, width(), dy );
	if ( dx ) {
	    update( width() - dx + 1, 0, dx, height() );
	    update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	    update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
	    // titlebar needs no background
	    QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
	}
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
    qDrawShadePanel(&p, r.x()+1, r.y()+1, r.width()-2, r.height()-2, g, false, 1 );
//                     &g.brush(QColorGroup::Background));
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
                   1, 0 ); //, &g.brush(QColorGroup::Background));
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
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
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

Client::MousePosition LaptopClient::mousePosition( const QPoint& pos) const
{
    QPoint p = pos;
    if ( pos.x() > rect().right() - 12 )
	p.rx() += 8;
    if ( pos.y() > rect().bottom() - 12 )
	p.ry() += 8;
    return Client::mousePosition( p );
}

void LaptopClient::activeChange(bool)
{
    repaint(false);
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
    button[3]->reset();
    if(button[4])
        button[4]->reset();
}


#include "laptopclient.moc"
