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
#include <kapp.h>
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
static KPixmap *aUpperGradient=0;
static KPixmap *iUpperGradient=0;
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
        p.setPen(options->color(Options::Groove, true).light(150));
        p.drawLine(0, y, 31, y);
        maskPainter.drawLine(0, y, 31, y);
        p.setPen(options->color(Options::Groove, true).dark(120));
        p.drawLine(0, y+1, 31, y+1);
        maskPainter.drawLine(0, y+1, 31, y+1);
    }
    p.end();
    maskPainter.end();
    titlePix->setMask(mask);

    if(QPixmap::defaultDepth() > 8 &&
       kapp->palette().normal().brush(QColorGroup::Background).pixmap()==NULL){
        aUpperGradient = new KPixmap;
        aUpperGradient->resize(32, 18);
        iUpperGradient = new KPixmap;
        iUpperGradient->resize(32, 18);
        QColor bgColor = kapp->palette().normal().background();
        KPixmapEffect::gradient(*aUpperGradient,
                                options->color(Options::Frame, true).light(130),
                                bgColor,
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iUpperGradient,
                                options->color(Options::Frame, false).light(130),
                                bgColor,
                                KPixmapEffect::VerticalGradient);
    }
}


SystemButton::SystemButton(QWidget *parent, const char *name,
                           const unsigned char *bitmap)
    : QButton(parent, name)
{
    aBackground.resize(14, 14);
    iBackground.resize(14, 14);
    reset();
    resize(14, 14);

    if(bitmap)
        setBitmap(bitmap);
}

QSize SystemButton::sizeHint() const
{
    return(QSize(14, 14));
}

void SystemButton::reset()
{
    QPainter p;
    QColor hColor(options->color(Options::ButtonBg, true));
    QColor lColor(options->color(Options::ButtonBlend, true));

    KPixmapEffect::gradient(aBackground, hColor.light(150), lColor.dark(150),
                            KPixmapEffect::DiagonalGradient);

    hColor = (options->color(Options::ButtonBg, false));
    lColor = (options->color(Options::ButtonBlend, false));
    KPixmapEffect::gradient(iBackground, hColor.light(150), lColor.dark(150),
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
    p.setPen(options->color(Options::ButtonBg, false).light(120));
    p.drawLine(0, 13, 13, 13);
    p.drawLine(13, 0, 13, 13);
    p.setPen(options->color(Options::ButtonBlend, false).dark(120));
    p.drawLine(0, 0, 12, 0);
    p.drawLine(0, 0, 0, 12);
    p.end();

    p.begin(&aBackground);
    p.drawPixmap(2, 2, aInternal);
    p.setPen(options->colorGroup(Options::ButtonBg, true).mid());
    p.drawRect(0, 0, 14, 14);
    
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
    p->drawPixmap(isDown() ? 4 : 3, isDown() ? 4 : 3, deco);
}

void SystemClient::slotReset()
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
}

SystemClient::SystemClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    create_pixmaps();
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
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
    else
        button[4] = NULL;

    connect( button[0], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[2], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[3], SIGNAL( clicked() ), this, ( SLOT( maximize() ) ) );

    QHBoxLayout* hb = new QHBoxLayout();
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 0, 1 );
    hb->addSpacing(2);
    hb->addWidget( button[0]);
    titlebar = new QSpacerItem(10, 14, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(3);
    if(help){
        hb->addWidget( button[4]);
        hb->addSpacing(1);
    }
    hb->addWidget( button[1]);
    hb->addSpacing(1);
    hb->addWidget( button[2]);
    hb->addSpacing(1);
    hb->addWidget( button[3]);
    hb->addSpacing(3);

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
    if(aUpperGradient){
        if(isActive()){
            p.drawTiledPixmap(0, 0, width(), 18, *aUpperGradient);
        }
        else{
            p.drawTiledPixmap(0, 0, width(), 18, *iUpperGradient);
        }
    }
    t.setTop( 1 );
    if(isActive())
        p.drawTiledPixmap(t, *titlePix);

    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    p.setClipping( FALSE );

    t.setTop( 2 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    p.setPen(options->color(Options::GrooveText, isActive()));
    p.setFont(options->font(isActive()));
    if(isActive()){
        QFontMetrics fm(options->font(true));
        if(aUpperGradient)
            p.drawTiledPixmap(t.x()+((t.width()-fm.width(caption()))/2)-4,
                              0, fm.width(caption())+8, 18,
                              isActive() ? *aUpperGradient :
                              *iUpperGradient);
        else
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

    drawRoundFrame(p, 0, 0, width(), height());
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


