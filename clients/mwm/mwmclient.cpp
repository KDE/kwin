#include "mwmclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

using namespace KWinInternal;

extern "C"
{
    Client *allocate(Workspace *ws, WId w)
    {
        return(new MwmClient(ws, w));
    }
}


static unsigned char close_bits[] = {
  0x03, 0x03, 0x87, 0x03, 0xce, 0x01, 0xfc, 0x00, 0x78, 0x00, 0x78, 0x00,
  0xfc, 0x00, 0xce, 0x01, 0x87, 0x03, 0x03, 0x03};

static unsigned char iconify_bits[] = {
  0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0xff, 0x03, 0xff, 0x03};

static unsigned char sticky_bits[] = {
  0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0xfe, 0x01, 0xfe, 0x01,
  0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00};

static unsigned char unsticky_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static KPixmap *aTitlePix=0;
static KPixmap *iTitlePix=0;
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

    aTitlePix = new KPixmap();
    aTitlePix->resize(32, 14);
    KPixmapEffect::gradient(*aTitlePix,
                            options->color(Options::TitleBar, true),
                            options->color(Options::TitleBlend, true),
                            KPixmapEffect::VerticalGradient);
    iTitlePix = new KPixmap();
    iTitlePix->resize(32, 14);
    KPixmapEffect::gradient(*iTitlePix,
                            options->color(Options::TitleBar, false),
                            options->color(Options::TitleBlend, false),
                            KPixmapEffect::VerticalGradient);
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

void MwmClient::slotReset()
{
    delete aTitlePix;
    delete iTitlePix;
    delete aFramePix;
    delete iFramePix;
    delete aHandlePix;
    delete iHandlePix;
    pixmaps_created = false;
    create_pixmaps();
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
}

MwmButton::MwmButton(QWidget *parent, const char *name,
                       const unsigned char *bitmap, int bw, int bh)
    : QButton(parent, name)
{
    QPainter p;

    aBackground.resize(18, 18);
    iBackground.resize(18, 18);
    reset();
    resize(18, 18);

    if(bitmap)
        setBitmap(bitmap, bw, bh);
}

void MwmButton::reset()
{
    QPainter p;

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
    aInternal.resize(12, 12);
    KPixmap iInternal;
    iInternal.resize(12, 12);
    KPixmapEffect::gradient(iInternal,
                            options->color(Options::ButtonBlend, true),
                            options->color(Options::ButtonBg, true),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(aInternal,
                            options->color(Options::ButtonBg, false),
                            options->color(Options::ButtonBlend, false),
                            KPixmapEffect::DiagonalGradient);

    p.begin(&iBackground);
    p.drawPixmap(3, 3, iInternal);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();

    p.begin(&aBackground);
    p.drawPixmap(3, 3, aInternal);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
}

void MwmButton::setBitmap(const unsigned char *bitmap, int w, int h)
{
    deco = QBitmap(w, h, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void MwmButton::drawButton(QPainter *p)
{
    if(isDown())
        p->drawPixmap(0, 0, aBackground);
    else
        p->drawPixmap(0, 0, iBackground);

    p->setPen(options->color(Options::ButtonFg, isDown()));
    p->drawPixmap(isDown()? 5 : 4, isDown() ? 5 : 4, deco);
}

MwmClient::MwmClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    create_pixmaps();
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QHBoxLayout *windowLayout = new QHBoxLayout();

    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(windowLayout, 1);
    mainLayout->addSpacing(6);

    windowLayout->addSpacing(1);
    windowLayout->addWidget(windowWrapper(), 1);
    windowLayout->addSpacing(1);


    button[0] = new MwmButton(this, "close", close_bits, 10, 10);
    button[1] = new MwmButton(this, "sticky");
    if(isSticky())
        button[1]->setBitmap(unsticky_bits, 10, 10);
    else
        button[1]->setBitmap(sticky_bits, 10, 10);
    button[2] = new MwmButton(this, "iconify", iconify_bits, 10, 10);

    connect( button[0], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[2], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    titleLayout->addWidget( button[2] );
    titlebar = new QSpacerItem(10, 16, QSizePolicy::Expanding,
                               QSizePolicy::Minimum );
    titleLayout->addItem(titlebar);
    titleLayout->addWidget( button[1] );
    titleLayout->addWidget( button[0] );
    for ( int i = 0; i < 3; i++) {
        button[i]->setMouseTracking( TRUE );
        button[i]->setFixedSize( 18, 18 );
    }

}

void MwmClient::resizeEvent( QResizeEvent* e)
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

void MwmClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


void MwmClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    p.setPen(Qt::black);
    p.drawRect(rect());

    QRect t = titlebar->geometry();
    t.setTop(1);
    p.drawTiledPixmap(t.x()+1, t.y()+1, t.width()-2, t.height()-2,
                      isActive() ? *aTitlePix : *iTitlePix);
    qDrawShadePanel(&p, t.x(), t.y(), t.width(), t.height()-1,
                   options->colorGroup(Options::TitleBar, isActive()));
    p.drawLine(t.x(), t.bottom(), t.right(), t.bottom());
    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    p.setClipping( FALSE );

    t.setTop( 2 );
    t.setHeight(t.height()-4);
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
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

void MwmClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        setShade( !isShade() );
    workspace()->requestFocus( this );
}

void MwmClient::stickyChange(bool on)
{
    if(on)
        button[1]->setBitmap(unsticky_bits, 8, 8);
    else
        button[1]->setBitmap(sticky_bits, 8, 8);
}


void MwmClient::init()
{
    Client::init();
}


#include "mwmclient.moc"
