/* 
 * $Id$
 *
 * B-II KWin Client
 *
 * Changes:
 * 	Customizable button positions by Karol Szwed <gallium@kde.org>
 */ 

#include "b2client.h"
#include <qlayout.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <kconfig.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

using namespace KWinInternal;

namespace B2 {

#include "bitmaps.h"

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
static bool colored_frame = false;

static void read_config()
{
    KConfig conf("kwinb2rc");
    conf.setGroup("General");
    colored_frame = conf.readBoolEntry( "UseTitleBarBorderColors", false );
}

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


B2Button::B2Button(Client *_client, QWidget *parent, const QString& tip)
   : KWinButton(parent, 0, tip) 
{ 
    setBackgroundMode(NoBackground);
    client = _client;
    useMiniIcon = false;
    setFixedSize(16,16); 
};


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
  setPixmaps(B2::pixmap[button_id], B2::pixmap[button_id+1],
	     B2::pixmap[button_id+2], B2::pixmap[button_id+3]);
}

void B2Button::mousePressEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), 
                     LeftButton, e->state() );
    KWinButton::mousePressEvent( &me );
}

void B2Button::mouseReleaseEvent( QMouseEvent* e )
{
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), 
                     LeftButton, e->state() );
    KWinButton::mouseReleaseEvent( &me );
}

B2Titlebar::B2Titlebar(B2Client *parent)
    : QWidget(parent, 0, WStyle_Customize | WRepaintNoErase),
      set_x11mask(false), isfullyobscured(false), shift_move(false),
      client(parent)
{
    setBackgroundMode(NoBackground);
    captionSpacer = new QSpacerItem(10, 20, QSizePolicy::Expanding,
                                            QSizePolicy::Fixed);
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
    t = captionSpacer->geometry();
    p.drawText(t, AlignLeft | AlignVCenter, client->caption());
    p.end();

    oldTitle = caption();
}

void B2Titlebar::resizeEvent(QResizeEvent *)
{
    recalcBuffer();
    repaint(false);
}


void B2Titlebar::paintEvent(QPaintEvent *)
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
	t = captionSpacer->geometry();
        p.drawText(t, AlignLeft | AlignVCenter, client->caption());
    }
}

void B2Titlebar::mouseDoubleClickEvent( QMouseEvent * )
{
    client->workspace()->performWindowOperation( client, options->operationTitlebarDblClick() );
    // ?? client->workspace()->requestFocus( client );
}

void B2Titlebar::mousePressEvent( QMouseEvent * e )
{
    shift_move = e->state() & ShiftButton;
    if (shift_move ) {
        moveOffset = e->globalPos();
    }
    QMouseEvent _e(QEvent::MouseButtonPress, mapToParent(e->pos()),
        e->globalPos(), e->button(), e->state());
    client->mousePressEvent( &_e);
}

void B2Titlebar::mouseReleaseEvent( QMouseEvent * e )
{
    shift_move = false;
    QMouseEvent _e(QEvent::MouseButtonRelease, mapToParent(e->pos()),
        e->globalPos(), e->button(), e->state());
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
    : Client( ws, w, parent, name, WResizeNoErase | WRepaintNoErase ),
      bar_x_ofs(0), in_unobs(0)
{
    const QString tips[]= {i18n("Menu"), i18n("Sticky"), 
                           i18n("Minimize"), i18n("Maximize"),
                           i18n("Close"), i18n("Help") };

    setBackgroundMode(NoBackground);

    // Set button pointers to NULL so we know what has been created
    for(int i = 0; i < BtnCount; i++)
        button[i] = NULL;        

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

    QBoxLayout *titleLayout = new QBoxLayout(titlebar, QBoxLayout::LeftToRight, 0, 1, 0);
    titleLayout->addSpacing(3);

    if (options->customButtonPositions())
    {
        addButtons( options->titleButtonsLeft(), tips, titlebar, titleLayout );
        titleLayout->addItem(titlebar->captionSpacer);
        addButtons( options->titleButtonsRight(), tips, titlebar, titleLayout );
    } else {
        addButtons( "MSH", tips, titlebar, titleLayout );
        titleLayout->addItem(titlebar->captionSpacer);
        addButtons( "IAX", tips, titlebar, titleLayout );
    }

    titleLayout->addSpacing(3);
 
    QColor c = options->colorGroup(Options::TitleBar, isActive()).
        color(QColorGroup::Button);

    for(int i = 0; i < BtnCount; i++)
        if (button[i])
            button[i]->setBg(c);

    titlebar->recalcBuffer();
    positionButtons();

    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
}

void B2Client::addButtons(const QString& s, const QString tips[],
                          B2Titlebar* tb, QBoxLayout* titleLayout)
{
    if (s.length() <= 0)
	return;

    for(unsigned int i = 0; i < s.length(); i++) {
        switch(s[i].latin1()) {
            case 'M':  // Menu button
                if (!button[BtnMenu]) {
                    button[BtnMenu] = new B2Button(this, tb, tips[BtnMenu]);
                    button[BtnMenu]->setPixmaps(P_MENU);
                    button[BtnMenu]->setUseMiniIcon();
                    connect(button[BtnMenu], SIGNAL(clicked()),
                            this, SLOT(menuButtonPressed()));
                    titleLayout->addWidget(button[BtnMenu]);
                }
                break;
            case 'S':  // Sticky button
                if (!button[BtnSticky]) {
                    button[BtnSticky] = new B2Button(this, tb, tips[BtnSticky]);
                    button[BtnSticky]->setPixmaps(P_PINUP);
                    button[BtnSticky]->setToggle();
                    button[BtnSticky]->setDown(isSticky());
                    connect(button[BtnSticky], SIGNAL(clicked()),
                            this, SLOT(toggleSticky()));
                    titleLayout->addWidget(button[BtnSticky]);
                }
                break;
            case 'H':  // Help button
                if (providesContextHelp() && (!button[BtnHelp])) {
                    button[BtnHelp] = new B2Button(this, tb, tips[BtnHelp]);
                    button[BtnHelp]->setPixmaps(P_HELP);
                    connect(button[BtnHelp], SIGNAL(clicked()),
                            this, SLOT(contextHelp()));
                    titleLayout->addWidget(button[BtnHelp]);
                }
                break;
            case 'I':  // Minimize button
                if (isMinimizable() && (!button[BtnIconify])) {
                    button[BtnIconify]= new B2Button(this, tb,tips[BtnIconify]);
                    button[BtnIconify]->setPixmaps(P_ICONIFY);
                    connect(button[BtnIconify], SIGNAL(clicked()),
                            this, SLOT(iconify()));
                    titleLayout->addWidget(button[BtnIconify]);
                }
                break;
            case 'A':  // Maximize button
                if (isMaximizable() && (!button[BtnMax])) {
                    button[BtnMax]= new B2Button(this, tb, tips[BtnMax]);
                    button[BtnMax]->setPixmaps(isMaximized() ? 
                                               P_NORMALIZE : P_MAX);
                    connect(button[BtnMax], SIGNAL(clicked()),
                            this, SLOT(maxButtonClicked()));
                    titleLayout->addWidget(button[BtnMax]);
                }
                break;
            case 'X':  // Close button
                if (isCloseable() && !button[BtnClose]) {
                    button[BtnClose]= new B2Button(this, tb, tips[BtnClose]);
                    button[BtnClose]->setPixmaps(P_CLOSE);
                    connect(button[BtnClose], SIGNAL(clicked()),
                            this, SLOT(closeWindow()));
                    titleLayout->addWidget(button[BtnClose]);
                }
	}
    } 
}


void B2Client::iconChange()
{
    if (button[BtnMenu])
        button[BtnMenu]->repaint(false);   
}


// Gallium: New button show/hide magic for customizable
//          button positions.
void B2Client::calcHiddenButtons()
{
    // Hide buttons in this order:
    // Sticky, Help, Maximize, Minimize, Close, Menu
    B2Button* btnArray[] = { button[BtnSticky], button[BtnHelp],
                             button[BtnMax], button[BtnIconify],
                             button[BtnClose], button[BtnMenu] };
    int minWidth = 120; 
    int currentWidth = width();
    int count = 0;
    int i;

    // Determine how many buttons we need to hide
    while(currentWidth < minWidth) {
        currentWidth += 17;  // Allow for spacer (extra 1pix)
        count++;
    }
    // Bound the number of buttons to hide
    if (count > BtnCount) count = BtnCount;

    // Hide the required buttons
    for(i = 0; i < count; i++) {
        if (btnArray[i] && btnArray[i]->isVisible())
            btnArray[i]->hide();
    }
    // Show the rest of the buttons
    for(i = count; i < BtnCount; i++) {
        if (btnArray[i] && (!btnArray[i]->isVisible()))
            btnArray[i]->show();
    }
}

void B2Client::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
    calcHiddenButtons();
    titlebar->layout()->activate();
    positionButtons();

    /* may be the resize cuted off some space occupied by titlebar, which
       was moved, so instead of reducing it, we first try to move it */
    titleMoveAbs(bar_x_ofs);
    doShape();

    repaint(); //there is some strange wrong repaint of the frame without
}

void B2Client::captionChange( const QString &)
{
    positionButtons();
    titleMoveAbs(bar_x_ofs);
    doShape();
    titlebar->recalcBuffer();
    titlebar->repaint(false);
}

void B2Client::paintEvent( QPaintEvent* e)
{
    QPainter p( this );

    QRect t = titlebar->geometry();

    // inner window rect
    p.drawRect(3, t.bottom(), width()-6, height()-t.height()-6);
    p.drawLine(4, t.bottom()+1, width()-5, t.bottom()+1);

    // outer frame rect
    p.drawRect(0, t.bottom()-3, width(), height()-t.height());

    // draw frame interior
    if (colored_frame)
        p.setPen(options->color(Options::TitleBar, isActive()));
    else
        p.setPen(options->color(Options::Frame, isActive()));
    
    p.drawRect(2, t.bottom()-1, width()-4, height()-t.height()-4);
    p.setPen(Qt::black);
    
    // frame shade panel
   if (colored_frame)
       qDrawShadePanel(&p, 1, t.bottom()-2, width()-2, height()-t.height()-2,
                   options->colorGroup(Options::TitleBar, isActive()), false);
   else
       qDrawShadePanel(&p, 1, t.bottom()-2, width()-2, height()-t.height()-2,
                    options->colorGroup(Options::Frame, isActive()), false);
   
    //bottom handle rect
    int hx = width()-40;
    int hw = 40;

    p.drawLine(width()-1, height()-8, width()-1, height()-1);
    p.drawLine(hx, height()-1, width()-1, height()-1);
    p.drawLine(hx, height()-4, hx, height()-1);

    p.fillRect(hx+1, height()-7, hw-2, 6, 
        options->color(colored_frame ? Options::TitleBar:Options::Frame,
	               isActive()));

    p.setPen(options->colorGroup(colored_frame ? Options::TitleBar:Options::Frame,
                                 isActive()).dark());				 
    p.drawLine(width()-2, height()-8, width()-2, height()-2);
    p.drawLine(hx+1, height()-2, width()-2, height()-2);

    p.setPen(options->colorGroup(colored_frame ? Options::TitleBar:Options::Frame,
                                 isActive()).light());
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
    calcHiddenButtons();
    Client::showEvent(ev);
    doShape();
    repaint();
    titlebar->repaint(false);
}

void B2Client::windowWrapperShowEvent( QShowEvent* )
{
    doShape();
}

Client::MousePosition B2Client::mousePosition( const QPoint& p ) const
{
    const int range = 16;
    const int border = 4;
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
    if (button[BtnSticky]) {
        button[BtnSticky]->setDown(on);
        button[BtnSticky]->setTipText(on ? i18n("Un-Sticky") : i18n("Sticky"));
    }
}

void B2Client::maximizeChange(bool m)
{
    if (button[BtnMax]) {
        button[BtnMax]->setPixmaps( m ? P_NORMALIZE : P_MAX );
        button[BtnMax]->repaint();
        button[BtnMax]->setTipText(m ? i18n("Restore") : i18n("Maximize"));
    }
}

void B2Client::activeChange(bool on)
{
    repaint(false);
    titlebar->repaint(false);

    QColor c = options->colorGroup(Options::TitleBar, on).
        color(QColorGroup::Button);

    for(int i = 0; i < BtnCount; i++)
        if (button[i]) {
           button[i]->setBg(c);
           button[i]->repaint(false);
        }
}

void B2Client::menuButtonPressed()
{
    QPoint menupoint = button[BtnMenu]->mapToGlobal(button[BtnMenu]->
		              rect().bottomLeft());
    workspace()->showWindowMenu( menupoint, this );
    button[BtnMenu]->setDown(false);
}

void B2Client::slotReset()
{
    redraw_pixmaps();
    QColor c = options->colorGroup(Options::TitleBar, isActive()).
        color(QColorGroup::Button);

    for(int i = 0; i < BtnCount; i++)
        if (button[i]) {
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
	drawB2Rect(&smallBox, is_act ? aGrp.button() : iGrp.button(), is_down);
	drawB2Rect(&largeBox, is_act ? aGrp.button() : iGrp.button(), is_down);
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
          default:
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
    QString cap = caption();
    if( cap.length() < 5 ) // make sure the titlebar has sufficiently wide
        cap = "XXXXX";     // area for dragging the window
    int textLen = fm.width( cap );

    QRect t = titlebar->captionSpacer->geometry();
    int titleWidth = titlebar->width() - t.width() + textLen+2;
    if( titleWidth > width()) titleWidth=width();

    titlebar->resize(titleWidth, 20);
    titlebar->move(bar_x_ofs, 0);
}

};

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
       return(new B2::B2Client(ws, w));
    }
    void init()
    {
       B2::read_config();
       B2::create_pixmaps();
    }
    void reset()
    {
       B2::read_config();
       B2::redraw_pixmaps();
       // Ensure change in tooltip state gets applied
       KWinInternal::Workspace::self()->slotResetAllClientsDelayed();
    }
    void deinit()
    {
       B2::delete_pixmaps();
    }
}

#include "b2client.moc"
