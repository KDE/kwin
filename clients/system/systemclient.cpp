#include "systemclient.h"
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <kapp.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

using namespace KWinInternal;


static unsigned char iconify_bits[] = {
    0x00, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00};

static unsigned char maximize_bits[] = {
    0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00};

static unsigned char minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};

static unsigned char unsticky_bits[] = {
    0x00, 0x18, 0x18, 0x7e, 0x7e, 0x18, 0x18, 0x00};

static unsigned char sticky_bits[] = {
    0x00, 0x00, 0x00, 0x7e, 0x7e, 0x00, 0x00, 0x00};

static unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};

static KPixmap *aUpperGradient=0;
static KPixmap *iUpperGradient=0;

static KPixmap *btnPix=0;
static KPixmap *btnPixDown=0;
static KPixmap *iBtnPix=0;
static KPixmap *iBtnPixDown=0;
static QColor *btnForeground;

static bool pixmaps_created = false;

static void drawButtonFrame(KPixmap *pix, const QColorGroup &g)
{
    QPainter p;
    p.begin(pix);
    p.setPen(g.mid());
    p.drawLine(0, 0, 13, 0);
    p.drawLine(0, 0, 0, 13);
    p.setPen(g.light());
    p.drawLine(13, 0, 13, 13);
    p.drawLine(0, 13, 13, 13);
    p.setPen(g.dark());
    p.drawRect(1, 1, 12, 12);
    p.end();
}

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    if(QPixmap::defaultDepth() > 8){
        // titlebar
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

        // buttons
        KPixmap aPix;
        aPix.resize(12, 12);
        KPixmap iPix;
        iPix.resize(12, 12);
        KPixmap aInternal;
        aInternal.resize(8, 8);
        KPixmap iInternal;
        iInternal.resize(8, 8);

        QColor hColor(options->color(Options::ButtonBg, false));
        KPixmapEffect::gradient(iInternal,
                                hColor.dark(120),
                                hColor.light(120),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(iPix,
                                hColor.light(150),
                                hColor.dark(150),
                                KPixmapEffect::DiagonalGradient);

        hColor =options->color(Options::ButtonBg, true);
        KPixmapEffect::gradient(aInternal,
                                hColor.dark(120),
                                hColor.light(120),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(aPix,
                                hColor.light(150),
                                hColor.dark(150),
                                KPixmapEffect::DiagonalGradient);
        bitBlt(&aPix, 1, 1, &aInternal, 0, 0, 8, 8, Qt::CopyROP, true);
        bitBlt(&iPix, 1, 1, &iInternal, 0, 0, 8, 8, Qt::CopyROP, true);

        // normal buttons
        btnPix = new KPixmap;
        btnPix->resize(14, 14);
        bitBlt(btnPix, 2, 2, &aPix, 0, 0, 10, 10, Qt::CopyROP, true);
        drawButtonFrame(btnPix, options->colorGroup(Options::Frame, true));

        iBtnPix = new KPixmap;
        iBtnPix->resize(14, 14);
        bitBlt(iBtnPix, 2, 2, &iPix, 0, 0, 10, 10, Qt::CopyROP, true);
        drawButtonFrame(iBtnPix, options->colorGroup(Options::Frame, false));


        // pressed buttons
        hColor = options->color(Options::ButtonBg, false);
        KPixmapEffect::gradient(iInternal,
                                hColor.light(130),
                                hColor.dark(130),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(iPix,
                                hColor.light(150),
                                hColor.dark(150),
                                KPixmapEffect::DiagonalGradient);

        hColor =options->color(Options::ButtonBg, true);
        KPixmapEffect::gradient(aInternal,
                                hColor.light(130),
                                hColor.dark(130),
                                KPixmapEffect::DiagonalGradient);
        KPixmapEffect::gradient(aPix,
                                hColor.light(150),
                                hColor.dark(150),
                                KPixmapEffect::DiagonalGradient);
        bitBlt(&aPix, 1, 1, &aInternal, 0, 0, 8, 8, Qt::CopyROP, true);
        bitBlt(&iPix, 1, 1, &iInternal, 0, 0, 8, 8, Qt::CopyROP, true);

        btnPixDown = new KPixmap;
        btnPixDown->resize(14, 14);
        bitBlt(btnPixDown, 2, 2, &aPix, 0, 0, 10, 10, Qt::CopyROP, true);
        drawButtonFrame(btnPixDown, options->colorGroup(Options::Frame,
                                                        true));

        iBtnPixDown = new KPixmap;
        iBtnPixDown->resize(14, 14);
        bitBlt(iBtnPixDown, 2, 2, &iPix, 0, 0, 10, 10, Qt::CopyROP, true);
        drawButtonFrame(iBtnPixDown, options->colorGroup(Options::Frame,
                                                         false));
    }
    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 128)
        btnForeground = new QColor(Qt::black);
    else
        btnForeground = new QColor(Qt::white);
}

static void delete_pixmaps()
{
    if(aUpperGradient){
        delete aUpperGradient;
        delete iUpperGradient;
        delete btnPix;
        delete btnPixDown;
        delete iBtnPix;
        delete iBtnPixDown;
        aUpperGradient = 0;
    }
    delete btnForeground;
    pixmaps_created = false;
}

SystemButton::SystemButton(Client *parent, const char *name,
             const unsigned char *bitmap, const QString& tip)
    : KWinButton(parent, name, tip)
{
    setBackgroundMode( NoBackground );
    resize(14, 14);
    connect( this, SIGNAL( clicked() ), this, SLOT( handleClicked() ) );
    if(bitmap)
        setBitmap(bitmap);
    client = parent;
}

QSize SystemButton::sizeHint() const
{
    return(QSize(14, 14));
}

void SystemButton::reset()
{
    repaint(false);
}

void SystemButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void SystemButton::drawButton(QPainter *p)
{
    if(btnPixDown){
        if(client->isActive())
            p->drawPixmap(0, 0, isDown() ? *btnPixDown : *btnPix);
        else
            p->drawPixmap(0, 0, isDown() ? *iBtnPixDown : *iBtnPix);
    }
    else{
        QColorGroup g = options->colorGroup(Options::Frame,
                                            client->isActive());
        int x2 = width()-1;
        int y2 = height()-1;
        // outer frame
        p->setPen(g.mid());
        p->drawLine(0, 0, x2, 0);
        p->drawLine(0, 0, 0, y2);
        p->setPen(g.light());
        p->drawLine(x2, 0, x2, y2);
        p->drawLine(0, x2, y2, x2);
        p->setPen(g.dark());
        p->drawRect(1, 1, width()-2, height()-2);
        // inner frame
        g = options->colorGroup(Options::ButtonBg, client->isActive());
        p->fillRect(3, 3, width()-6, height()-6, g.background());
        p->setPen(isDown() ? g.mid() : g.light());
        p->drawLine(2, 2, x2-2, 2);
        p->drawLine(2, 2, 2, y2-2);
        p->setPen(isDown() ? g.light() : g.mid());
        p->drawLine(x2-2, 2, x2-2, y2-2);
        p->drawLine(2, x2-2, y2-2, x2-2);

    }

    if(!deco.isNull()){
        p->setPen(*btnForeground);
        p->drawPixmap(isDown() ? 4 : 3, isDown() ? 4 : 3, deco);
    }
}

void SystemButton::mousePressEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
    KWinButton::mousePressEvent( &me );
}

void SystemButton::mouseReleaseEvent( QMouseEvent* e )
{
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
    KWinButton::mouseReleaseEvent( &me );
}

void SystemButton::handleClicked()
{
    emit clicked( last_button );
}


void SystemClient::slotReset()
{
    titleBuffer.resize(0, 0);
    recalcTitleBuffer();
    repaint();
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
    button[3]->reset();
    if(button[4])
        button[4]->reset();
}

void SystemClient::maxButtonClicked( int button )
{
    switch ( button  ){
    case MidButton:
	maximize( MaximizeVertical );
	break;
    case RightButton:
	maximize( MaximizeHorizontal );
	break;
    default: //LeftButton:
	maximize( MaximizeFull );
	break;
    }
}

SystemClient::SystemClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    bool help = providesContextHelp();

    QGridLayout* g = new QGridLayout(this, 0, 0, 2);
    g->setRowStretch(1, 10);
    g->addWidget(windowWrapper(), 1, 1 );
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, 2);
    g->addColSpacing(2, 2);
    g->addRowSpacing(2, 6);

    button[0] = new SystemButton(this, "close", NULL, i18n("Close"));
    button[1] = new SystemButton(this, "sticky", NULL, i18n("Sticky"));
    if(isSticky())
        button[1]->setBitmap(unsticky_bits);
    else
        button[1]->setBitmap(sticky_bits);

    button[2] = new SystemButton(this, "iconify", iconify_bits, i18n("Minimize"));
    button[3] = new SystemButton(this, "maximize", maximize_bits, i18n("Maximize"));
    if(help){
        button[4] = new SystemButton(this, "help", question_bits, i18n("Help"));
        connect( button[4], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }
    else
        button[4] = NULL;

    connect( button[0], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[2], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[3], SIGNAL( clicked(int) ), this, ( SLOT( maxButtonClicked(int) ) ) );

    QHBoxLayout* hb = new QHBoxLayout(0);
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 0, 1 );
    hb->addSpacing(3);
    hb->addWidget( button[0]);
    titlebar = new QSpacerItem(10, 14, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addSpacing(3);
    hb->addItem(titlebar);
    hb->addSpacing(3);
    if(help){
        hb->addWidget( button[4]);
        hb->addSpacing(1);
    }
    hb->addWidget( button[1]);
    hb->addSpacing(1);

    if (isMinimizable()) {
         hb->addWidget(button[2]);
         hb->addSpacing(1);
    } else
         button[2]->hide();

    if (isMaximizable()) {
         hb->addWidget(button[3]);
         hb->addSpacing(3);
    } else
         button[3]->hide();

    setBackgroundMode(NoBackground);
    recalcTitleBuffer();

}

void SystemClient::resizeEvent( QResizeEvent* )
{
    //Client::resizeEvent( e );
    recalcTitleBuffer();
    doShape();
    /*
    if ( isVisibleToTLW() && !testWFlags( WNorthWestGravity )) {
        QPainter p( this );
	QRect t = titlebar->geometry();
	t.setTop( 0 );
	QRegion r = rect();
	r = r.subtract( t );
	p.setClipRegion( r );
	p.eraseRect( rect() );
        }*/
}

void SystemClient::recalcTitleBuffer()
{
    if(oldTitle == caption() && width() == titleBuffer.width())
        return;
    QFontMetrics fm(options->font(true));
    titleBuffer.resize(width(), 18);
    QPainter p;
    p.begin(&titleBuffer);
    if(aUpperGradient)
        p.drawTiledPixmap(0, 0, width(), 18, *aUpperGradient);
    else
        p.fillRect(0, 0, width(), 18,
                   options->colorGroup(Options::Frame, true).
                   brush(QColorGroup::Button));

    QRect t = titlebar->geometry();
    t.setTop( 2 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    QRegion r(t.x(), 0, t.width(), 18);
    r -= QRect(t.x()+((t.width()-fm.width(caption()))/2)-4,
               0, fm.width(caption())+8, 18);
    p.setClipRegion(r);
    int i, ly;
    for(i=0, ly=4; i < 4; ++i, ly+=3){
        p.setPen(options->color(Options::TitleBar, true).light(150));
        p.drawLine(0, ly, width()-1, ly);
        p.setPen(options->color(Options::TitleBar, true).dark(120));
        p.drawLine(0, ly+1, width()-1, ly+1);
    }
    p.setClipRect(t);
    p.setPen(options->color(Options::Font, true));
    p.setFont(options->font(true));

    p.drawText(t.x()+((t.width()-fm.width(caption()))/2)-4,
               0, fm.width(caption())+8, 18, AlignCenter, caption());
    p.setClipping(false);
    p.end();
    oldTitle = caption();
}

void SystemClient::captionChange( const QString &)
{
    recalcTitleBuffer();
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

    QBrush fillBrush(colorGroup().brush(QColorGroup::Background).pixmap() ?
                     colorGroup().brush(QColorGroup::Background) :
                     options->colorGroup(Options::Frame, isActive()).
                     brush(QColorGroup::Button));

    p.fillRect(1, 18, width()-2, height()-19, fillBrush);

    t.setTop( 2 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    if(isActive())
        p.drawPixmap(0, 0, titleBuffer);
    else{
        if(iUpperGradient)
            p.drawTiledPixmap(0, 0, width(), 18, *iUpperGradient);
        else
            p.fillRect(0, 0, width(), 18, fillBrush);
        p.setPen(options->color(Options::Font, isActive()));
        p.setFont(options->font(isActive()));
        p.drawText(t, AlignCenter, caption() );
    }



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
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}

void SystemClient::stickyChange(bool on)
{
    button[1]->setBitmap(on ? unsticky_bits : sticky_bits);
    button[1]->setTipText(on ? i18n("Un-Sticky") : i18n("Sticky"));
}

void SystemClient::maximizeChange(bool m)
{
    button[3]->setBitmap(m ? minmax_bits : maximize_bits);
    button[3]->setTipText(m ? i18n("Restore") : i18n("Maximize"));
}

void SystemClient::init()
{
    //
}

void SystemClient::activeChange(bool)
{
    repaint(false);
    button[0]->reset();
    button[1]->reset();
    button[2]->reset();
    button[3]->reset();
    if(button[4])
        button[4]->reset();
}

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
        return(new SystemClient(ws, w));
    }
    void init()
    {
       create_pixmaps();
    }
    void reset()
    {
       delete_pixmaps();
       create_pixmaps();
       // Ensure changes in tooltip state get applied
       Workspace::self()->slotResetAllClientsDelayed();
    }
    void deinit()
    {
       delete_pixmaps();
    }
}

#include "systemclient.moc"
