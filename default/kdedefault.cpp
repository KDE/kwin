
#include <kconfig.h> // up here to avoid X11 header conflict :P
#include "kdedefault.h"
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
#include "../workspace.h"
#include "../options.h"

extern "C"
{
    Client *allocate(Workspace *ws, WId w)
    {
        return(new KDEClient(ws, w));
    }
}


static unsigned char iconify_bits[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18};

static unsigned char close_bits[] = {
    0x42, 0xe7, 0x7e, 0x3c, 0x3c, 0x7e, 0xe7, 0x42};

static unsigned char maximize_bits[] = {
    0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0xff, 0xff };

static unsigned char minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};

static unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};

static unsigned char unsticky_bits[] = {
   0x3c, 0x42, 0x99, 0xbd, 0xbd, 0x99, 0x42, 0x3c};

static unsigned char sticky_bits[] = {
   0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c};

static QPixmap *titlePix=0;
static KPixmap *aUpperGradient=0;
static KPixmap *iUpperGradient=0;
// buttons active, inactive, up, down, and 2 sizes :P
static KPixmap *btnPix1=0;
static KPixmap *iBtnPix1=0;
static KPixmap *btnDownPix1=0;
static KPixmap *iBtnDownPix1=0;
static KPixmap *btnPix2=0;
static KPixmap *btnDownPix2=0;
static KPixmap *iBtnPix2=0;
static KPixmap *iBtnDownPix2=0;
static QColor btnForeground;

static bool pixmaps_created = false;

static int titleHeight = 14; // configurable title height not implemented yet
static int btnWidth1 = 17;
static int btnWidth2 = 27;


static void drawButtonFrame(KPixmap *pix, const QColorGroup &g, bool sunken)
{
    QPainter p;
    int w = pix->width();
    int h = pix->height();
    int x2 = w-1;
    int y2 = h-1;
    p.begin(pix);

    if(sunken){
        qDrawShadePanel(&p, 0, 0, w, h, g, true, 2);
    }
    else{
        p.setPen(g.dark());
        p.drawRect(0, 0, w-1, h-1);
        p.setPen(g.light());
        p.drawLine(x2, 0, x2, y2);
        p.drawLine(0, y2, x2, y2);
        p.drawLine(1, 1, x2-2, 1);
        p.drawLine(1, 1, 1, y2-2);
        p.end();
    }
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
    }
    // buttons (active/inactive, sunken/unsunken, 2 sizes each)
    QColorGroup g = options->colorGroup(Options::ButtonBg, true);
    QColor c = g.background();
    btnPix1 = new KPixmap;
    btnPix1->resize(btnWidth1, titleHeight);
    btnDownPix1 = new KPixmap;
    btnDownPix1->resize(btnWidth1, titleHeight);
    btnPix2 = new KPixmap;
    btnPix2->resize(btnWidth2, titleHeight);
    btnDownPix2 = new KPixmap;
    btnDownPix2->resize(btnWidth2, titleHeight);
    iBtnPix1 = new KPixmap;
    iBtnPix1->resize(btnWidth1, titleHeight);
    iBtnDownPix1 = new KPixmap;
    iBtnDownPix1->resize(btnWidth1, titleHeight);
    iBtnPix2 = new KPixmap;
    iBtnPix2->resize(btnWidth2, titleHeight);
    iBtnDownPix2 = new KPixmap;
    iBtnDownPix2->resize(btnWidth2, titleHeight);
    if(QPixmap::defaultDepth() > 8){
        KPixmapEffect::gradient(*btnPix1, c.light(120), c.dark(130),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(*btnDownPix1, c.dark(130), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(*btnPix2, c.light(120), c.dark(130),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(*btnDownPix2, c.dark(130), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        KPixmapEffect::gradient(*iBtnPix1, c.light(120), c.dark(130),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(*iBtnDownPix1, c.dark(130), c.light(120),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(*iBtnPix2, c.light(120), c.dark(130),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(*iBtnDownPix2, c.dark(130), c.light(120),
                                KPixmapEffect::DiagonalGradient);
    }
    else{
        btnPix1->fill(c.rgb());
        btnDownPix1->fill(c.rgb());
        btnPix2->fill(c.rgb());
        btnDownPix2->fill(c.rgb());
        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        iBtnPix1->fill(c.rgb());
        iBtnDownPix1->fill(c.rgb());
        iBtnPix2->fill(c.rgb());
        iBtnDownPix2->fill(c.rgb());
    }
    g = options->colorGroup(Options::ButtonBg, true);
    c = g.background();
    drawButtonFrame(btnPix1, g, false);
    drawButtonFrame(btnDownPix1, g, true);
    drawButtonFrame(btnPix2, g, false);
    drawButtonFrame(btnDownPix2, g, true);
    g = options->colorGroup(Options::ButtonBg, false);
    c = g.background();
    drawButtonFrame(iBtnPix1, g, false);
    drawButtonFrame(iBtnDownPix1, g, true);
    drawButtonFrame(iBtnPix2, g, false);
    drawButtonFrame(iBtnDownPix2, g, true);

    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 128)
        btnForeground = Qt::black;
    else
        btnForeground = Qt::white;
}


KDEDefaultClientButton::KDEDefaultClientButton(int w, int h, Client *parent, const char *name,
                           const unsigned char *bitmap)
    : QButton(parent, name)
{
    client = parent;
    defaultSize = QSize(w, h);
    setFixedHeight(h);
    resize(defaultSize);
    if(bitmap)
        setBitmap(bitmap);
    //setBackgroundMode(QWidget::NoBackground);
}

QSize KDEDefaultClientButton::sizeHint() const
{
    return(defaultSize);
}

void KDEDefaultClientButton::reset()
{
    repaint(false);
}

void KDEDefaultClientButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void KDEDefaultClientButton::drawButton(QPainter *p)
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

void KDEClient::slotReset()
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
    int i;
    for(i=0; i < 5; ++i){
        if(button[i])
            button[i]->reset();
    }
    repaint();
}

KDEClient::KDEClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WNorthWestGravity )
{
    lastButtonWidth = 0;
    lastBufferWidth = 0;
 
    create_pixmaps();
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    bool help = providesContextHelp();


    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);
    g->addRowSpacing(2, 1);
    g->addWidget(windowWrapper(), 3, 1);
    g->setRowStretch(3, 10);
    g->addRowSpacing(4, 8); // bottom handles
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    button[BtnClose] = new KDEDefaultClientButton(27, titleHeight, this, "close", close_bits);
    button[BtnSticky] = new KDEDefaultClientButton(17, titleHeight, this, "sticky");
    if(isSticky())
        button[BtnSticky]->setBitmap(unsticky_bits);
    else
        button[BtnSticky]->setBitmap(sticky_bits);
    button[BtnIconify] = new KDEDefaultClientButton(27, titleHeight, this, "iconify",
                                          iconify_bits);
    button[BtnMax] = new KDEDefaultClientButton(27, titleHeight, this, "maximize",
                                      maximize_bits);
    if(help){
        button[BtnHelp] = new KDEDefaultClientButton(17, titleHeight, this, "help",
                                     question_bits);
        connect(button[BtnHelp], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }
    else
        button[BtnHelp] = NULL;

    connect( button[BtnClose], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[BtnSticky], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[BtnIconify], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[BtnMax], SIGNAL( clicked() ), this, ( SLOT( slotMaximize() ) ) );

    hb = new QHBoxLayout();
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 1, 1 );
    hb->addWidget( button[BtnClose]);
    hb->addSpacing(1);
    titlebar = new QSpacerItem(10, titleHeight, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(1);
    if(help){
        hb->addWidget( button[BtnHelp]);
    }
    hb->addWidget( button[BtnSticky]);
    hb->addWidget( button[BtnIconify]);
    hb->addWidget( button[BtnMax]);

    if ( isTransient() ) {
	button[BtnSticky]->hide();
	button[BtnIconify]->hide();
	button[BtnMax]->hide();
    }

    hiddenItems = false;
    bufferDirty = true;
}

void KDEClient::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
	maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
	maximize( MaximizeHorizontal );
    else
	maximize();
}

void KDEClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

    doShape();
    calcHiddenButtons();
    if ( isVisibleToTLW() ) {
	int dx = 0;
	int dy = 0;
	if ( e->oldSize().width() != width() )
	    dx = 32 + QABS( e->oldSize().width() -  width() );
	if ( e->oldSize().height() != height() )
	    dy = 8 + QABS( e->oldSize().height() -  height() );
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

void KDEClient::captionChange( const QString&str )
{
    bufferDirty = true;
    repaint( titlebar->geometry(), false );
}

void KDEClient::paintEvent( QPaintEvent* )
{
    QPainter p(this);
    QColorGroup g = options->colorGroup(Options::Frame, isActive());

    QRect r(rect());
    p.setPen(Qt::black);
    p.drawRect(r);
    // outer frame
    p.setPen(g.light());
    p.drawLine(r.x()+1, r.y()+1, r.right()-1, r.y()+1);
    p.drawLine(r.x()+1, r.y()+1, r.x()+1, r.bottom()-1);
    p.setPen(g.dark());
    p.drawLine(r.right()-1, r.y()+1, r.right()-1, r.bottom()-1);
    p.drawLine(r.x()+1, r.bottom()-1, r.right()-1, r.bottom()-1);

    // inner rect
    p.drawRect(r.x()+3, r.y()+titleHeight+3, r.width()-6,
               r.height()-titleHeight-10);
    // handles
    if(r.width() > 44){
        qDrawShadePanel(&p, r.x()+1, r.bottom()-6, 20,
                        6, g, false, 1, &g.brush(QColorGroup::Mid));
        qDrawShadePanel(&p, r.x()+21, r.bottom()-6, r.width()-42, 6,
                        g, false, 1, isActive() ?
                        &g.brush(QColorGroup::Background) :
                        &g.brush(QColorGroup::Mid));
        qDrawShadePanel(&p, r.right()-20, r.bottom()-6, 20, 6,
                        g, false, 1, &g.brush(QColorGroup::Mid));
    }
    else
        qDrawShadePanel(&p, r.x()+1, r.bottom()-6, r.width()-2, 6, g,
                        false, 1, isActive() ?
                        &g.brush(QColorGroup::Background) :
                        &g.brush(QColorGroup::Mid));

    r = titlebar->geometry();

    if(isActive()){
        updateActiveBuffer();
        p.drawPixmap(r.x(), r.y(), activeBuffer);
    }
    else{
        if(iUpperGradient)
            p.drawTiledPixmap(r.x(), r.y(), r.width(), r.height()-1,
                              *iUpperGradient);
        else
            p.fillRect(r.x(), r.y(), r.width(), r.height()-1,
                       options->color(Options::TitleBar, false));

        p.setFont(options->font(false));
        QFontMetrics fm(options->font(false));
        g = options->colorGroup(Options::TitleBar, false);
        if(iUpperGradient)
            p.drawTiledPixmap(r.x()+((r.width()-fm.width(caption()))/2)-4,
                              r.y(), fm.width(caption())+8, r.height()-1,
                              *iUpperGradient);
        else
            p.fillRect(r.x()+((r.width()-fm.width(caption()))/2)-4, r.y(),
                       fm.width(caption())+8, r.height()-1,
                       g.brush(QColorGroup::Background));
        p.setPen(g.mid());
        p.drawLine(r.x(), r.y(), r.right(), r.y());
        p.drawLine(r.x(), r.y(), r.x(), r.bottom());
        p.setPen(g.button());
        p.drawLine(r.right(), r.y(), r.right(), r.bottom());
        p.drawLine(r.x(), r.bottom(), r.right(), r.bottom());
        p.setPen(options->color(Options::Font, false));
        p.drawText(r.x(), r.y(), r.width(), r.height()-1,
                   AlignCenter, caption() );
    }
}

#define QCOORDARRLEN(x) sizeof(x)/(sizeof(QCOORD)*2)

void KDEClient::doShape()
{
    QRegion mask(QRect(0, 0, width(), height()));
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-1, 0, 1, 1);
    mask -= QRect(0, height()-1, 1, 1);
    mask -= QRect(width()-1, height()-1, 1, 1);

    setMask(mask);
}

void KDEClient::showEvent(QShowEvent *ev)
{
    Client::showEvent(ev);
    doShape();
    repaint();
}

void KDEClient::windowWrapperShowEvent( QShowEvent* )
{
    doShape();
}

void KDEClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}

void KDEClient::stickyChange(bool on)
{
    button[BtnSticky]->setBitmap(on ? unsticky_bits : sticky_bits);
}

void KDEClient::maximizeChange(bool m)
{
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
}

void KDEClient::init()
{
    //
}

void KDEClient::activeChange(bool)
{
    repaint(false);
    int i;
    for(i=0; i < 5; ++i){
        if(button[i])
            button[i]->reset();
    }
}


void KDEClient::calcHiddenButtons()
{
    // order of hiding is help, sticky, maximize, minimize, close;
    // buttons can have
    int minWidth = 32 + btnWidth2*3 + (providesContextHelp() ? btnWidth1*2 :
                                       btnWidth1);

    if(lastButtonWidth > width()){ // shrinking
        lastButtonWidth = width();
        if(width() < minWidth){
            hiddenItems = true;
            int i;
            for(i=0; i<5; ++i){
                if(button[i]){
                    if( !button[i]->isHidden() ) {
                        button[i]->hide();
                    }
                    minWidth-=button[i]->sizeHint().width();
                    if(width() >= minWidth)
                        return;
                }
            }
        }
    }
    else if(hiddenItems){ // expanding
        lastButtonWidth = width();
        int i;
        int totalSize=32;
        for(i=4; i>=0; --i){
            if(button[i]){
                if(button[i]->sizeHint().width() + totalSize <= width()){
                    totalSize+=button[i]->sizeHint().width();
                    if(button[i]->isHidden() && ( !isTransient() || ( i != BtnIconify && i != BtnSticky && i != BtnMax ) ) ){
                        button[i]->resize(button[i]->sizeHint());
                        button[i]->show();
                    }
                }
                else
                    return;
            }
        }
        // all items shown now
        hiddenItems = false;
    }
    else
        lastButtonWidth = width();
}

void KDEClient::updateActiveBuffer( )
{
    if( !bufferDirty && (lastBufferWidth == titlebar->geometry().width()))
        return;
    if ( titlebar->geometry().width() <= 0 || titlebar->geometry().height() <= 0 )
	return;
    lastBufferWidth = titlebar->geometry().width();
    bufferDirty = false;

    activeBuffer.resize(titlebar->geometry().width(),
                        titlebar->geometry().height());
    QPainter p;
    QRect r(0, 0, activeBuffer.width(), activeBuffer.height());
    p.begin(&activeBuffer);
    if(aUpperGradient){
        p.drawTiledPixmap(r, *aUpperGradient);
    }
    else{
        p.fillRect(r, options->color(Options::TitleBar, true));
    }
    if(titlePix)
        p.drawTiledPixmap(r, *titlePix);

    p.setFont(options->font(true));
    QFontMetrics fm(options->font(true));
    QColorGroup g = options->colorGroup(Options::TitleBar, true);
    if(aUpperGradient)
        p.drawTiledPixmap(r.x()+((r.width()-fm.width(caption()))/2)-4,
                          r.y(), fm.width(caption())+8, r.height()-1,
                          *aUpperGradient);
    else
        p.fillRect(r.x()+((r.width()-fm.width(caption()))/2)-4, 0,
                   fm.width(caption())+8, r.height(),
                   g.brush(QColorGroup::Background));
    p.setPen(g.mid());
    p.drawLine(r.x(), r.y(), r.right(), r.y());
    p.drawLine(r.x(), r.y(), r.x(), r.bottom());
    p.setPen(g.button());
    p.drawLine(r.right(), r.y(), r.right(), r.bottom());
    p.drawLine(r.x(), r.bottom(), r.right(), r.bottom());
    p.setPen(options->color(Options::Font, true));
    p.drawText(r.x(), r.y(), r.width(), r.height()-1,
               AlignCenter, caption() );
    p.end();
}


#include "kdedefault.moc"
