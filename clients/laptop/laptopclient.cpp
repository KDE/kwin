/*
 * Laptop KWin Decoration
 *
 * Port of this decoration to KDE 3.2, accessibility enhancement are
 * Copyright (c) 2003 Luciano Montanaro <mikelima@cirulla.net>
 */

#include <kconfig.h> // up here to avoid X11 header conflict :P
#include "laptopclient.h"
#include <qlayout.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <kglobal.h>
#include <kapplication.h>
#include <klocale.h>
#include <qbitmap.h>
#include <qtooltip.h>
#include <qlabel.h>

namespace Laptop {

static const unsigned char iconify_bits[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18};

static const unsigned char close_bits[] = {
    0x42, 0xe7, 0x7e, 0x3c, 0x3c, 0x7e, 0xe7, 0x42};

static const unsigned char maximize_bits[] = {
    0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0xff, 0xff };

static const unsigned char minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};

static const unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};

static const unsigned char unsticky_bits[] = {
   0x3c, 0x42, 0x99, 0xbd, 0xbd, 0x99, 0x42, 0x3c};

static const unsigned char sticky_bits[] = {
   0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c};

static QPixmap *titlePix;
static KPixmap *aUpperGradient;
static KPixmap *iUpperGradient;
// buttons active, inactive, up, down, and 2 sizes :P
static KPixmap *btnPix1;
static KPixmap *iBtnPix1;
static KPixmap *btnDownPix1;
static KPixmap *iBtnDownPix1;
static KPixmap *btnPix2;
static KPixmap *btnDownPix2;
static KPixmap *iBtnPix2;
static KPixmap *iBtnDownPix2;
static QColor btnForeground;

static int titleHeight = 14;
static int btnWidth1 = 17;
static int btnWidth2 = 27;

static int handleSize = 8;	// the resize handle size in pixels

static bool pixmaps_created = false;

// =====================================

extern "C" KDecorationFactory* create_factory()
{
    return new Laptop::LaptopClientFactory();
}

// =====================================

static inline const KDecorationOptions* options()
{
    return KDecoration::options();
}

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

    titleHeight = QFontMetrics(options()->font(true)).height() + 2;
    if (titleHeight < handleSize) titleHeight = handleSize;
    titleHeight &= ~1; // Make title height even
    if (titleHeight < 14) titleHeight = 14;

    btnWidth1 = titleHeight + 3;
    btnWidth2 = 3*titleHeight/2 + 6;

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
            p.setPen(options()->color(KDecoration::ColorTitleBar, true).light(150));
            p.drawPoint(x, y);
            maskPainter.drawPoint(x, y);
            p.setPen(options()->color(KDecoration::ColorTitleBar, true).dark(150));
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
        QColor bgColor = options()->color(KDecoration::ColorTitleBar, true);
        KPixmapEffect::gradient(*aUpperGradient,
                                bgColor.light(120),
                                bgColor.dark(120),
                                KPixmapEffect::VerticalGradient);
        bgColor = options()->color(KDecoration::ColorTitleBar, false);
        KPixmapEffect::gradient(*iUpperGradient,
                                bgColor.light(120),
                                bgColor.dark(120),
                                KPixmapEffect::VerticalGradient);
    }
    // buttons (active/inactive, sunken/unsunken, 2 sizes each)
    QColorGroup g = options()->colorGroup(KDecoration::ColorButtonBg, true);
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
        g = options()->colorGroup(KDecoration::ColorButtonBg, false);
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
        g = options()->colorGroup(KDecoration::ColorButtonBg, false);
        c = g.background();
        iBtnPix1->fill(c.rgb());
        iBtnDownPix1->fill(c.rgb());
        iBtnPix2->fill(c.rgb());
        iBtnDownPix2->fill(c.rgb());
    }
    g = options()->colorGroup(KDecoration::ColorButtonBg, true);
    c = g.background();
    drawButtonFrame(btnPix1, g, false);
    drawButtonFrame(btnDownPix1, g, true);
    drawButtonFrame(btnPix2, g, false);
    drawButtonFrame(btnDownPix2, g, true);
    g = options()->colorGroup(KDecoration::ColorButtonBg, false);
    c = g.background();
    drawButtonFrame(iBtnPix1, g, false);
    drawButtonFrame(iBtnDownPix1, g, true);
    drawButtonFrame(iBtnPix2, g, false);
    drawButtonFrame(iBtnDownPix2, g, true);

    if(qGray(options()->color(KDecoration::ColorButtonBg, true).rgb()) > 128)
        btnForeground = Qt::black;
    else
        btnForeground = Qt::white;
}

static void delete_pixmaps()
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
}

// =====================================

LaptopButton::LaptopButton(int w, int h, LaptopClient *parent,
        const char *name, const unsigned char *bitmap,
        const QString& tip, const int realizeBtns)
    : QButton(parent->widget(), name), client(parent)
{
    realizeButtons = realizeBtns;

    setCursor( arrowCursor );
    defaultSize = QSize(w, h);
    setFixedHeight(h);
    resize(defaultSize);
    if(bitmap)
        setBitmap(bitmap);

    //setBackgroundMode(QWidget::NoBackground);

    QToolTip::add(this, tip);
}

QSize LaptopButton::sizeHint() const
{
    return(defaultSize);
}

void LaptopButton::reset()
{
    repaint(false);
}

void LaptopButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void LaptopButton::drawButton(QPainter *p)
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
        QColorGroup g = options()->colorGroup(KDecoration::ColorButtonBg,
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

// =====================================

void LaptopClient::reset(unsigned long)
{
    for (int i = 0; i < 5; ++i) {
        if (button[i])
            button[i]->reset();
    }
    widget()->repaint();
}

LaptopClient::LaptopClient(KDecorationBridge *b, KDecorationFactory *f)
    : KDecoration(b, f)
{
}

void LaptopClient::init()
{
    createMainWidget(WResizeNoErase | WStaticContents);
    widget()->installEventFilter(this);

    lastButtonWidth = 0;
    lastBufferWidth = 0;

    // XXX Check how to do this...
    // connect(options(), SIGNAL(resetClients()), this, SLOT(slotReset()));
    bool help = providesContextHelp();

    g = new QGridLayout(widget(), 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);
    g->addRowSpacing(2, 1);
    if (isPreview())
	g->addWidget(new QLabel(i18n("<center><b></b>Laptop preview</center>"),
		    widget()), 3, 1);
    else
	g->addItem( new QSpacerItem( 0, 0 ), 3, 1); // no widget in the middle

    g->setRowStretch(3, 10);
    spacer = new QSpacerItem(10, isResizable() ? handleSize : 4,
			QSizePolicy::Expanding, QSizePolicy::Minimum);
    g->addItem(spacer, 4, 1);
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    int th = titleHeight;
    if ( isTool() )
	th -= 2;

    button[BtnClose] = new LaptopButton(btnWidth2, th, this, "close",
                                 close_bits, i18n("Close"));
    button[BtnSticky] = new LaptopButton(btnWidth1, th, this, "sticky",
                                 NULL, isOnAllDesktops()?i18n("Not on all desktops"):i18n("On all desktops"));
    if(isOnAllDesktops())
        button[BtnSticky]->setBitmap(unsticky_bits);
    else
        button[BtnSticky]->setBitmap(sticky_bits);
    button[BtnIconify] = new LaptopButton(btnWidth2, th, this, "iconify",
                                          iconify_bits, i18n("Minimize"));
    button[BtnMax] = new LaptopButton(btnWidth2, th, this, "maximize",
                                      maximize_bits, i18n("Maximize"), LeftButton|MidButton|RightButton);
    if (help) {
        button[BtnHelp] = new LaptopButton(btnWidth1, th, this, "help",
                                     question_bits, i18n("Help"));
        connect(button[BtnHelp], SIGNAL( clicked() ), this, SLOT( showContextHelp() ) );
    }
    else
        button[BtnHelp] = NULL;

    connect( button[BtnClose], SIGNAL( clicked() ), this, SLOT( closeWindow() ) );
    connect( button[BtnSticky], SIGNAL( clicked() ), this, SLOT( toggleOnAllDesktops() ) );
    connect( button[BtnIconify], SIGNAL( clicked() ), this, SLOT( minimize() ) );
    connect( button[BtnMax], SIGNAL( clicked() ), this, SLOT( slotMaximize() ) );

    hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0);
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 1, 1 );
    hb->addWidget( button[BtnClose]);
    hb->addSpacing(1);
    titlebar = new QSpacerItem(10, th, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(1);
    if (help) {
        hb->addWidget(button[BtnHelp]);
    }
    hb->addWidget( button[BtnSticky]);
    hb->addWidget( button[BtnIconify]);
    hb->addWidget( button[BtnMax]);

    if ( isTransient() || isTool() )
	button[BtnSticky]->hide();
    if ( !isMinimizable() )
	button[BtnIconify]->hide();
    if ( !isMaximizable() )
	button[BtnMax]->hide();
    if ( !isCloseable() )
        button[BtnClose]->hide();

    hiddenItems = false;
    bufferDirty = true;
}

void LaptopClient::slotMaximize()
{
    maximize(button[BtnMax]->last_button);
}

void LaptopClient::resizeEvent(QResizeEvent* e)
{
    doShape();
    calcHiddenButtons();
    if ( widget()->isVisibleToTLW() ) {
	int dx = 0;
	int dy = 0;
	if ( e->oldSize().width() != width() )
	    dx = 32 + QABS( e->oldSize().width() -  width() );
	if ( e->oldSize().height() != height() )
	    dy = isResizable() ? handleSize : 4 +
		QABS( e->oldSize().height() -  height() );
	if ( dy )
	    widget()->update( 0, height() - dy + 1, width(), dy );
	if ( dx ) {
	    widget()->update( width() - dx + 1, 0, dx, height() );
	    widget()->update( QRect( QPoint(4,4),
			titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	    widget()->update( QRect( titlebar->geometry().topRight(),
			QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
	    widget()->update(titlebar->geometry());
	}
    }
}

void LaptopClient::captionChange()
{
    bufferDirty = true;
    widget()->repaint(titlebar->geometry(), false);
}

void LaptopClient::paintEvent( QPaintEvent* )
{
    QPainter p(widget());
    QColorGroup g = options()->colorGroup(KDecoration::ColorFrame, isActive());

    QRect r(widget()->rect());
    p.setPen(Qt::black);
    p.drawRect(r);
    // outer frame
    p.setPen(g.light());
    p.drawLine(r.x()+1, r.y()+1, r.right()-1, r.y()+1);
    p.drawLine(r.x()+1, r.y()+1, r.x()+1, r.bottom()-1);
    p.setPen(g.dark());
    p.drawLine(r.right()-1, r.y()+1, r.right()-1, r.bottom()-1);
    p.drawLine(r.x()+1, r.bottom()-1, r.right()-1, r.bottom()-1);

    int th = titleHeight;
    int bb = handleSize + 2; // Bottom border
    int bs = handleSize - 2; // inner size of bottom border
    if (!isResizable()) {
	bb = 6;
	bs = 0;
    }
    if ( isTool() )
	th -= 2;

    // inner rect
    p.drawRect(r.x() + 3, r.y() + th + 3, r.width() - 6, r.height() - th - bb);

    // handles
    if (!isResizable()) {
    } else if (r.width() > 3*handleSize + 20) {
        int range = 8 + 3*handleSize/2;
        qDrawShadePanel(&p, r.x() + 1, r.bottom() - bs, range,
                        handleSize - 2, g, false, 1, &g.brush(QColorGroup::Mid));
        qDrawShadePanel(&p, r.x() + range + 1, r.bottom() - bs,
		r.width() - 2*range - 2, handleSize - 2, g, false, 1,
		isActive() ? &g.brush(QColorGroup::Background) :
			     &g.brush(QColorGroup::Mid));
        qDrawShadePanel(&p, r.right() - range, r.bottom() - bs,
		range, bs, g, false, 1, &g.brush(QColorGroup::Mid));
    }
    else
        qDrawShadePanel(&p, r.x() + 1, r.bottom() - bs,
		r.width() - 2, bs, g, false, 1,
		isActive() ?  &g.brush(QColorGroup::Background) :
		              &g.brush(QColorGroup::Mid));

    r = titlebar->geometry();
    r.setRight(r.right()-1);

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
                       options()->color(KDecoration::ColorTitleBar, false));

        p.setFont(options()->font(false, isTool() ));
        QFontMetrics fm(options()->font(false));
        g = options()->colorGroup(KDecoration::ColorTitleBar, false);
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
        p.setPen(options()->color(KDecoration::ColorFont, false));
        p.drawText(r.x(), r.y(), r.width(), r.height()-1,
                   AlignCenter, caption() );
        g = options()->colorGroup(KDecoration::ColorFrame, true);
        p.setPen(g.background());
        p.drawPoint(r.x(), r.y());
        p.drawPoint(r.right(), r.y());
        p.drawLine(r.right()+1, r.y(), r.right()+1, r.bottom());
    }
}

#define QCOORDARRLEN(x) sizeof(x)/(sizeof(QCOORD)*2)

void LaptopClient::doShape()
{
    QRegion mask(QRect(0, 0, width(), height()));
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-1, 0, 1, 1);
    mask -= QRect(0, height()-1, 1, 1);
    mask -= QRect(width()-1, height()-1, 1, 1);

    setMask(mask);
}

void LaptopClient::showEvent(QShowEvent *)
{
    doShape();
    widget()->repaint();
}

void LaptopClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        titlebarDblClickOperation();
}

void LaptopClient::iconChange()
{
    // There is no icon support in this theme
}

void LaptopClient::desktopChange()
{
    bool on = isOnAllDesktops();
    button[BtnSticky]->setBitmap(on ? unsticky_bits : sticky_bits);
    QToolTip::remove(button[BtnSticky]);
    QToolTip::add(button[BtnSticky],
	    on ? i18n("Not on all desktops") : i18n("On all desktops"));
}

void LaptopClient::maximizeChange()
{
    bool m = (maximizeMode() == MaximizeFull);
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
    QToolTip::remove(button[BtnMax]);
    QToolTip::add(button[BtnMax], m ? i18n("Restore") : i18n("Maximize"));
    spacer->changeSize(10, isResizable() ? handleSize : 4,
	    QSizePolicy::Expanding, QSizePolicy::Minimum);
    g->activate();
    doShape();
    widget()->repaint(false);
}

void LaptopClient::activeChange()
{
    widget()->repaint(false);
    int i;
    for(i=0; i < 5; ++i){
        if(button[i])
            button[i]->reset();
    }
}


void LaptopClient::calcHiddenButtons()
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
                    if(button[i]->isHidden() &&
		       ( !isTransient() || i != BtnSticky ) &&
		       ( isMinimizable() || i != BtnIconify ) &&
		       ( isMaximizable() || ( i != BtnIconify && i != BtnSticky && i != BtnMax ) )

		       ) {
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

void LaptopClient::updateActiveBuffer( )
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
    QRect r(0, 0, activeBuffer.width()-1, activeBuffer.height());
    p.begin(&activeBuffer);
    if(aUpperGradient){
        p.drawTiledPixmap(r, *aUpperGradient);
    }
    else{
        p.fillRect(r, options()->color(KDecoration::ColorTitleBar, true));
    }
    if(titlePix)
        p.drawTiledPixmap(r, *titlePix);

    p.setFont(options()->font(true, isTool() ));
    QFontMetrics fm(options()->font(true));
    QColorGroup g = options()->colorGroup(KDecoration::ColorTitleBar, true);
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
    p.setPen(options()->color(KDecoration::ColorFont, true));
    p.drawText(r.x(), r.y(), r.width(), r.height()-1,
               AlignCenter, caption() );
    g = options()->colorGroup(KDecoration::ColorFrame, true);
    p.setPen(g.background());
    p.drawPoint(r.x(), r.y());
    p.drawPoint(r.right(), r.y());
    p.drawLine(r.right()+1, r.y(), r.right()+1, r.bottom());
    p.end();
}

LaptopClient::Position LaptopClient::mousePosition(const QPoint & p) const
{
  Position m = PositionCenter;
  int range = 8 + 3*handleSize/2;

  if (p.y() < (height() - handleSize + 1))
    m = KDecoration::mousePosition(p);

  else {
    if (p.x() >= (width() - range))
      m = PositionBottomRight;
    else if (p.x() <= range)
      m = PositionBottomLeft;
    else
      m = PositionBottom;
  }

  return m;
}

void LaptopClient::borders(int &left, int &right, int &top, int &bottom) const
{
    left = right = 4;
    top = titleHeight + 4;
    bottom = isResizable() ? handleSize : 4;
}

void LaptopClient::shadeChange()
{
}

QSize LaptopClient::minimumSize() const
{
    return QSize(4 * handleSize, handleSize);
}

void LaptopClient::resize(const QSize& s)
{
    widget()->resize(s);
    widget()->repaint(); //there is some strange wrong repaint of the frame without
}

static const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask |
    NET::DesktopMask | NET::DockMask | NET::ToolbarMask | NET::MenuMask |
    NET::DialogMask | NET::OverrideMask | NET::TopMenuMask |
    NET::UtilityMask | NET::SplashMask;

bool LaptopClient::isTransient() const
{
    NET::WindowType type = windowType(SUPPORTED_WINDOW_TYPES_MASK);
    return type == NET::Dialog;
}

bool LaptopClient::isTool() const
{
    NET::WindowType type = windowType(SUPPORTED_WINDOW_TYPES_MASK);
    return type == NET::Toolbar || type == NET::Utility || type == NET::Menu;
}

bool LaptopClient::eventFilter(QObject *o, QEvent *e)
{
    if (o != widget())
	return false;
    switch (e->type()) {
    case QEvent::Resize:
	resizeEvent(static_cast< QResizeEvent* >( e ));
	return true;
    case QEvent::Paint:
	paintEvent(static_cast< QPaintEvent* >( e ));
	return true;
    case QEvent::MouseButtonDblClick:
	mouseDoubleClickEvent(static_cast< QMouseEvent* >( e ));
	return true;
    case QEvent::MouseButtonPress:
	processMousePressEvent(static_cast< QMouseEvent* >( e ));
	return true;
    case QEvent::Show:
	showEvent(static_cast< QShowEvent* >( e ));
	return true;
    default:
	break;
    }
    return false;
}

// =====================================

LaptopClientFactory::LaptopClientFactory()
{
    create_pixmaps();
}

LaptopClientFactory::~LaptopClientFactory()
{
    delete_pixmaps();
}

KDecoration *LaptopClientFactory::createDecoration(KDecorationBridge *b)
{
    findPreferredHandleSize();
    return new Laptop::LaptopClient(b, this);
}

bool LaptopClientFactory::reset(unsigned long /*changed*/)
{
    findPreferredHandleSize();

    // TODO Do not recreate decorations if it is not needed. Look at
    // ModernSystem for how to do that
    Laptop::delete_pixmaps();
    Laptop::create_pixmaps();
    // For now just return true.
    return true;
}

QValueList< LaptopClientFactory::BorderSize >
LaptopClientFactory::borderSizes() const
{
    // the list must be sorted
    return QValueList< BorderSize >() << BorderNormal << BorderLarge <<
	BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

void LaptopClientFactory::findPreferredHandleSize()
{
    switch (options()->preferredBorderSize(this)) {
    case KDecoration::BorderLarge:
	handleSize = 11;
	break;
    case KDecoration::BorderVeryLarge:
	handleSize = 16;
	break;
    case KDecoration::BorderHuge:
	handleSize = 24;
	break;
    case KDecoration::BorderVeryHuge:
	handleSize = 32;
	break;
    case KDecoration::BorderOversized:
	handleSize = 40;
	break;
    case KDecoration::BorderTiny:
    case KDecoration::BorderNormal:
    default:
	handleSize = 8;
    }
}

} // Laptop namespace

#include "laptopclient.moc"

// vim: sw=4
