#include "nextclient.h"
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <klocale.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

using namespace KWinInternal;



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
static KPixmap *aBtn=0;
static KPixmap *aBtnDown=0;
static KPixmap *iBtn=0;
static KPixmap *iBtnDown=0;
static bool pixmaps_created = false;
static QColor *btnForeground;

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

    iBtn = new KPixmap;
    iBtn->resize(18, 18);
    iBtnDown = new KPixmap;
    iBtnDown->resize(18, 18);
    aBtn = new KPixmap;
    aBtn->resize(18, 18);
    aBtnDown = new KPixmap;
    aBtnDown->resize(18, 18);
    KPixmap internal;
    internal.resize(12, 12);

    // inactive buttons
    QColor c(options->color(Options::ButtonBg, false));
    KPixmapEffect::gradient(*iBtn, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(iBtn, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    KPixmapEffect::gradient(*iBtnDown, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(iBtnDown, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    // active buttons
    c = options->color(Options::ButtonBg, true);
    KPixmapEffect::gradient(*aBtn, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(aBtn, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    KPixmapEffect::gradient(*aBtnDown, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(aBtnDown, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    QPainter p;
    p.begin(aBtn);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
    p.begin(iBtn);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
    p.begin(aBtnDown);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
    p.begin(iBtnDown);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();

    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 128)
        btnForeground = new QColor(Qt::black);
    else
        btnForeground = new QColor(Qt::white);
}

static void delete_pixmaps()
{
    delete aTitlePix;
    delete iTitlePix;
    delete aFramePix;
    delete iFramePix;
    delete aHandlePix;
    delete iHandlePix;
    delete aBtn;
    delete iBtn;
    delete aBtnDown;
    delete iBtnDown;
    delete btnForeground;

    pixmaps_created = false;
}

void NextClient::slotReset()
{
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
}

NextButton::NextButton(Client *parent, const char *name,
                       const unsigned char *bitmap, int bw, int bh,
                       const QString& tip)
    : KWinButton(parent, name, tip)
{
    setBackgroundMode( NoBackground );
    client = parent;
    resize(18, 18);

    if(bitmap)
        setBitmap(bitmap, bw, bh);
}

void NextButton::reset()
{
    repaint(false);
}

void NextButton::setBitmap(const unsigned char *bitmap, int w, int h)
{
    deco = QBitmap(w, h, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void NextButton::drawButton(QPainter *p)
{
    if(client->isActive())
        p->drawPixmap(0, 0, isDown() ? *aBtnDown : *aBtn);
    else
        p->drawPixmap(0, 0, isDown() ? *iBtnDown : *iBtn);

    p->setPen(*btnForeground);
    p->drawPixmap(isDown()? 5 : 4, isDown() ? 5 : 4, deco);
}

NextClient::NextClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    setBackgroundMode( NoBackground );
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


    button[0] = new NextButton(this, "close", close_bits, 10, 10, i18n("Close"));
    button[1] = new NextButton(this, "sticky", NULL, 0, 0, i18n("Sticky"));
    stickyChange(isSticky());
    button[2] = new NextButton(this, "iconify", iconify_bits, 10, 10, i18n("Minimize"));

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

void NextClient::resizeEvent( QResizeEvent* e)
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

void NextClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


void NextClient::paintEvent( QPaintEvent* )
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

    t.setTop( 1 );
    t.setHeight(t.height()-2);
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    p.drawText( t, AlignCenter | AlignVCenter, caption() );


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

void NextClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}

void NextClient::stickyChange(bool on)
{
    button[1]->setBitmap( on ? unsticky_bits : sticky_bits, 10, 10);
    button[1]->setTipText( on ? i18n("Un-Sticky") : i18n("Sticky") );
}


void NextClient::init()
{
    Client::init();
}

void NextClient::activeChange(bool)
{
    repaint(false);
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
}

  Client::MousePosition
NextClient::mousePosition( const QPoint& p ) const
{
  MousePosition m = Nowhere;

  if (p.y() < (height() - 6))
    m = Client::mousePosition(p);

  else {
    if (p.x() >= (width() - 25))
      m = BottomRight;
    else if (p.x() <= 25)
      m = BottomLeft;
    else
      m = Bottom;
  }

  return m;
}

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int )
    {
        return(new NextClient(ws, w));
    }
    void init()
    {
       create_pixmaps();
    }
    void reset()
    {
       delete_pixmaps();
       create_pixmaps();
       // Ensure change in tooltip state gets applied
       Workspace::self()->slotResetAllClientsDelayed();
    }
    void deinit()
    {
       delete_pixmaps();
    }
}

#include "nextclient.moc"
