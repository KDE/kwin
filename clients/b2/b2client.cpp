/* 
 * B-II KWin Client
 *
 * Changes:
 *   Customizable button positions by Karol Szwed <gallium@kde.org>
 *   
 *   Thin frame in fixed size windows, titlebar gradient support and 
 *   accessibility improvements are
 *   Copyright (c) 2003 Luciano Montanaro <mikelima@virgilio.it>
 */ 

#include "b2client.h"
#include <qlayout.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <kconfig.h>
#include <qbitmap.h>
#include <qlabel.h>
#include <qtooltip.h>

#include <X11/Xlib.h>

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

KPixmap* titleGradient[2] = {0, 0};

static int thickness = 4; // Frame thickness

static bool pixmaps_created = false;
static bool colored_frame = false;

// =====================================

extern "C" KDecorationFactory* create_factory()
{
    return new B2::B2ClientFactory();
}

// =====================================

static inline const KDecorationOptions *options()
{
    return KDecoration::options();
}

static void read_config(B2ClientFactory *f)
{
    KConfig conf("kwinb2rc");
    conf.setGroup("General");
    colored_frame = conf.readBoolEntry( "UseTitleBarBorderColors", false );

    switch (options()->preferredBorderSize(f)) {
    case KDecoration::BorderTiny:
	thickness = 2;
	break;
    case KDecoration::BorderLarge:
	thickness = 5;
	break;
    case KDecoration::BorderVeryLarge:
	thickness = 8;
	break;
    case KDecoration::BorderHuge:
    case KDecoration::BorderVeryHuge:
    case KDecoration::BorderOversized:
    case KDecoration::BorderNormal:
    default:
	thickness = 4;
    }
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
    
    QBitmap normalizeMask(16, 16, true);
    // draw normalize icon mask
    QPainter mask(&normalizeMask);
	
    QBrush one(Qt::color1);
    mask.fillRect(3, 3, 12, 12, one);
    mask.fillRect(0, 0, 10, 10, one);
    
    for (i = 0; i < 4; i++) pixmap[P_NORMALIZE * 4 + i]->setMask(normalizeMask);
    
    titleGradient[0] = 0;
    titleGradient[1] = 0;

    redraw_pixmaps();
}

static void delete_pixmaps()
{
    for (int i = 0; i < NUM_PIXMAPS; i++) {
    	delete pixmap[i];
	pixmap[i] = 0;
    }
    for (int i = 0; i < 2; i++) {
    	delete titleGradient[i];
	titleGradient[i] = 0;
    }
    pixmaps_created = false;
}

// =====================================

B2Button::B2Button(B2Client *_client, QWidget *parent, const QString& tip)
   : QButton(parent, 0) 
{ 
    setBackgroundMode(NoBackground);
    client = _client;
    useMiniIcon = false;
    setFixedSize(16,16); 
    QToolTip::add(this, tip);
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
    KPixmap* gradient = titleGradient[client->isActive() ? 0 : 1];
    if (gradient) { 
	p->drawTiledPixmap(0, 0, 16, 16, *gradient, 0, 2);
    } else { 
	p->fillRect(rect(), bg);
    }
    if (useMiniIcon) {
        QPixmap miniIcon = client->icon().pixmap(QIconSet::Small,
		client->isActive() ? QIconSet::Normal : QIconSet::Disabled);
        p->drawPixmap((width()-miniIcon.width())/2,
                      (height()-miniIcon.height())/2, miniIcon);
    }
    else{
        if(client->isActive()){
            if (isDown())
                p->drawPixmap((width()-pDown->width())/2,
                              (height()-pDown->height())/2, *pDown);
            else
                p->drawPixmap((width()-pNorm->width())/2,
                              (height()-pNorm->height())/2, *pNorm);
        }
        else{
            if (isDown())
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
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
                    LeftButton, e->state());
    QButton::mousePressEvent(&me);
}

void B2Button::mouseReleaseEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
                   LeftButton, e->state());
    QButton::mouseReleaseEvent(&me);
}

// =====================================

B2Titlebar::B2Titlebar(B2Client *parent)
    : QWidget(parent->widget(), 0, WStyle_Customize | WRepaintNoErase),
      client(parent),
      set_x11mask(false), isfullyobscured(false), shift_move(false)
{
    setBackgroundMode(NoBackground);
    captionSpacer = new QSpacerItem(10, 20, QSizePolicy::Expanding,
                                            QSizePolicy::Fixed);
}

// TODO JUMPYTITLEBAR This is not useful until titlebar revealing can be reenabled

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

void B2Titlebar::drawTitlebar(QPainter &p, bool state)
{
    KPixmap* gradient = titleGradient[state ? 0 : 1];
    
    QRect t = rect();
    // black titlebar frame
    p.setPen(Qt::black);
    p.drawLine(0, 0, 0, t.bottom());
    p.drawLine(0, 0, t.right(), 0);
    p.drawLine(t.right(), 0, t.right(), t.bottom());

    // titlebar fill
    const QColorGroup cg = 
	options()->colorGroup(KDecoration::ColorTitleBar, state);
    QBrush brush(cg.background());
    if (gradient) brush.setPixmap(*gradient);
    qDrawShadeRect(&p, 1, 1, t.right() - 1, t.height() - 1, 
		   cg, false, 1, 0, &brush);

    // and the caption
    p.setPen(options()->color(KDecoration::ColorFont, state));
    p.setFont(options()->font(state));
    t = captionSpacer->geometry();
    p.drawText(t, AlignLeft | AlignVCenter, client->caption());
}

void B2Titlebar::recalcBuffer()
{
    QFontMetrics fm(options()->font(true));
    titleBuffer.resize(width(), height());

    QPainter p(&titleBuffer);
    drawTitlebar(p, true);
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
    else {
        QPainter p(this);
	drawTitlebar(p, false);
    }
}

void B2Titlebar::mouseDoubleClickEvent( QMouseEvent * )
{
    client->titlebarDblClickOperation();
}

#if 0
void B2Titlebar::mousePressEvent( QMouseEvent * e )
{
    shift_move = e->state() & ShiftButton;
    if (shift_move) {
        moveOffset = e->globalPos();
    } else {
	client->processMousePressEvent(e);
   	// client->performWindowOperation(KDecoration::MoveOp); 
    }
}

void B2Titlebar::mouseReleaseEvent( QMouseEvent * e )
{
    shift_move = false;
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
    } 
}

#endif

// =====================================

void B2Client::maxButtonClicked( )
{
    switch ( button[BtnMax]->last_button ) {
    case MidButton: 
	maximize(maximizeMode() ^ MaximizeVertical);
	break;
    case RightButton:
	maximize(maximizeMode() ^ MaximizeHorizontal);
	break;
    case LeftButton:
    default: 
	maximize(maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull);
	break;
    }
}

B2Client::B2Client(KDecorationBridge *b, KDecorationFactory *f) 
    : KDecoration(b, f), bar_x_ofs(0), in_unobs(0)
{
}

void B2Client::init()
{
    const QString tips[]= {i18n("Menu"), i18n("On All desktops"), 
                           i18n("Minimize"), i18n("Maximize"),
                           i18n("Close"), i18n("Help") };

    createMainWidget(WResizeNoErase | WRepaintNoErase);
    widget()->installEventFilter(this);

    widget()->setBackgroundMode(NoBackground);

    // Set button pointers to NULL so we know what has been created
    for(int i = 0; i < BtnCount; i++)
        button[i] = NULL;        

    g = new QGridLayout(widget(), 0, 0);
    if (isPreview()) {
        g->addMultiCellWidget( 
		new QLabel(i18n( "<b><center>B II preview</center></b>"), 
		    widget()), 
		1, 1, 1, 2);
    } else {
	g->addMultiCellWidget(new QWidget(widget()), 1, 1, 1, 2);
    }

    // Left and right border width
    leftSpacer = new QSpacerItem(thickness, 16, 
	    QSizePolicy::Minimum, QSizePolicy::Expanding);
    rightSpacer = new QSpacerItem(thickness, 16, 
	    QSizePolicy::Minimum, QSizePolicy::Expanding);

    g->addItem(leftSpacer, 1, 0);
    g->addColSpacing(1, 16);
    g->setColStretch(2, 1);
    g->setRowStretch(1, 1);
    g->addItem(rightSpacer, 1, 3);

    // Bottom border height
    spacer = new QSpacerItem(10, thickness + (isResizable() ? 4 : 0), 
	    QSizePolicy::Expanding, QSizePolicy::Minimum);
    g->addItem(spacer, 3, 1);

    // titlebar
    g->addRowSpacing(0, 20);

    titlebar = new B2Titlebar(this);
    titlebar->setMinimumWidth(16);
    titlebar->setFixedHeight(20);

    QBoxLayout *titleLayout = new QBoxLayout(titlebar, QBoxLayout::LeftToRight, 0, 1, 0);
    titleLayout->addSpacing(3);

    if (options()->customButtonPositions())
    {
        addButtons( options()->titleButtonsLeft(), tips, titlebar, titleLayout );
        titleLayout->addItem(titlebar->captionSpacer);
        addButtons( options()->titleButtonsRight(), tips, titlebar, titleLayout );
    } else {
        addButtons( "MSH", tips, titlebar, titleLayout );
        titleLayout->addItem(titlebar->captionSpacer);
        addButtons( "IAX", tips, titlebar, titleLayout );
    }

    titleLayout->addSpacing(3);
 
    QColor c = options()->colorGroup(KDecoration::ColorTitleBar, isActive()).
        color(QColorGroup::Button);

    for(int i = 0; i < BtnCount; i++)
        if (button[i])
            button[i]->setBg(c);

    titlebar->recalcBuffer();
    titlebar->installEventFilter(this);
    positionButtons();
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
                    button[BtnSticky]->setDown(isOnAllDesktops());
                    connect(button[BtnSticky], SIGNAL(clicked()),
                            this, SLOT(toggleOnAllDesktops()));
                    titleLayout->addWidget(button[BtnSticky]);
                }
                break;
            case 'H':  // Help button
                if (providesContextHelp() && (!button[BtnHelp])) {
                    button[BtnHelp] = new B2Button(this, tb, tips[BtnHelp]);
                    button[BtnHelp]->setPixmaps(P_HELP);
                    connect(button[BtnHelp], SIGNAL(clicked()),
                            this, SLOT(showContextHelp()));
                    titleLayout->addWidget(button[BtnHelp]);
                }
                break;
            case 'I':  // Minimize button
                if (isMinimizable() && (!button[BtnIconify])) {
                    button[BtnIconify]= new B2Button(this, tb,tips[BtnIconify]);
                    button[BtnIconify]->setPixmaps(P_ICONIFY);
                    connect(button[BtnIconify], SIGNAL(clicked()),
                            this, SLOT(minimize()));
                    titleLayout->addWidget(button[BtnIconify]);
                }
                break;
            case 'A':  // Maximize button
                if (isMaximizable() && (!button[BtnMax])) {
                    button[BtnMax]= new B2Button(this, tb, tips[BtnMax]);
                    button[BtnMax]->setPixmaps(maximizeMode() == MaximizeFull ?  
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
		break;
	    case '_': // Additional spacing
		titleLayout->addSpacing(4);
		break;
	}
    } 
}

void B2Client::reset(unsigned long)
{
    widget()->repaint();
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

void B2Client::resizeEvent(QResizeEvent * /*e*/)
{
    calcHiddenButtons();
    titlebar->layout()->activate();
    positionButtons();

    /* may be the resize cut off some space occupied by titlebar, which
       was moved, so instead of reducing it, we first try to move it */
    titleMoveAbs(bar_x_ofs);
    doShape();

    widget()->repaint(); // the frame is misrendered without this
}

void B2Client::captionChange()
{
    positionButtons();
    titleMoveAbs(bar_x_ofs);
    doShape();
    titlebar->recalcBuffer();
    titlebar->repaint(false);
}

void B2Client::paintEvent( QPaintEvent* e)
{
    QPainter p(widget());
    
    KDecoration::ColorType frameColorGroup = colored_frame ?  
	KDecoration::ColorTitleBar : KDecoration::ColorFrame;
    
    QRect t = titlebar->geometry();

    // Frame height, this is used a lot of times
    int fHeight = height() - t.height();

    // distance from the bottom border - it is different if window is resizable
    int bb = isResizable() ? 4 : 0;
    int bDepth = thickness + bb;

    QColorGroup fillColor = options()->colorGroup(frameColorGroup, isActive());
    QBrush fillBrush(options()->color(frameColorGroup, isActive()));

    // outer frame rect
    p.drawRect(0, t.bottom() - thickness + 1, width(), fHeight - bb + thickness);

    if (thickness >= 2) {
	// inner window rect
	p.drawRect(thickness - 1, t.bottom(), width() - 2 * (thickness - 1), 
		fHeight - bDepth + 2);
	
	if (thickness >= 3) {
	    // frame shade panel
	    qDrawShadePanel(&p, 1, t.bottom() - thickness + 2, 
	    	width() - 2, fHeight - 2 - bb + thickness, fillColor, false);
	    if (thickness == 4) {
		p.setPen(fillColor.background());
		p.drawRect(thickness - 2, t.bottom() - 1, 
			width() - 2 * (thickness - 2), fHeight + 4 - bDepth);
	    } else if (thickness > 4) {
		qDrawShadePanel(&p, thickness - 2, 
	    	    t.bottom() - 1, width() - 2 * (thickness - 2), 
	    	    fHeight + 4 - bDepth, fillColor, true);
		if (thickness >= 5) {
		    // draw frame interior
		    p.fillRect(2, t.bottom() - thickness + 3, 
		    	width() - 4, thickness - 4, fillBrush);
		    p.fillRect(2, height() - bDepth + 2, 
		    	width() - 4, thickness - 4, fillBrush);
		    p.fillRect(2, t.bottom() - 1, 
		    	thickness - 4, fHeight - bDepth + 4, fillBrush);
		    p.fillRect(width() - thickness + 2, t.bottom() - 1,
		    	thickness - 4, fHeight - bDepth + 4, fillBrush);
		}
	    }
	}
    }

    // bottom handle rect
    if (isResizable()) {
        p.setPen(Qt::black);
	int hx = width() - 40;
	int hw = 40;

	p.drawLine(width() - 1, height() - thickness - 4, 
		width() - 1, height() - 1);
	p.drawLine(hx, height() - 1, width() - 1, height() - 1);
	p.drawLine(hx, height() - 4, hx, height() - 1);

	p.fillRect(hx + 1, height() - thickness - 3, 
		hw - 2, thickness + 2, fillBrush);

	p.setPen(fillColor.dark()); 
	p.drawLine(width() - 2, height() - thickness - 4, 
		width() - 2, height() - 2);
	p.drawLine(hx + 1, height() - 2, width() - 2, height() - 2);

	p.setPen(fillColor.light());
	p.drawLine(hx + 1, height() - thickness - 2, 
		hx + 1, height() - 3);
	p.drawLine(hx + 1, height() - thickness - 3, 
		width() - 3, height() - thickness - 3);
    } 

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
    QRegion mask(widget()->rect());
    // top to the tilebar right
    if (bar_x_ofs) {
	mask -= QRect(0, 0, bar_x_ofs, t.height() - thickness); //left from bar
	mask -= QRect(0, t.height() - thickness, 1, 1); //top left point
    }
    if (t.right() < width() - 1) {
	mask -= QRect(width() - 1, 
		t.height() - thickness, 1, 1); //top right point
	mask -= QRect(t.right() + 1, 0, 
		width() - t.right() - 1, t.height() - thickness);
    }
    mask -= QRect(width() - 1, height() - 1, 1, 1); //bottom right point
    if (isResizable()) {
	mask -= QRect(0, height() - 5, 1, 1); //bottom left point
	mask -= QRect(width() - 1, height() - 1, 1, 1); //bottom right point
	mask -= QRect(width() - 40, height() - 1, 1, 1); //handle left point
	mask -= QRect(0, height() - 4, width() - 40, 4); //bottom left
    } else {
	mask -= QRect(0, height() - 1, 1, 1); // bottom left point
    }

    setMask(mask);
}

void B2Client::showEvent(QShowEvent *ev)
{
    calcHiddenButtons();
    doShape();
    widget()->repaint();
    titlebar->repaint(false);
}

KDecoration::MousePosition B2Client::mousePosition( const QPoint& p ) const
{
    const int range = 16;
    QRect t = titlebar->geometry();
    t.setHeight(20 - thickness);
    int ly = t.bottom();
    int lx = t.right();
    int bb = isResizable() ? 0 : 5;

    if ( p.x() > t.right() ) {
        if ( p.y() <= ly + range && p.x() >= width()-range)
            return TopRight2;
        else if ( p.y() <= ly + thickness )
            return Top;
    } else if ( p.x() < bar_x_ofs ) {
        if ( p.y() <= ly + range && p.x() <= range )
            return TopLeft2;
        else if ( p.y() <= ly + thickness )
            return Top;
    } else if ( p.y() < ly ) {
        if (p.x() > bar_x_ofs + thickness && 
		p.x() < lx - thickness && p.y() > thickness)
            return KDecoration::mousePosition(p);
        if (p.x() > bar_x_ofs + range && p.x() < lx - range)
            return Top;
        if ( p.y() <= range ) {
            if ( p.x() <= bar_x_ofs + range )
                return TopLeft2;
            else return TopRight2;
        } else {
            if ( p.x() <= bar_x_ofs + range )
                return Left;
            else return Right;
        }
    }

    if (p.y() >= height() - 8 + bb) {
        /* the normal Client:: only wants border of 4 pixels */
	if (p.x() <= range) return BottomLeft2;
	if (p.x() >= width() - range) return BottomRight2;
	return Bottom;
    }

    return KDecoration::mousePosition(p);
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
	widget()->repaint( 0, 0, width(), 20, false );
	titlebar->repaint(false);
    }
}

void B2Client::titleMoveRel(int xdiff)
{
    titleMoveAbs(bar_x_ofs + xdiff);
}

void B2Client::desktopChange()
{
    bool on = isOnAllDesktops();
    if (B2Button *b = button[BtnSticky]) {
        b->setDown(on);
	QToolTip::remove(b);
	QToolTip::add(b, on ? i18n("Not On All Desktops") : i18n("On All Desktops"));
    }
}

void B2Client::maximizeChange()
{
    bool m = maximizeMode() == MaximizeFull;
    if (button[BtnMax]) {
        button[BtnMax]->setPixmaps( m ? P_NORMALIZE : P_MAX );
        button[BtnMax]->repaint();
	QToolTip::remove(button[BtnMax]);
	QToolTip::add(button[BtnMax], 
		m ? i18n("Restore") : i18n("Maximize"));
    }
    spacer->changeSize(10, thickness + (isResizable() ? 4 : 0), 
	    QSizePolicy::Expanding, QSizePolicy::Minimum);

    g->activate();
    doShape();
    widget()->repaint(false);
}

void B2Client::activeChange()
{
    widget()->repaint(false);
    titlebar->repaint(false);

    QColor c = options()->colorGroup(
	    KDecoration::ColorTitleBar, isActive()).color(QColorGroup::Button);

    for(int i = 0; i < BtnCount; i++)
        if (button[i]) {
           button[i]->setBg(c);
           button[i]->repaint(false);
        }
}

void B2Client::shadeChange()
{
    spacer->changeSize(10, thickness + (isResizable() ? 4 : 0), 
	    QSizePolicy::Expanding, QSizePolicy::Minimum);
    g->activate();
    doShape();
}

QSize B2Client::minimumSize() const
{
    return QSize(64, 48); 
}

void B2Client::resize(const QSize& s)
{
    widget()->resize(s);
}

void B2Client::borders(int &left, int &right, int &top, int &bottom) const
{ 
    left = right = thickness + 1;
    top = 20;
    bottom = thickness + (isResizable() ? 4 : 0);
}

void B2Client::menuButtonPressed()
{
    QPoint menupoint = button[BtnMenu]->mapToGlobal(
	    button[BtnMenu]->rect().bottomLeft());
    showWindowMenu(menupoint);
    button[BtnMenu]->setDown(false);
}

void B2Client::slotReset()
{
    redraw_pixmaps();
    QColor c = options()->colorGroup(KDecoration::ColorTitleBar, isActive()).
        color(QColorGroup::Button);

    for(int i = 0; i < BtnCount; i++)
        if (button[i]) {
            button[i]->setBg(c);
            button[i]->repaint(false);
        }

    widget()->repaint();
    titlebar->recalcBuffer();
    titlebar->repaint(false);
}

void B2Client::unobscureTitlebar()
{
#if 0 // TODO JUMPYTITLEBAR
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
#endif
}

static void redraw_pixmaps()
{
    QColorGroup aGrp = options()->colorGroup(KDecoration::ColorButtonBg, true);
    QColorGroup iGrp = options()->colorGroup(KDecoration::ColorButtonBg, false);

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
	pix->fill(options()->color(KDecoration::ColorTitleBar, is_act));
	bitBlt(pix, 3, 3, &largeBox, 0, 0, 12, 12, Qt::CopyROP, true);
	bitBlt(pix, 0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);

	bitBlt(pixmap[P_ICONIFY*4 + i],
	       0, 0, &smallBox, 0, 0, 10, 10, Qt::CopyROP, true);
    }

    QPainter p;
    // x for close + menu + help
    for (int j = 0; j < 3; j++) {
        int pix;
        const unsigned char *light, *dark;
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
        const unsigned char *white = (i&1) ? pindown_white_bits : pinup_white_bits;
        const unsigned char *gray = (i&1) ? pindown_gray_bits : pinup_gray_bits;
        const unsigned char *dgray = (i&1) ? pindown_dgray_bits : pinup_dgray_bits;
        p.begin(pixmap[P_PINUP*4 + i]);
        kColorBitmaps(&p, (i<2)?aGrp:iGrp, 0, 0, 16, 16, true, white,
                      gray, NULL, dgray, NULL, NULL);
        p.end();
    }
    
    // Create the titlebar gradients
    if (QPixmap::defaultDepth() > 8) {
	QColor titleColor[4] = {
	    options()->color(KDecoration::ColorTitleBar, true),
            options()->color(KDecoration::ColorFrame, true),

	    options()->color(KDecoration::ColorTitleBlend, false),
	    options()->color(KDecoration::ColorTitleBar, false)
	};

	if (colored_frame) {
	    titleColor[0] = options()->color(KDecoration::ColorTitleBlend, true);
	    titleColor[1] = options()->color(KDecoration::ColorTitleBar, true);
	}
	
	for (i = 0; i < 2; i++) {
	    if (titleColor[2 * i] != titleColor[2 * i + 1]) {
		if (!titleGradient[i]) {
		    titleGradient[i] = new KPixmap;
		    titleGradient[i]->resize(64, 19);
		}
		KPixmapEffect::gradient(*titleGradient[i], 
			titleColor[2 * i], titleColor[2 * i + 1],
			KPixmapEffect::VerticalGradient);
	    } else { 
	       delete titleGradient[i]; 
	       titleGradient[i] = 0;
	    }
	}
    }
}

void B2Client::positionButtons()
{
    QFontMetrics fm(options()->font(isActive()));
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

// Transparent bound stuff.

static QRect *visible_bound;
static QPointArray bound_shape;

bool B2Client::drawbound(const QRect& geom, bool clear)
{
    if (clear) {
	if (!visible_bound) return true;
    }

    if (!visible_bound) {
	visible_bound = new QRect(geom);
	QRect t = titlebar->geometry();
	int frameTop = geom.top() + t.bottom();
	int barLeft = geom.left() + bar_x_ofs;
	int barRight = barLeft + t.width() - 1; 
	if (barRight > geom.right()) barRight = geom.right();
	
	bound_shape.putPoints(0, 8,
		geom.left(), frameTop,
		barLeft, frameTop,
		barLeft, geom.top(),
		barRight, geom.top(),
		barRight, frameTop,
		geom.right(), frameTop,
		geom.right(), geom.bottom(),
		geom.left(), geom.bottom());
    } else {
	*visible_bound = geom;
    }
    QPainter p(workspaceWidget());
    p.setPen(QPen(Qt::white, 5));
    p.setRasterOp(Qt::XorROP);
    p.drawPolygon(bound_shape);

    if (clear) {
	delete visible_bound;
	visible_bound = 0;
    }
    return true;
}

bool B2Client::eventFilter(QObject *o, QEvent *e)
{
    if (o != widget())
	return false;
    switch (e->type()) {
    case QEvent::Resize:
	resizeEvent(static_cast< QResizeEvent* >(e));
	return true;
    case QEvent::Paint:
	paintEvent(static_cast< QPaintEvent* >(e));
	return true;
    case QEvent::MouseButtonDblClick:
	titlebar->mouseDoubleClickEvent(static_cast< QMouseEvent* >(e));
	return true;
    case QEvent::MouseButtonPress:
	processMousePressEvent(static_cast< QMouseEvent* >(e));
	return true;
    case QEvent::Show:
	showEvent(static_cast< QShowEvent* >(e));
	return true;
    default:
	break;
    }
    return false;
}

// =====================================

B2ClientFactory::B2ClientFactory()
{
    read_config(this);
    create_pixmaps();
}

B2ClientFactory::~B2ClientFactory()
{
    delete_pixmaps();
}

KDecoration *B2ClientFactory::createDecoration(KDecorationBridge *b)
{
    return new B2::B2Client(b, this);
}

bool B2ClientFactory::reset(unsigned long /*changed*/)
{
    // TODO Do not recreate decorations if it is not needed. Look at
    // ModernSystem for how to do that
    read_config(this);
    redraw_pixmaps();
    // For now just return true.
    return true;
}

QValueList< B2ClientFactory::BorderSize > B2ClientFactory::borderSizes() const
{ 
    // the list must be sorted
    return QValueList< BorderSize >() << BorderTiny << BorderNormal << 
	BorderLarge << BorderVeryLarge;
}

}

#include "b2client.moc"

// vim: sw=4

