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

using namespace KWinInternal;

#define P_CLOSE 0
#define P_MAX 1
#define P_NORMALIZE 2
#define P_ICONIFY 3
#define P_PINUP 4
#define P_MENU 5
#define P_HELP 6
#define NUM_PIXMAPS ((P_HELP + 1) * 4)

static KPixmap *pixmap[NUM_PIXMAPS];

//active
#define PIXMAP_A(i)  (pixmap[(i)*4])
//active, down
#define PIXMAP_AD(i) (pixmap[(i)*4 +1])
//inactive
#define PIXMAP_I(i)  (pixmap[(i)*4 +2])
//inactive, down
#define PIXMAP_ID(i) (pixmap[(i)*4 +3])

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
    //return menu_pix;  FIXME
    return PIXMAP_A(P_MENU);
}

static void create_pixmaps()
{
    if ( pixmaps_created )
        return;
    pixmaps_created = true;

    int i;
    for (i = 0; i < NUM_PIXMAPS; i++) {
        pixmap[i] = new KPixmap;
	switch (i / 4) {
  	    case P_MAX : break;  // will be initialized by copying P_CLOSE
  	    case P_ICONIFY : pixmap[i]->resize(10,10); break;
  	    default : pixmap[i]->resize(16, 16); break;
	}
    }

    // there seems to be no way to load X bitmaps from data properly, so
    // we need to create new ones for each mask :P
    QBitmap pinupMask(16, 16, pinup_mask_bits, true);
    PIXMAP_A(P_PINUP)->setMask(pinupMask);
    PIXMAP_I(P_PINUP)->setMask(pinupMask);
    QBitmap pindownMask(16, 16, pindown_mask_bits, true);
    PIXMAP_AD(P_PINUP)->setMask(pindownMask);
    PIXMAP_ID(P_PINUP)->setMask(pindownMask);

    QBitmap menuMask(16, 16, menu_mask_bits, true);
    for (i = 0; i < 4; i++) pixmap[P_MENU * 4 + i]->setMask(menuMask);

    QBitmap helpMask(16, 16, help_mask_bits, true);
    for (i = 0; i < 4; i++) pixmap[P_HELP * 4 + i]->setMask(helpMask);
    redraw_pixmaps();
}

static void delete_pixmaps()
{
    for (int i = 0; i < NUM_PIXMAPS; i++)
      delete pixmap[i];
    pixmaps_created = false;
}

B2Button::B2Button(KPixmap *pix, KPixmap *pixDown, KPixmap *iPix,
                   KPixmap *iPixDown, Client *_client, QWidget *parent,
		   const char *name)
    : QButton(parent, name),
      pNorm(pix), pDown(pixDown), iNorm(iPix), iDown(iPixDown),
      client(_client)
{
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

void B2Button::setPixmaps(int button_id)
{
  button_id *= 4;
  setPixmaps(::pixmap[button_id], ::pixmap[button_id+1],
             ::pixmap[button_id+2], ::pixmap[button_id+3]);
}

void B2Button::mousePressEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
    QButton::mousePressEvent( &me );
}

void B2Button::mouseReleaseEvent( QMouseEvent* e )
{
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
    QButton::mouseReleaseEvent( &me );
}

B2Titlebar::B2Titlebar(B2Client *parent)
    : QWidget(parent),
      set_x11mask(false), isfullyobscured(false), shift_move(false),
      client(parent)
{
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

void B2Titlebar::recalcBuffer()
{
    QFontMetrics fm(options->font(true));
    titleBuffer.resize(width(), height());

    QPainter p;
    p.begin(&titleBuffer);
    QRect t=rect();

    // black titlebar frame
    p.setPen(Qt::black);
    p.drawLine(0, 0, 0, t.bottom() );
    p.drawLine(0, 0, t.right(), 0);
    p.drawLine(t.right(), 0, t.right(), t.bottom());

    // titlebar fill
    qDrawShadeRect(&p, 1, 1, t.right()-1, t.height()-1,
                   options->colorGroup(Options::TitleBar, true),
                   false, 1, 0,
                   &options->colorGroup(Options::TitleBar, true).
                   brush(QColorGroup::Background));

    // and the caption
    p.setPen(options->color(Options::Font, true));
    p.setFont(options->font(true));

    QLayoutIterator it=layout()->iterator();
    ++it;
    ++it;
    ++it;
    t=(++it)->geometry();

    p.drawText(t, AlignLeft | AlignVCenter, client->caption());
    p.end();

    oldTitle = caption();
}

void B2Titlebar::resizeEvent(QResizeEvent *)
{
    recalcBuffer();
    repaint(false);
}


void B2Titlebar::paintEvent(QPaintEvent * /*e*/)
{
    if(client->isActive())
        bitBlt(this, 0, 0, &titleBuffer, 0, 0, titleBuffer.width(),
               titleBuffer.height(), Qt::CopyROP, true);
    else{
        QPainter p(this);
        QRect t=rect();
        // black titlebar frame
        p.setPen(Qt::black);
        p.drawLine(0, 0, 0, t.bottom() );
        p.drawLine(0, 0, t.right(), 0);
        p.drawLine(t.right(), 0, t.right(), t.bottom());

        // titlebar fill
        qDrawShadeRect(&p, 1, 1, t.right()-1, t.height()-1,
                       options->colorGroup(Options::TitleBar, false),
                       false, 1, 0,
                       &options->colorGroup(Options::TitleBar, false).
                       brush(QColorGroup::Button));

        // and the caption
        p.setPen(options->color(Options::Font, false));
        p.setFont(options->font(false));

	QLayoutIterator it=layout()->iterator();
	++it;
	++it;
	++it;
	t=(++it)->geometry();

        p.drawText(t, AlignLeft | AlignVCenter, client->caption());
    }
}

void B2Titlebar::mouseDoubleClickEvent( QMouseEvent * )
{
    client->workspace()->performWindowOperation( client, options->operationTitlebarDblClick() );
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

void B2Client::maxButtonClicked( )
{
    switch ( button[BtnMax]->last_button ) {
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

B2Client::B2Client( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase ),
      bar_x_ofs(0), in_unobs(0)
{
    g = new QGridLayout( this, 0, 0);
    g->addMultiCellWidget(windowWrapper(), 1, 1, 1, 2);

    g->addColSpacing(0, 4);

    g->addColSpacing(1, 16);

    g->setColStretch(2, 1);

    g->setRowStretch(1, 1);
    g->addColSpacing(3, 4);
    g->addRowSpacing(2, 8);

    // titlebar
    g->addRowSpacing(0, 20);

    titlebar = new B2Titlebar(this);
    titlebar->setMinimumWidth(16);
    titlebar->setFixedHeight(20);

    int i;
    for(i=0; i < 6; ++i){
        button[i] = new B2Button(this, titlebar/*this*/);
        button[i]->setFixedSize(16, 16);
    }
    
    QHBoxLayout *titleLayout=new QHBoxLayout(titlebar);
    titleLayout->setSpacing(1);
   
    titleLayout->addSpacing(3);
    titleLayout->addWidget(button[BtnMenu]);
    titleLayout->addWidget(button[BtnSticky]);
    titleLayout->addWidget(button[BtnHelp]);
    titleLayout->addStretch(1);
    titleLayout->addWidget(button[BtnIconify]);
    titleLayout->addWidget(button[BtnMax]);
    titleLayout->addWidget(button[BtnClose]);
    titleLayout->addSpacing(3);
 
    button[BtnSticky]->setToggle();
    button[BtnSticky]->setDown(isSticky());
    button[BtnMenu]->setUseMiniIcon();

    if(!providesContextHelp())
        button[BtnHelp]->hide();

    button[BtnMenu]->setPixmaps(P_MENU);
    button[BtnSticky]->setPixmaps(P_PINUP);
    button[BtnIconify]->setPixmaps(P_ICONIFY);
    button[BtnClose]->setPixmaps(P_CLOSE);
    button[BtnHelp]->setPixmaps(P_HELP);

    if(isMaximized())
        button[BtnMax]->setPixmaps(P_NORMALIZE);
    else
        button[BtnMax]->setPixmaps(P_MAX);

    QColor c = options->colorGroup(Options::TitleBar, isActive()).
        color(QColorGroup::Button);
    titlebar->setBackgroundColor(c);
    titlebar->recalcBuffer();
    for(i=0; i < 6; ++i)
        button[i]->setBg(c);

    positionButtons();
    connect(button[BtnMenu], SIGNAL(clicked()), this, SLOT(menuButtonPressed()));
    connect(button[BtnSticky], SIGNAL(clicked()), this, SLOT(toggleSticky()));
    connect(button[BtnIconify], SIGNAL(clicked()), this, SLOT(iconify()));
    connect(button[BtnMax], SIGNAL( clicked() ), this, SLOT( maxButtonClicked()));
    connect(button[BtnClose], SIGNAL(clicked()), this, SLOT(closeWindow()));
    connect(button[BtnHelp], SIGNAL(clicked()), this, SLOT(contextHelp()));

    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
}

void B2Client::iconChange()
{
    button[BtnMenu]->repaint( false );   
}

void B2Client::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
  
    int sizeProblem = 0;

    if (width() < 45) sizeProblem = 3;
    else if (width() < 70) sizeProblem = 2;
    else if (width() < 120) sizeProblem = 1;
 
    switch (sizeProblem) {
 
      case 1:
        button[BtnMenu]    ->show();
        button[BtnClose]   ->show();
        button[BtnSticky]  ->hide();
	button[BtnIconify] ->show();
	button[BtnMax] 	   ->hide();
	button[BtnHelp]    ->hide();
	break;

      case 2:
        button[BtnMenu]    ->show();
	button[BtnClose]   ->show();
        button[BtnSticky]  ->hide();
	button[BtnIconify] ->hide();
	button[BtnMax] 	   ->hide();
	button[BtnHelp]    ->hide();
	break;

      case 3:
        button[BtnMenu]    ->hide();
	button[BtnClose]   ->hide();
        button[BtnSticky]  ->hide();
	button[BtnIconify] ->hide();
	button[BtnMax] 	   ->hide();
	button[BtnHelp]    ->hide();
	break;
 
      case 0:
      default:
        button[BtnMenu]    ->show();
	button[BtnClose]   ->show();
        button[BtnSticky]  ->show();
	button[BtnIconify] ->show();
	button[BtnMax] 	   ->show();
	if(providesContextHelp())
	  button[BtnHelp]->show();
	break;
    }   

    titlebar->layout()->activate();

    positionButtons();
    /* may be the resize cuted off some space occupied by titlebar, which
       was moved, so instead of reducing it, we first try to move it */
    titleMoveAbs(bar_x_ofs);
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
    titleMoveAbs(bar_x_ofs);
    doShape();
    //repaint();
    titlebar->recalcBuffer();
    titlebar->repaint(false);
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
    button[BtnSticky]->setDown(on);
}

void B2Client::maximizeChange(bool m)
{
    if(m){
        button[BtnMax]->setPixmaps(P_NORMALIZE);
    }
    else{
        button[BtnMax]->setPixmaps(P_MAX);
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
    titlebar->recalcBuffer();
    titlebar->repaint(false);
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
    drawB2Rect(PIXMAP_A(P_CLOSE), aGrp.button(), false);
    drawB2Rect(PIXMAP_AD(P_CLOSE), aGrp.button(), true);

    drawB2Rect(PIXMAP_I(P_CLOSE), iGrp.button(), false);
    drawB2Rect(PIXMAP_ID(P_CLOSE), iGrp.button(), true);

    // maximize
    int i;
    for (i = 0; i < 4; i++) {
	*pixmap[P_MAX*4 + i] = *pixmap[P_CLOSE*4 + i];
	pixmap[P_MAX*4 + i]->detach();
    }

    // normalize + iconify
    KPixmap smallBox;
    smallBox.resize(10, 10);
    KPixmap largeBox;
    largeBox.resize(12, 12);

    for (i = 0; i < 4; i++) {
	bool is_act = (i < 2);
	bool is_down = ((i & 1) == 1);
	KPixmap *pix = pixmap[P_NORMALIZE*4 + i];
	drawB2Rect(&smallBox, aGrp.button(), is_down);
	drawB2Rect(&largeBox, aGrp.button(), is_down);
	pix->fill(options->color(Options::TitleBar, is_act));
	bitBlt(pix, 3, 3, &largeBox, 0, 0, 12, 12, Qt::CopyROP, true);
	bitBlt(pix, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

	bitBlt(pixmap[P_ICONIFY*4 + i],
	       0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);
    }

    QPainter p;
    // x for close + menu + help
    for (int j = 0; j < 3; j++) {
        int pix;
        unsigned char *light, *dark;
        switch (j) {
          case 0 :
            pix = P_CLOSE; light = close_white_bits; dark = close_dgray_bits;
            break;
          case 1 :
            pix = P_MENU; light = menu_white_bits; dark = menu_dgray_bits;
            break;
          case 2 :
            pix = P_HELP; light = help_light_bits; dark = help_dark_bits;
            break;
        }
        for (i = 0; i < 4; i++) {
            p.begin(pixmap[pix*4 + i]);
            kColorBitmaps(&p, (i<2)?aGrp:iGrp, 0, 0, 16, 16, true,
                          light, NULL, NULL, dark, NULL, NULL);
            p.end();
        }
    }

    // pin
    for (i = 0; i < 4; i++) {
        unsigned char *white = (i&1) ? pindown_white_bits : pinup_white_bits;
        unsigned char *gray = (i&1) ? pindown_gray_bits : pinup_gray_bits;
        unsigned char *dgray = (i&1) ? pindown_dgray_bits : pinup_dgray_bits;
        p.begin(pixmap[P_PINUP*4 + i]);
        kColorBitmaps(&p, (i<2)?aGrp:iGrp, 0, 0, 16, 16, true, white,
                      gray, NULL, dgray, NULL, NULL);
        p.end();
    }
}

void B2Client::positionButtons()
{
    QFontMetrics fm(options->font(isActive()));

    int textLen = fm.width(caption());
    QLayoutIterator it=titlebar->layout()->iterator();
    ++it;
    ++it;
    ++it;

    int titleWidth=titlebar->width()-(++it)->geometry().width()+textLen+2;
   
    if( titleWidth > width()) titleWidth=width();

    titlebar->resize(titleWidth,20);
    titlebar->move(bar_x_ofs, 0);
}

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
       return(new B2Client(ws, w));
    }
    void init()
    {
       create_pixmaps();
    }
    void reset()
    {
       redraw_pixmaps();
    }
    void deinit()
    {
       delete_pixmaps();
    }
}

#include "b2client.moc"
