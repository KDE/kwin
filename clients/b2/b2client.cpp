#include "b2client.h"
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
#include "bitmaps.h"

extern "C"
{
    Client *allocate(Workspace *ws, WId w)
    {
        return(new B2Client(ws, w));
    }
}

// TODO: stick all these in an array
static KPixmap *aClosePix=0, *aClosePixDown;
static KPixmap *iClosePix, *iClosePixDown;
static KPixmap *aMaxPix, *aMaxPixDown;
static KPixmap *iMaxPix, *iMaxPixDown;
static KPixmap *aNormalizePix, *aNormalizePixDown;
static KPixmap *iNormalizePix, *iNormalizePixDown;
static KPixmap *aIconifyPix, *aIconifyPixDown;
static KPixmap *iIconifyPix, *iIconifyPixDown;
static KPixmap *aPinupPix, *aPinupPixDown;
static KPixmap *iPinupPix, *iPinupPixDown;
static KPixmap *aMenuPix, *aMenuPixDown;
static KPixmap *iMenuPix, *iMenuPixDown;
static KPixmap *aHelpPix, *aHelpPixDown;
static KPixmap *iHelpPix, *iHelpPixDown;

static bool pixmaps_created = false;

static void drawB2Rect(KPixmap *pix, const QColor &primary, bool down)
{
    QPainter p;
    QColor hColor = primary.light(150);
    QColor lColor = primary.dark(150);

    if(QPixmap::defaultDepth() > 8){
        if(down)
            KPixmapEffect::gradient(*pix, lColor, hColor,
                                    KPixmapEffect::DiagonalGradient);
        else
            KPixmapEffect::gradient(*pix, hColor, lColor,
                                    KPixmapEffect::DiagonalGradient);
    }
    else
        pix->fill(primary);
    int x2 = pix->width()-1;
    int y2 = pix->height()-1;
    p.begin(pix);
    p.setPen(down ? hColor : lColor);
    p.drawLine(0, 0, x2, 0);
    p.drawLine(0, 0, 0, y2);
    p.drawLine(1, x2-1, y2-1, x2-1);
    p.drawLine(x2-1, 1, x2-1, y2-1);
    p.setPen(down ? lColor : hColor);
    p.drawRect(1, 1, x2, y2);
    p.end();

}

static void create_pixmaps();
static void redraw_pixmaps();

QPixmap* kwin_get_menu_pix_hack()
{
    create_pixmaps();
    //return menu_pix;  FIXME
    return aMenuPix;
}

static void create_pixmaps()
{
    if ( pixmaps_created )
        return;
    pixmaps_created = true;

    // TODO: Stick all these in an array
    aClosePix = new KPixmap;
    aClosePix->resize(16, 16);
    aClosePixDown = new KPixmap;
    aClosePixDown->resize(16, 16);

    iClosePix = new KPixmap;
    iClosePix->resize(16, 16);
    iClosePixDown = new KPixmap;
    iClosePixDown->resize(16, 16);

    aMaxPix = new KPixmap;
    aMaxPixDown = new KPixmap;
    iMaxPix = new KPixmap;
    iMaxPixDown = new KPixmap;

    aNormalizePix = new KPixmap();
    aNormalizePix->resize(16, 16);
    aNormalizePixDown = new KPixmap();
    aNormalizePixDown->resize(16, 16);

    aIconifyPix = new KPixmap;
    aIconifyPix->resize(10, 10);
    aIconifyPixDown = new KPixmap;
    aIconifyPixDown->resize(10, 10);

    iNormalizePix = new KPixmap();
    iNormalizePix->resize(16, 16);
    iNormalizePixDown = new KPixmap();
    iNormalizePixDown->resize(16, 16);

    iIconifyPix = new KPixmap;
    iIconifyPix->resize(10, 10);
    iIconifyPixDown = new KPixmap;
    iIconifyPixDown->resize(10, 10);

    aPinupPix = new KPixmap;
    aPinupPix->resize(16, 16);
    aPinupPixDown = new KPixmap;
    aPinupPixDown->resize(16, 16);

    iPinupPix = new KPixmap;
    iPinupPix->resize(16, 16);
    iPinupPixDown = new KPixmap;
    iPinupPixDown->resize(16, 16);

    aMenuPix = new KPixmap;
    aMenuPix->resize(16, 16);
    aMenuPixDown = new KPixmap;
    aMenuPixDown->resize(16, 16);

    iMenuPix = new KPixmap;
    iMenuPix->resize(16, 16);
    iMenuPixDown = new KPixmap;
    iMenuPixDown->resize(16, 16);

    aHelpPix = new KPixmap;
    aHelpPix->resize(16, 16);
    aHelpPixDown = new KPixmap;
    aHelpPixDown->resize(16, 16);
    iHelpPix = new KPixmap;
    iHelpPix->resize(16, 16);
    iHelpPixDown = new KPixmap;
    iHelpPixDown->resize(16, 16);

    // there seems to be no way to load X bitmaps from data properly, so
    // we need to create new ones for each mask :P
    QBitmap pinupMask(16, 16, pinup_mask_bits, true);
    aPinupPix->setMask(pinupMask);
    iPinupPix->setMask(pinupMask);
    QBitmap pindownMask(16, 16, pindown_mask_bits, true);
    aPinupPixDown->setMask(pindownMask);
    iPinupPixDown->setMask(pindownMask);

    QBitmap menuMask(16, 16, menu_mask_bits, true);
    aMenuPix->setMask(menuMask);
    iMenuPix->setMask(menuMask);
    aMenuPixDown->setMask(menuMask);
    iMenuPixDown->setMask(menuMask);
    
    QBitmap helpMask(16, 16, help_mask_bits, true);
    aHelpPix->setMask(helpMask);
    iHelpPix->setMask(helpMask);
    aHelpPixDown->setMask(helpMask);
    iHelpPixDown->setMask(helpMask);
    redraw_pixmaps();
}

B2Button::B2Button(KPixmap *pix, KPixmap *pixDown, KPixmap *iPix,
                   KPixmap *iPixDown, Client *_client, QWidget *parent,
		   const char *name)
    : QButton(parent, name)
{
    client = _client;
    pNorm = pix;
    pDown = pixDown;
    iNorm = iPix;
    iDown = iPixDown;
    setFixedSize(16, 16);
    setFocusPolicy(NoFocus);
    resize(16, 16);
}

QSize B2Button::sizeHint() const
{
    return(QSize(16, 16));
}

QSizePolicy B2Button::sizePolicy() const
{
    return(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
}


void B2Button::drawButton(QPainter *p)
{
    p->fillRect(rect(), bg);
    if(useMiniIcon && !client->miniIcon().isNull()){
        QPixmap miniIcon = client->miniIcon();
        p->drawPixmap((width()-miniIcon.width())/2,
                      (height()-miniIcon.height())/2, miniIcon);
    }
    else{
        if(client->isActive()){
            if(isOn() || isDown())
                p->drawPixmap((width()-pDown->width())/2,
                              (height()-pDown->height())/2, *pDown);
            else
                p->drawPixmap((width()-pNorm->width())/2,
                              (height()-pNorm->height())/2, *pNorm);
        }
        else{
            if(isOn() || isDown())
                p->drawPixmap((width()-pDown->width())/2,
                              (height()-pDown->height())/2, *iDown);
            else
                p->drawPixmap((width()-pNorm->width())/2,
                              (height()-pNorm->height())/2, *iNorm);
        }
    }
}

void B2Button::setPixmaps(KPixmap *pix, KPixmap *pixDown, KPixmap *iPix,
                          KPixmap *iPixDown)
{
    pNorm = pix;
    pDown = pixDown;
    iNorm = iPix;
    iDown = iPixDown;
    repaint(false);
}

B2Titlebar::B2Titlebar(B2Client *parent)
    : QWidget(parent)
{
    set_x11mask = false;
    isfullyobscured = false;
    shift_move = false;
    client = parent;
}

bool B2Titlebar::x11Event(XEvent *e)
{
    if (!set_x11mask) {
	set_x11mask = true;
	XSelectInput(qt_xdisplay(), winId(),
	    KeyPressMask | KeyReleaseMask |
	    ButtonPressMask | ButtonReleaseMask |
	    KeymapStateMask |
	    ButtonMotionMask |
	    EnterWindowMask | LeaveWindowMask |
	    FocusChangeMask |
	    ExposureMask |
	    PropertyChangeMask |
	    StructureNotifyMask | SubstructureRedirectMask |
	    VisibilityChangeMask);
    }
    switch( e->type ) {
    case VisibilityNotify:
	isfullyobscured = false;
	if (e->xvisibility.state == VisibilityFullyObscured) {
	    isfullyobscured = true;
	    client->unobscureTitlebar();
	}
	break;
    default:
	break;
    }
    return QWidget::x11Event(e);
}

void B2Titlebar::paintEvent(QPaintEvent * /*e*/)
{
    QPainter p(this);
    QRect t=rect();
    bool active = client->isActive();

    // black titlebar frame
    p.setPen(Qt::black);
    p.drawLine(0, 0, 0, t.bottom() );
    p.drawLine(0, 0, t.right(), 0);
    p.drawLine(t.right(), 0, t.right(), t.bottom());

    // titlebar fill
    qDrawShadeRect(&p, 1, 1, t.right()-1, t.height()-1,
                   options->colorGroup(Options::TitleBar, active),
                   false, 1, 0,
                   &options->colorGroup(Options::TitleBar, active).
                   brush(QColorGroup::Button));

    // and the caption
    p.setPen(options->color(Options::Font, active));
    p.setFont(options->font(active));

    t.setX(client->providesContextHelp() ?
           client->button[B2Client::BtnHelp]->x()+17 :
           client->button[B2Client::BtnSticky]->x()+17);
    t.setRight(client->button[B2Client::BtnIconify]->x()-1);
    p.drawText(t, AlignLeft | AlignVCenter, client->caption());
}

void B2Titlebar::mouseDoubleClickEvent( QMouseEvent * )
{
    client->setShade( !client->isShade() );
    client->workspace()->requestFocus( client );
}

void B2Titlebar::mousePressEvent( QMouseEvent * e )
{
    shift_move = e->state() & ShiftButton;
    if (shift_move ) {
        moveOffset = e->globalPos();
    }
    QMouseEvent _e(QEvent::MouseButtonPress, mapToParent(e->pos()),
        e->globalPos(), e->button(), e->state());
    //QWidget::mousePressEvent( e );
    client->mousePressEvent( &_e);
}

void B2Titlebar::mouseReleaseEvent( QMouseEvent * e )
{
    shift_move = false;
    QMouseEvent _e(QEvent::MouseButtonRelease, mapToParent(e->pos()),
        e->globalPos(), e->button(), e->state());
    //QWidget::mouseReleaseEvent( e );
    client->mouseReleaseEvent( &_e);
}

void B2Titlebar::mouseMoveEvent( QMouseEvent * e )
{
    if (shift_move) {
	int oldx = mapFromGlobal(moveOffset).x();
        int xdiff = e->globalPos().x() - moveOffset.x();
        moveOffset = e->globalPos();
	if (oldx >= 0 && oldx <= rect().right()) {
            client->titleMoveRel(xdiff);
	}
    } else {
	QMouseEvent _e(QEvent::MouseMove, mapToParent(e->pos()),
	    e->globalPos(), e->button(), e->state());
	client->mouseMoveEvent( &_e);
    }
}

B2Client::B2Client( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    bar_x_ofs = 0;
    in_unobs = 0;

    create_pixmaps();
    g = new QGridLayout( this, 0, 0);
    g->addMultiCellWidget(windowWrapper(), 1, 1, 1, 2);

    g->addColSpacing(0, 4);

    g->addColSpacing(1, providesContextHelp() ? 102 : 85);

    g->setColStretch(2, 1);

    g->setRowStretch(1, 1);
    g->addColSpacing(3, 4);
    g->addRowSpacing(2, 8);

    // titlebar
    g->addRowSpacing(0, 20);

    titlebar = new B2Titlebar(this);
    int i;
    for(i=0; i < 6; ++i){
        button[i] = new B2Button(this, titlebar/*this*/);
        button[i]->setFixedSize(16, 16);
    }

    button[BtnSticky]->setToggle();
    button[BtnSticky]->setDown(isSticky());
    button[BtnMenu]->setUseMiniIcon();
    
    if(!providesContextHelp())
        button[5]->hide();

    button[BtnMenu]->setPixmaps(aMenuPix, aMenuPixDown, iMenuPix,
                                iMenuPixDown);
    button[BtnSticky]->setPixmaps(aPinupPix, aPinupPixDown, iPinupPix,
                                  iPinupPixDown);
    button[BtnIconify]->setPixmaps(aIconifyPix, aIconifyPixDown,
                                   iIconifyPix, iIconifyPixDown);
    button[BtnClose]->setPixmaps(aClosePix, aClosePixDown, iClosePix,
                                 iClosePixDown);
    button[BtnHelp]->setPixmaps(aHelpPix, aHelpPixDown, iHelpPix,
                                iHelpPixDown);

    if(isMaximized())
        button[BtnMax]->setPixmaps(aNormalizePix, aNormalizePixDown,
                                   iNormalizePix, iNormalizePixDown);
    else
        button[BtnMax]->setPixmaps(aMaxPix, aMaxPixDown, iMaxPix,
                                   iMaxPixDown);

    QColor c = options->colorGroup(Options::TitleBar, isActive()).
        color(QColorGroup::Button);
    titlebar->setBackgroundColor(c);
    for(i=0; i < 6; ++i)
        button[i]->setBg(c);

    positionButtons();
    connect(button[BtnMenu], SIGNAL(clicked()), this, SLOT(menuButtonPressed()));
    connect(button[BtnSticky], SIGNAL(clicked()), this, SLOT(toggleSticky()));
    connect(button[BtnIconify], SIGNAL(clicked()), this, SLOT(iconify()));
    connect(button[BtnMax], SIGNAL(clicked()), this, SLOT(maximize()));
    connect(button[BtnClose], SIGNAL(clicked()), this, SLOT(closeWindow()));
    connect(button[BtnHelp], SIGNAL(clicked()), this, SLOT(contextHelp()));

    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
}

void B2Client::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
    /* may be the resize cuted off some space occupied by titlebar, which
       was moved, so instead of reducing it, we first try to move it */
    titleMoveAbs(bar_x_ofs);
    positionButtons();
    doShape();
    /*
    What does this? (MM)
    if ( isVisibleToTLW() && !testWFlags( WNorthWestGravity )) {
        QPainter p( this );
        QRect t = titlebar->geometry();
	QRegion r = rect();
	r = r.subtract( t );
	p.setClipRegion( r );
	p.eraseRect( rect() );
    }
    */
    repaint(); //there is some strange wrong repaint of the frame without
}

void B2Client::captionChange( const QString &)
{
    positionButtons();
    doShape();
    //repaint();
    titlebar->repaint();
}

void B2Client::paintEvent( QPaintEvent* e)
{
    QPainter p( this );

    QRect t = titlebar->geometry();

    // inner window rect
    p.drawRect(3, t.bottom(), width()-6, height()-t.height()-6);

    // outer frame rect
    p.drawRect(0, t.bottom()-3, width(), height()-t.height());

    // frame shade panel
    qDrawShadePanel(&p, 1, t.bottom()-2, width()-2, height()-t.height()-2,
                    options->colorGroup(Options::Frame, isActive()),
                    false);

    //bottom handle rect
    int hx = width()-40;
    int hw = 40;

    //p.drawRect(hx, height()-8, hw, 8);
    p.drawLine(width()-1, height()-8, width()-1, height()-1);
    p.drawLine(hx, height()-1, width()-1, height()-1);
    p.drawLine(hx, height()-4, hx, height()-1);

    p.fillRect(hx+1, height()-7, hw-2, 6,
               options->colorGroup(Options::Frame, isActive())
               .brush(QColorGroup::Button));

    p.setPen(options->colorGroup(Options::Frame, isActive()).dark());
    p.drawLine(width()-2, height()-8, width()-2, height()-2);
    p.drawLine(hx+1, height()-2, width()-2, height()-2);
    p.setPen(options->colorGroup(Options::Frame, isActive()).light());
    p.drawLine(hx+1, height()-6, hx+1, height()-3);
    p.drawLine(hx+1, height()-7, width()-3, height()-7);

    /* OK, we got a paint event, which means parts of us are now visible
       which were not before. We try the titlebar if it is currently fully
       obscured, and if yes, try to unobscure it, in the hope that some
       of the parts which we just painted were in the titlebar area.
       It can happen, that the titlebar, as it got the FullyObscured event
       had no chance of becoming partly visible. The problem is, that
       we now might have the space available, but the titlebar gets no
       visibilitinotify events until its state changes, so we just try
     */
    if (titlebar->isFullyObscured()) {
        /* We first see, if our repaint contained the titlebar area */
	QRegion reg(QRect(0,0,width(),20));
	reg = reg.intersect(e->region());
	if (!reg.isEmpty())
	    unobscureTitlebar();
    }
}

#define QCOORDARRLEN(x) sizeof(x)/(sizeof(QCOORD)*2)

void B2Client::doShape()
{
    QRect t = titlebar->geometry();
    QRegion mask(rect());
    // top to the tilebar right
    if (bar_x_ofs) {
        mask -= QRect(0, 0, bar_x_ofs, t.height()-4); //left from bar
	mask -= QRect(0, t.height()-4, 1, 1);         //top left point
    }
    if (t.right() < width()-1) {
        mask -= QRect(width()-1, t.height()-4, 1, 1); // top right point 
        mask -= QRect(t.right()+1, 0, width()-t.right()-1, t.height()-4);
    }
    mask -= QRect(width()-1, height()-1, 1, 1); // bottom right point
    mask -= QRect(0, height()-5, 1, 1); // bottom left point
    mask -= QRect(width()-1, height()-1, 1, 1); // bottom right point
    mask -= QRect(width()-40, height()-1, 1, 1); // handle left point
    mask -= QRect(0, height()-4, width()-40, 4); // bottom left
    
    setMask(mask);
}

void B2Client::showEvent(QShowEvent *ev)
{
    Client::showEvent(ev);
    doShape();
    repaint();
    titlebar->repaint();
}

void B2Client::windowWrapperShowEvent( QShowEvent* )
{
    doShape();
}                                                                               

Client::MousePosition B2Client::mousePosition( const QPoint& p ) const
{
    const int range = 16;
    const int border = 4;
    /*QRect t = g->cellGeometry(0, 1);
    t.setRight(button[BtnClose]->x()+17);
    */
    QRect t = titlebar->geometry();
    t.setHeight(20-border); 
    int ly = t.bottom();
    int lx = t.right();

    if ( p.x() > t.right() ) {
        if ( p.y() <= ly + range && p.x() >= width()-range)
            return TopRight;
        else if ( p.y() <= ly + border )
            return Top;
    } else if ( p.x() < bar_x_ofs ) {
        if ( p.y() <= ly + range && p.x() <= range )
            return TopLeft;
        else if ( p.y() <= ly+border )
            return Top;
    } else if ( p.y() < ly ) {
        if ( p.x() > bar_x_ofs+border && p.x() < lx-border && p.y() > border )
            return Client::mousePosition( p );
        if ( p.x() > bar_x_ofs+range && p.x() < lx - range)
            return Top;
        if ( p.y() <= range ) {
            if ( p.x() <= bar_x_ofs+range )
                return TopLeft;
            else return TopRight;
        } else {
            if ( p.x() <= bar_x_ofs+range )
                return Left;
            else return Right;
        }
    }
    
    if (p.y() >= height() - 8) {
        /* the normal Client:: only wants border of 4 pixels */
	if (p.x() <= range) return BottomLeft;
	if (p.x() >= width()-range) return BottomRight;
	return Bottom;
    }
 
    return Client::mousePosition( p );
}


void B2Client::titleMoveAbs(int new_ofs)
{
    if (new_ofs < 0) new_ofs = 0;
    if (new_ofs + titlebar->width() > width()) {
        new_ofs = width() - titlebar->width();
    }
    if (bar_x_ofs != new_ofs) {
        bar_x_ofs = new_ofs;
	positionButtons();
	doShape();
	repaint( 0, 0, width(), 20, false );
	titlebar->repaint(false);
    }
}

void B2Client::titleMoveRel(int xdiff)
{
    titleMoveAbs(bar_x_ofs + xdiff);
}

void B2Client::stickyChange(bool on)
{
    button[1]->setDown(on);
}

void B2Client::maximizeChange(bool m)
{
    if(m){
        button[BtnMax]->setPixmaps(aNormalizePix, aNormalizePixDown,
                                   iNormalizePix, iNormalizePixDown);
    }
    else{
        button[BtnMax]->setPixmaps(aMaxPix, aMaxPixDown, iMaxPix,
                                   iMaxPixDown);
    }
    button[BtnMax]->repaint();
}


void B2Client::activeChange(bool on)
{
    int i;
    repaint(false);
    titlebar->repaint(false);
    QColor c = options->colorGroup(Options::TitleBar, on).
        color(QColorGroup::Button);
    for(i=0; i < 6; ++i){
        button[i]->setBg(c);
        button[i]->repaint();
    }
}

void B2Client::init()
{
    //
}

void B2Client::menuButtonPressed()
{
    workspace()->clientPopup(this)->
        popup(button[BtnMenu]->mapToGlobal(button[BtnMenu]->
                                           rect().bottomLeft()));
}

void B2Client::slotReset()
{
    redraw_pixmaps();
    QColor c = options->colorGroup(Options::TitleBar, isActive()).
        color(QColorGroup::Button);
    int i;
    for(i=0; i < 6; ++i){
        button[i]->setBg(c);
        button[i]->repaint(false);
    }
    repaint();
    titlebar->repaint();
}

void B2Client::unobscureTitlebar()
{
    /* we just noticed, that we got obscured by other windows
       so we look at all windows above us (stacking_order) merging their
       masks, intersecting it with our titlebar area, and see if we can
       find a place not covered by any window */
    if (in_unobs) {
	return;
    }
    in_unobs = 1;
    QRegion reg(QRect(0,0,width(),20));
    ClientList::ConstIterator it = workspace()->stackingOrder().find(this);
    ++it;
    while (it != workspace()->stackingOrder().end()) {
        /* the clients all have their mask-regions in local coords
	   so we have to translate them to a shared coord system
	   we choose ours */
	int dx = (*it)->x() - x();
	int dy = (*it)->y() - y();
	QRegion creg = (*it)->getMask();
	creg.translate(dx, dy);
	reg -= creg;
	if (reg.isEmpty()) {
	    // early out, we are completely obscured
	    break;
	}
	++it;
    }
    if (!reg.isEmpty()) {
        // there is at least _one_ pixel from our title area, which is not
	// obscured, we use the first rect we find
	// for a first test, we use boundingRect(), later we may refine
	// to rect(), and search for the nearest, or biggest, or smthg.
	titleMoveAbs(reg.boundingRect().x());
    }
    in_unobs = 0;
}

static void redraw_pixmaps()
{
    QColorGroup aGrp = options->colorGroup(Options::ButtonBg, true);
    QColorGroup iGrp = options->colorGroup(Options::ButtonBg, false);

    // close
    drawB2Rect(aClosePix, aGrp.button(), false);
    drawB2Rect(aClosePixDown, aGrp.button(), true);

    drawB2Rect(iClosePix, iGrp.button(), false);
    drawB2Rect(iClosePixDown, iGrp.button(), true);

    // maximize
    *aMaxPix = *aClosePix;
    aMaxPix->detach();
    *aMaxPixDown = *aClosePixDown;
    aMaxPixDown->detach();
    *iMaxPix = *iClosePix;
    iMaxPix->detach();
    *iMaxPixDown = *iClosePixDown;
    iMaxPixDown->detach();

    // normalize
    KPixmap smallBox;
    smallBox.resize(10, 10);
    KPixmap largeBox;
    largeBox.resize(12, 12);

    drawB2Rect(&smallBox, aGrp.button(), false);
    drawB2Rect(&largeBox, aGrp.button(), false);
    aNormalizePix->fill(options->color(Options::TitleBar, true));
    bitBlt(aNormalizePix, 3, 3, &largeBox, 0, 0, 12, 12, Qt::CopyROP, true);
    bitBlt(aNormalizePix, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

    bitBlt(aIconifyPix, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);
    
    drawB2Rect(&smallBox, aGrp.button(), true);
    drawB2Rect(&largeBox, aGrp.button(), true);
    aNormalizePixDown->fill(options->color(Options::TitleBar, true));
    bitBlt(aNormalizePixDown, 3, 3, &largeBox, 0, 0, 12, 12, Qt::CopyROP, true);
    bitBlt(aNormalizePixDown, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

    bitBlt(aIconifyPixDown, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

    drawB2Rect(&smallBox, iGrp.button(), false);
    drawB2Rect(&largeBox, iGrp.button(), false);
    iNormalizePix->fill(options->color(Options::TitleBar, false));
    bitBlt(iNormalizePix, 3, 3, &largeBox, 0, 0, 12, 12, Qt::CopyROP, true);
    bitBlt(iNormalizePix, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

    bitBlt(iIconifyPix, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);
    
    drawB2Rect(&smallBox, iGrp.button(), true);
    drawB2Rect(&largeBox, iGrp.button(), true);
    iNormalizePixDown->fill(options->color(Options::TitleBar, false));
    bitBlt(iNormalizePixDown, 3, 3, &largeBox, 0, 0, 12, 12, Qt::CopyROP, true);
    bitBlt(iNormalizePixDown, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

    bitBlt(iIconifyPixDown, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

    QPainter p;
    // x for close
    p.begin(aClosePix);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, close_white_bits,
                  NULL, NULL, close_dgray_bits, NULL, NULL);
    p.end();
    p.begin(aClosePixDown);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, close_white_bits,
                  NULL, NULL, close_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iClosePix);
    kColorBitmaps(&p, iGrp, 0, 0, 16, 16, true, close_white_bits,
                  NULL, NULL, close_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iClosePixDown);
    kColorBitmaps(&p, iGrp, 0, 0, 16, 16, true, close_white_bits,
                  NULL, NULL, close_dgray_bits, NULL, NULL);
    p.end();

    // pin
    p.begin(aPinupPix);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, pinup_white_bits,
                  pinup_gray_bits, NULL, pinup_dgray_bits, NULL, NULL);
    p.end();
    p.begin(aPinupPixDown);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, pindown_white_bits,
                  pindown_gray_bits, NULL, pindown_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iPinupPix);
    kColorBitmaps(&p, iGrp, 0, 0, 16, 16, true, pinup_white_bits,
                  pinup_gray_bits, NULL, pinup_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iPinupPixDown);
    kColorBitmaps(&p, iGrp, 0, 0, 16, 16, true, pindown_white_bits,
                  pindown_gray_bits, NULL, pindown_dgray_bits, NULL, NULL);
    p.end();

    // menu
    p.begin(aMenuPix);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, menu_white_bits,
                  NULL, NULL, menu_dgray_bits, NULL, NULL);
    p.end();
    p.begin(aMenuPixDown);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, menu_white_bits,
                  NULL, NULL, menu_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iMenuPix);
    kColorBitmaps(&p, iGrp, 0, 0, 16, 16, true, menu_white_bits,
                  NULL, NULL, menu_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iMenuPixDown);
    kColorBitmaps(&p, iGrp, 0, 0, 16, 16, true, menu_white_bits,
                  NULL, NULL, menu_dgray_bits, NULL, NULL);
    p.end();

    // help
    p.begin(aHelpPix);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, help_light_bits,
                  NULL, NULL, help_dark_bits, NULL, NULL);
    p.end();
    p.begin(aHelpPixDown);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, help_light_bits,
                  NULL, NULL, help_dark_bits, NULL, NULL);
    p.end();
    p.begin(iHelpPix);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, help_light_bits,
                  NULL, NULL, help_dark_bits, NULL, NULL);
    p.end();
    p.begin(iHelpPixDown);
    kColorBitmaps(&p, aGrp, 0, 0, 16, 16, true, help_light_bits,
                  NULL, NULL, help_dark_bits, NULL, NULL);
    p.end();
}

void B2Client::positionButtons()
{
    QFontMetrics fm(options->font(isActive()));
    
    int textLen = fm.width(caption());
    //int xpos = bar_x_ofs+4;
    int xpos = 4;
    button[BtnMenu]->move(xpos, 2);
    xpos+=17;
    button[BtnSticky]->move(xpos, 2);
    xpos+=17;
    if(providesContextHelp()){
        button[BtnHelp]->move(xpos, 2);
        xpos+=17;
    }

    if(xpos + textLen+52 < width()-3)
        xpos += textLen+1;
    else
        xpos = width()-3-52;

    button[BtnIconify]->move(xpos, 2);
    xpos+=17;
    button[BtnMax]->move(xpos, 2);
    xpos+=17;
    button[BtnClose]->move(xpos, 2);
    titlebar->setFixedSize(xpos+17+4,20);
    titlebar->move(bar_x_ofs, 0);
}

#include "b2client.moc"




