#include "systemclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <qbitmap.h>
#include "workspace.h"
#include "options.h"

static unsigned char iconify_bits[] = {
    0x00, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00 };

static unsigned char close_bits[] = {
    0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3, 0x00 };

static unsigned char maximize_bits[] = {
    0x3f, 0x9f, 0xcf, 0x67, 0x33, 0x19, 0x0c, 0x06 };

static unsigned char unsticky_bits[] = {
    0x00, 0x18, 0x18, 0x7e, 0x7e, 0x18, 0x18, 0x00};

static unsigned char sticky_bits[] = {
    0x00, 0x00, 0x00, 0x7e, 0x7e, 0x00, 0x00, 0x00};

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
    p.setPen(options->color(Options::ButtonBlend, false));
    p.drawRect(0, 0, 14, 14);
    p.end();

    p.begin(&aBackground);
    p.drawPixmap(2, 2, aInternal);
    p.setPen(options->color(Options::ButtonBlend, true));
    p.drawRect(0, 0, 14, 14);
    p.end();

    resize(14, 14);

    QBitmap mask;
    mask.resize(14, 14);
    mask.fill(color1);
    p.begin(&mask);
    p.setPen(color0);
    p.drawPoint(0, 0);
    p.drawPoint(13, 0);
    p.drawPoint(0, 13);
    p.drawPoint(13, 13);
    p.end();
    setMask(mask);

    if(bitmap)
        setBitmap(bitmap);
}

void SystemButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap);
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

    QGridLayout* g = new QGridLayout(this, 0, 0, 1);
    g->setRowStretch(1, 10);
    g->addWidget(windowWrapper(), 1, 1 );
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );
    g->addRowSpacing(2, 6);

    button[0] = new SystemButton(this, "close", close_bits);
    button[1] = new SystemButton(this, "sticky");
    if(isSticky())
        button[1]->setBitmap(unsticky_bits);
    else
        button[1]->setBitmap(sticky_bits);
    button[2] = new SystemButton(this, "iconify", iconify_bits);
    button[3] = new SystemButton(this, "maximize", maximize_bits);

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
    hb->addSpacing(2);
    hb->addWidget( button[1] );
    hb->addSpacing(2);
    hb->addWidget( button[2] );
    hb->addSpacing(2);
    hb->addWidget( button[3] );
    hb->addSpacing(2);

    for ( int i = 0; i < 4; i++) {
        button[i]->setMouseTracking( TRUE );
        button[i]->setFixedSize( 14, 14 );
    }


}

void SystemClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

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


void SystemClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect t = titlebar->geometry();
    t.setTop( 1 );

    // if we have a pixmapped bg use that, otherwise use color settings
    if(colorGroup().brush(QColorGroup::Background).pixmap())
        qDrawShadePanel(&p, rect(), colorGroup(), false, 1,
                        &colorGroup().brush(QColorGroup::Background));
    else
        qDrawShadePanel(&p, rect(),
                        options->colorGroup(Options::Frame, isActive()), false, 1,
                        &options->colorGroup(Options::Frame, isActive()).
                        brush(QColorGroup::Button));


    t.setTop( 2 );
    if(isActive())
        p.drawTiledPixmap(t, *titlePix);
    else if(colorGroup().brush(QColorGroup::Background).pixmap())
        p.fillRect(t, colorGroup().brush(QColorGroup::Background));
    else
        p.fillRect(t, options->colorGroup(Options::Frame, false).
                   brush(QColorGroup::Button));
        
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
                   fm.width(caption())+8, t.height(),
                   colorGroup().brush(QColorGroup::Background));
    }
    
    p.drawText( t, AlignCenter, caption() );

    qDrawShadePanel(&p, rect().x()+1, rect().bottom()-6, 24, 6,
                    options->colorGroup(Options::Handle, isActive()), false);
    p.drawTiledPixmap(rect().x()+2, rect().bottom()-5, 22, 4,
                      isActive() ? *aHandlePix : *iHandlePix);

    qDrawShadePanel(&p, rect().x()+25, rect().bottom()-6, rect().width()-50, 6,
                    options->colorGroup(Options::Frame, isActive()), false);
    p.drawTiledPixmap(rect().x()+26, rect().bottom()-5, rect().width()-52, 4,
                      isActive() ? *aFramePix : *iFramePix);

    qDrawShadePanel(&p, rect().right()-24, rect().bottom()-6, 24, 6,
                    options->colorGroup(Options::Handle, isActive()), false);
    p.drawTiledPixmap(rect().right()-23, rect().bottom()-5, 22, 4,
                      isActive() ? *aHandlePix : *iHandlePix);
}

void SystemClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        setShade( !isShade() );
    workspace()->requestFocus( this );
}

void SystemClient::stickyChange(bool on)
{
    if(on)
        button[1]->setBitmap(unsticky_bits);
    else
        button[1]->setBitmap(sticky_bits);
}


void SystemClient::init()
{
    //
}

