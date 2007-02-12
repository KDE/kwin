#ifndef CLIENTS_B2_B2CLIENT
#define CLIENTS_B2_B2CLIENT
/*
 * B-II KWin Client
 *
 * Changes:
 *   Customizable button positions by Karol Szwed <gallium@kde.org>
 *
 *   Thin frame in fixed size windows, titlebar gradient support, accessibility
 *   improvements, customizable menu double click action and button hover
 *   effects are
 *   Copyright (c) 2003, 2004, 2006 Luciano Montanaro <mikelima@cirulla.net>
 */

#include "b2client.h"
#include <QApplication>
#include <QLayout>
#include <qdrawutil.h>
#include <QPixmap>
#include <QPaintEvent>
#include <QPolygon>
#include <QGridLayout>
#include <QEvent>
#include <QBoxLayout>
#include <QShowEvent>
#include <QStyle>
#include <QResizeEvent>
#include <QMouseEvent>
#include <kpixmapeffect.h>
#include <kimageeffect.h>
#include <kicontheme.h>
#include <kiconeffect.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <kconfig.h>
#include <QBitmap>
#include <QLabel>
#include <QToolTip>
#include <QPixmap>

#include <X11/Xlib.h>
#include <QX11Info>

namespace B2 {

#include "bitmaps.h"

enum {
    Norm = 0,
    Hover, Down, INorm, IHover, IDown,
    NumStates
};

enum {
    P_CLOSE = 0,
    P_MAX, P_NORMALIZE, P_ICONIFY, P_PINUP, P_MENU, P_HELP, P_SHADE, P_RESIZE,
    P_NUM_BUTTON_TYPES
};

#define NUM_PIXMAPS (P_NUM_BUTTON_TYPES * NumStates)

static QPixmap *pixmap[NUM_PIXMAPS];

// active
#define PIXMAP_A(i)  (pixmap[(i) * NumStates + Norm])
// active, hover
#define PIXMAP_AH(i) (pixmap[(i) * NumStates + Hover])
// active, down
#define PIXMAP_AD(i) (pixmap[(i) * NumStates + Down])
// inactive
#define PIXMAP_I(i)  (pixmap[(i) * NumStates + INorm])
// inactive, hover
#define PIXMAP_IH(i) (pixmap[(i) * NumStates + IHover])
// inactive, down
#define PIXMAP_ID(i) (pixmap[(i) * NumStates + IDown])

static QPixmap* titleGradient[2] = {0, 0};

static int thickness = 4; // Frame thickness
static int buttonSize = 16;

enum DblClickOperation {
	NoOp = 0,
	MinimizeOp,
	ShadeOp,
	CloseOp
};

static DblClickOperation menu_dbl_click_op = NoOp;

static bool pixmaps_created = false;
static bool colored_frame = false;
static bool do_draw_handle = true;
static bool drawSmallBorders = false;

// =====================================

extern "C" KDE_EXPORT KDecorationFactory* create_factory()
{
    return new B2::B2ClientFactory();
}

// =====================================

static inline const KDecorationOptions *options()
{
    return KDecoration::options();
}

static void redraw_pixmaps();

static void read_config(B2ClientFactory *f)
{
    // Force button size to be in a reasonable range.
    // If the frame width is large, the button size must be large too.
    buttonSize = (QFontMetrics(options()->font(true)).height() + 1) & 0x3e;
    if (buttonSize < 16) buttonSize = 16;

    KConfig conf("kwinb2rc");
    conf.setGroup("General");
    colored_frame = conf.readEntry("UseTitleBarBorderColors", false);
    do_draw_handle = conf.readEntry("DrawGrabHandle", true);
    drawSmallBorders = !options()->moveResizeMaximizedWindows();

    QString opString = conf.readEntry("MenuButtonDoubleClickOperation", "NoOp");
    if (opString == "Close") {
        menu_dbl_click_op = B2::CloseOp;
    } else if (opString == "Minimize") {
        menu_dbl_click_op = B2::MinimizeOp;
    } else if (opString == "Shade") {
        menu_dbl_click_op = B2::ShadeOp;
    } else {
        menu_dbl_click_op = B2::NoOp;
    }

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
	thickness = 12;
	break;
    case KDecoration::BorderNormal:
    default:
	thickness = 4;
    }
}

static void drawB2Rect(QPixmap *pix, const QColor &primary, bool down)
{
    QPainter p(pix);
    QColor hColor = primary.light(150);
    QColor lColor = primary.dark(150);

    if (down) qSwap(hColor, lColor);

    if (QPixmap::defaultDepth() > 8) {
	KPixmapEffect::gradient(*pix, hColor, lColor,
		KPixmapEffect::DiagonalGradient);
    }
    else
        pix->fill(primary);
    int x2 = pix->width() - 1;
    int y2 = pix->height() - 1;
    p.setPen(lColor);
    p.drawLine(0, 0, x2, 0);
    p.drawLine(0, 0, 0, y2);
    p.drawLine(1, x2 - 1, x2 - 1, y2 - 1);
    p.drawLine(x2 - 1, 1, x2 - 1, y2 - 1);
    p.setPen(hColor);
    p.drawRect(1, 1, x2, y2);

}

QPixmap* kwin_get_menu_pix_hack()
{
    //return menu_pix;  FIXME
    return PIXMAP_A(P_MENU);
}

static void create_pixmaps()
{
    if (pixmaps_created)
        return;
    pixmaps_created = true;

    int i;
    int bsize = buttonSize - 2;
    if (bsize < 16) bsize = 16;

    for (i = 0; i < NUM_PIXMAPS; i++) {

        switch (i / NumStates) {
	case P_MAX: // will be initialized by copying P_CLOSE
	case P_RESIZE:
	    pixmap[i] = new QPixmap();
	    break;
	case P_ICONIFY:
	    pixmap[i] = new QPixmap(10, 10);
	    break;
	case P_SHADE:
	case P_CLOSE:
	    pixmap[i] = new QPixmap(bsize, bsize);
	    break;
	default:
	    pixmap[i] = new QPixmap(16, 16);
	    break;
        }
    }

    // there seems to be no way to load X bitmaps from data properly, so
    // we need to create new ones for each mask :P
    QBitmap pinupMask = QBitmap::fromData(QSize(16, 16), pinup_mask_bits);
    PIXMAP_A(P_PINUP)->setMask(pinupMask);
    PIXMAP_I(P_PINUP)->setMask(pinupMask);
    QBitmap pindownMask = QBitmap::fromData(QSize(16, 16), pindown_mask_bits);
    PIXMAP_AD(P_PINUP)->setMask(pindownMask);
    PIXMAP_ID(P_PINUP)->setMask(pindownMask);

    QBitmap menuMask = QBitmap::fromData(QSize(16, 16), menu_mask_bits);
    for (i = 0; i < NumStates; i++)
	pixmap[P_MENU * NumStates + i]->setMask(menuMask);

    QBitmap helpMask = QBitmap::fromData(QSize(16, 16), help_mask_bits);
    for (i = 0; i < NumStates; i++)
	pixmap[P_HELP * NumStates + i]->setMask(helpMask);

    QBitmap normalizeMask(16, 16);
    normalizeMask.clear();
    // draw normalize icon mask
    QPainter mask;
    mask.begin(&normalizeMask);

    QBrush one(Qt::color1);
    mask.fillRect(normalizeMask.width() - 12, normalizeMask.height() - 12,
		  12, 12, one);
    mask.fillRect(0, 0, 10, 10, one);
    mask.end();

    for (i = 0; i < NumStates; i++)
	pixmap[P_NORMALIZE * NumStates + i]->setMask(normalizeMask);

    QBitmap shadeMask(bsize, bsize);
    shadeMask.clear();
    mask.begin(&shadeMask);
    mask.fillRect(0, 0, bsize, 6, one);
    mask.end();
    for (i = 0; i < NumStates; i++)
	pixmap[P_SHADE * NumStates + i]->setMask(shadeMask);

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

bool B2ClientFactory::reset(unsigned long changed)
{
    bool needsReset = SettingColors ? true : false;
    // TODO Do not recreate decorations if it is not needed. Look at
    // ModernSystem for how to do that
    read_config(this);
    if (changed & SettingFont) {
    	delete_pixmaps();
    	create_pixmaps();
	needsReset = true;
    }
    redraw_pixmaps();
    // For now just return true.
    return needsReset;
}

bool B2ClientFactory::supports(Ability ability)
{
    switch (ability) {
        case AbilityAnnounceButtons:
        case AbilityButtonMenu:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonSpacer:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
        case AbilityButtonResize:
            return true;
        default:
            return false;
    };
}

QList< B2ClientFactory::BorderSize > B2ClientFactory::borderSizes() const
{
    // the list must be sorted
    return QList< BorderSize >() << BorderTiny << BorderNormal <<
	BorderLarge << BorderVeryLarge << BorderHuge;
}

// =====================================

void B2Client::maxButtonClicked()
{
    maximize(button[BtnMax]->last_button);
}

void B2Client::shadeButtonClicked()
{
    setShade(!isSetShade());
}

void B2Client::resizeButtonPressed()
{
    performWindowOperation(ResizeOp);
}

B2Client::B2Client(KDecorationBridge *b, KDecorationFactory *f)
    : KDecoration(b, f), bar_x_ofs(0), in_unobs(0)
{
}

void B2Client::init()
{
    const QString tips[] = {
	i18n("Menu"),
	isOnAllDesktops() ?
	    i18n("Not on all desktops") : i18n("On all desktops"),
	i18n("Minimize"), i18n("Maximize"),
	i18n("Close"), i18n("Help"),
	isSetShade() ? i18n("Unshade") : i18n("Shade"),
	i18n("Resize")
    };

    // Check this early, otherwise the preview will be rendered badly.
    resizable = isResizable();

    createMainWidget(Qt::WResizeNoErase | Qt::WRepaintNoErase);
    widget()->installEventFilter(this);

    widget()->setAttribute(Qt::WA_NoSystemBackground);

    // Set button pointers to NULL so we know what has been created
    for (int i = 0; i < BtnCount; i++)
        button[i] = NULL;

    g = new QGridLayout(widget());
    // Left and right border width

    leftSpacer = new QSpacerItem(thickness, 16,
	    QSizePolicy::Fixed, QSizePolicy::Expanding);
    rightSpacer = new QSpacerItem(thickness, 16,
	    QSizePolicy::Fixed, QSizePolicy::Expanding);

    g->addItem(leftSpacer, 1, 0);
    g->addItem(rightSpacer, 1, 2);

    // Top border height
    topSpacer = new QSpacerItem(10, buttonSize + 4,
	    QSizePolicy::Expanding, QSizePolicy::Fixed);
    g->addItem(topSpacer, 0, 1);

    // Bottom border height.
    bottomSpacer = new QSpacerItem(10,
		thickness + (mustDrawHandle() ? 4 : 0),
		QSizePolicy::Expanding, QSizePolicy::Fixed);
    g->addItem(bottomSpacer, 2, 1);
    if (isPreview()) {
	QLabel *previewLabel = new QLabel(
		i18n("<b><center>B II preview</center></b>"),
		widget());
        g->addWidget(previewLabel, 1, 1);

    } else {
	g->addItem(new QSpacerItem(0, 0), 1, 1);
    }

    // titlebar
    g->addItem( new QSpacerItem(0, buttonSize + 4), 0, 0 );

    titlebar = new B2Titlebar(this);
    titlebar->setMinimumWidth(buttonSize + 4);
    titlebar->setFixedHeight(buttonSize + 4);

    QBoxLayout *titleLayout = new QBoxLayout(QBoxLayout::LeftToRight, titlebar );
    titleLayout->setMargin(1);
    titleLayout->setSpacing(3);

    if (options()->customButtonPositions()) {
        addButtons(options()->titleButtonsLeft(), tips, titlebar, titleLayout);
        titleLayout->addItem(titlebar->captionSpacer);
        addButtons(options()->titleButtonsRight(), tips, titlebar, titleLayout);
    } else {
        addButtons("MSH", tips, titlebar, titleLayout);
        titleLayout->addItem(titlebar->captionSpacer);
        addButtons("IAX", tips, titlebar, titleLayout);
    }

    titleLayout->addSpacing(3);

    QColor c = options()->palette(KDecoration::ColorTitleBar, isActive()).color( QPalette::Active, QPalette::Button );

    for (int i = 0; i < BtnCount; i++) {
        if (button[i])
            button[i]->setBg(c);
    }

    titlebar->updateGeometry();
    positionButtons();
    titlebar->recalcBuffer();
    titlebar->installEventFilter(this);
}

void B2Client::addButtons(const QString& s, const QString tips[],
                          B2Titlebar* tb, QBoxLayout* titleLayout)
{
    if (s.length() <= 0)
	return;

    for (int i = 0; i < s.length(); i++) {
        switch (s[i].toLatin1()) {
	case 'M':  // Menu button
	    if (!button[BtnMenu]) {
		button[BtnMenu] = new B2Button(this, tb, tips[BtnMenu],
			Qt::LeftButton | Qt::RightButton);
		button[BtnMenu]->setPixmaps(P_MENU);
		button[BtnMenu]->setUseMiniIcon();
		connect(button[BtnMenu], SIGNAL(pressed()),
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
		button[BtnIconify] = new B2Button(this, tb,tips[BtnIconify]);
		button[BtnIconify]->setPixmaps(P_ICONIFY);
		connect(button[BtnIconify], SIGNAL(clicked()),
			this, SLOT(minimize()));
		titleLayout->addWidget(button[BtnIconify]);
	    }
	    break;
	case 'A':  // Maximize button
	    if (isMaximizable() && (!button[BtnMax])) {
		button[BtnMax] = new B2Button(this, tb, tips[BtnMax],
			Qt::LeftButton | Qt::MidButton | Qt::RightButton);
		button[BtnMax]->setPixmaps(maximizeMode() == MaximizeFull ?
			P_NORMALIZE : P_MAX);
		connect(button[BtnMax], SIGNAL(clicked()),
			this, SLOT(maxButtonClicked()));
		titleLayout->addWidget(button[BtnMax]);
	    }
	    break;
	case 'X':  // Close button
	    if (isCloseable() && !button[BtnClose]) {
		button[BtnClose] = new B2Button(this, tb, tips[BtnClose]);
		button[BtnClose]->setPixmaps(P_CLOSE);
		connect(button[BtnClose], SIGNAL(clicked()),
			this, SLOT(closeWindow()));
		titleLayout->addWidget(button[BtnClose]);
	    }
	    break;
	case 'L': // Shade button
	    if (isShadeable() && !button[BtnShade]) {
		button[BtnShade] = new B2Button(this, tb, tips[BtnShade]);
		button[BtnShade]->setPixmaps(P_SHADE);
		connect(button[BtnShade], SIGNAL(clicked()),
			this, SLOT(shadeButtonClicked()));
		titleLayout->addWidget(button[BtnShade]);
	    }
	    break;
	case 'R': // Resize button
	    if (resizable && !button[BtnResize]) {
		button[BtnResize] = new B2Button(this, tb, tips[BtnResize]);
		button[BtnResize]->setPixmaps(P_RESIZE);
		connect(button[BtnResize], SIGNAL(pressed()),
			this, SLOT(resizeButtonPressed()));
		titleLayout->addWidget(button[BtnResize]);
	    }
	    break;
	case '_': // Additional spacing
	    titleLayout->addSpacing(4);
	    break;
	}
    }
}

bool B2Client::mustDrawHandle() const
{
    if (drawSmallBorders && (maximizeMode() & MaximizeVertical)) {
	return false;
    } else {
	return do_draw_handle && resizable;
    }
}

void B2Client::iconChange()
{
    if (button[BtnMenu])
        button[BtnMenu]->repaint();
}

// Gallium: New button show/hide magic for customizable
//          button positions.
void B2Client::calcHiddenButtons()
{
    // Hide buttons in this order:
    // Shade, Sticky, Help, Resize, Maximize, Minimize, Close, Menu
    B2Button* btnArray[] = {
	button[BtnShade], button[BtnSticky], button[BtnHelp], button[BtnResize],
	button[BtnMax], button[BtnIconify], button[BtnClose], button[BtnMenu]
    };
    int minWidth = 120;
    int currentWidth = width();
    int count = 0;
    int i;

    // Determine how many buttons we need to hide
    while (currentWidth < minWidth) {
        currentWidth += buttonSize + 1; // Allow for spacer (extra 1pix)
        count++;
    }
    // Bound the number of buttons to hide
    if (count > BtnCount) count = BtnCount;

    // Hide the required buttons
    for (i = 0; i < count; i++) {
        if (btnArray[i] && btnArray[i]->isVisible())
            btnArray[i]->hide();
    }
    // Show the rest of the buttons
    for (i = count; i < BtnCount; i++) {
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
    titlebar->repaint();
}

void B2Client::paintEvent(QPaintEvent* e)
{
    QPainter p(widget());

    KDecoration::ColorType frameColorGroup = colored_frame ?
	KDecoration::ColorTitleBar : KDecoration::ColorFrame;

    QRect t = titlebar->geometry();

    // Frame height, this is used a lot of times
    int fHeight = height() - t.height();

    // distance from the bottom border - it is different if window is resizable
    int bb = mustDrawHandle() ? 4 : 0;
    int bDepth = thickness + bb;

    QPalette fillColor = options()->palette(frameColorGroup, isActive());
    QBrush fillBrush(options()->color(frameColorGroup, isActive()));

    // outer frame rect
    p.drawRect(0, t.bottom() - thickness + 1,
	    width(), fHeight - bb + thickness);

    if (thickness >= 2) {
	// inner window rect
	p.drawRect(thickness - 1, t.bottom(),
		width() - 2 * (thickness - 1), fHeight - bDepth + 2);

	if (thickness >= 3) {
	    // frame shade panel
	    qDrawShadePanel(&p, 1, t.bottom() - thickness + 2,
	    	width() - 2, fHeight - 2 - bb + thickness, fillColor, false);
	    if (thickness == 4) {
		p.setPen(fillColor.color(QPalette::Background));
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
    if (mustDrawHandle()) {
        p.setPen(Qt::black);
	int hx = width() - 40;
	int hw = 40;

	p.drawLine(width() - 1, height() - thickness - 4,
		width() - 1, height() - 1);
	p.drawLine(hx, height() - 1, width() - 1, height() - 1);
	p.drawLine(hx, height() - 4, hx, height() - 1);

	p.fillRect(hx + 1, height() - thickness - 3,
		hw - 2, thickness + 2, fillBrush);

	p.setPen(fillColor.color(QPalette::Dark));
	p.drawLine(width() - 2, height() - thickness - 4,
		width() - 2, height() - 2);
	p.drawLine(hx + 1, height() - 2, width() - 2, height() - 2);

	p.setPen(fillColor.color(QPalette::Light));
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
	QRegion reg(QRect(0, 0, width(), buttonSize + 4));
	reg = reg.intersect(e->region());
	if (!reg.isEmpty())
	    unobscureTitlebar();
    }
}

void B2Client::doShape()
{
    QRect t = titlebar->geometry();
    QRegion mask(widget()->rect());
    // top to the tilebar right
    if (bar_x_ofs) {
	// left from bar
	mask -= QRect(0, 0, bar_x_ofs, t.height() - thickness);
	// top left point
	mask -= QRect(0, t.height() - thickness, 1, 1);
    }
    if (t.right() < width() - 1) {
	mask -= QRect(width() - 1,
		t.height() - thickness, 1, 1); // top right point
	mask -= QRect(t.right() + 1, 0,
		width() - t.right() - 1, t.height() - thickness);
    }
    // bottom right point
    mask -= QRect(width() - 1, height() - 1, 1, 1);
    if (mustDrawHandle()) {
	// bottom left point
	mask -= QRect(0, height() - 5, 1, 1);
	// handle left point
	mask -= QRect(width() - 40, height() - 1, 1, 1);
	// bottom left
	mask -= QRect(0, height() - 4, width() - 40, 4);
    } else {
	// bottom left point
	mask -= QRect(0, height() - 1, 1, 1);
    }

    setMask(mask);
}

void B2Client::showEvent(QShowEvent *)
{
    calcHiddenButtons();
    positionButtons();
    doShape();
}

KDecoration::Position B2Client::mousePosition(const QPoint& p) const
{
    const int range = 16;
    QRect t = titlebar->geometry();
    t.setHeight(buttonSize + 4 - thickness);
    int ly = t.bottom();
    int lx = t.right();
    int bb = mustDrawHandle() ? 0 : 5;

    if (p.x() > t.right()) {
        if (p.y() <= ly + range && p.x() >= width() - range)
            return PositionTopRight;
        else if (p.y() <= ly + thickness)
            return PositionTop;
    } else if (p.x() < bar_x_ofs) {
        if (p.y() <= ly + range && p.x() <= range)
            return PositionTopLeft;
        else if (p.y() <= ly + thickness)
            return PositionTop;
    } else if (p.y() < ly) {
        if (p.x() > bar_x_ofs + thickness &&
		p.x() < lx - thickness && p.y() > thickness)
            return KDecoration::mousePosition(p);
        if (p.x() > bar_x_ofs + range && p.x() < lx - range)
            return PositionTop;
        if (p.y() <= range) {
            if (p.x() <= bar_x_ofs + range)
                return PositionTopLeft;
            else return PositionTopRight;
        } else {
            if (p.x() <= bar_x_ofs + range)
                return PositionLeft;
            else return PositionRight;
        }
    }

    if (p.y() >= height() - 8 + bb) {
        /* the normal Client:: only wants border of 4 pixels */
	if (p.x() <= range) return PositionBottomLeft;
	if (p.x() >= width() - range) return PositionBottomRight;
	return PositionBottom;
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
	widget()->repaint(0, 0, width(), buttonSize + 4);
	titlebar->repaint();
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
	b->setToolTip(
		on ? i18n("Not on all desktops") : i18n("On all desktops"));
    }
}

void B2Client::maximizeChange()
{
    bool m = maximizeMode() == MaximizeFull;
    if (button[BtnMax]) {
        button[BtnMax]->setPixmaps(m ? P_NORMALIZE : P_MAX);
        button[BtnMax]->repaint();
	button[BtnMax]->setToolTip(
		m ? i18n("Restore") : i18n("Maximize"));
    }
    bottomSpacer->changeSize(10, thickness + (mustDrawHandle() ? 4 : 0),
	    QSizePolicy::Expanding, QSizePolicy::Minimum);

    g->activate();
    doShape();
    widget()->repaint();
}

void B2Client::activeChange()
{
    widget()->repaint();
    titlebar->repaint();

    QColor c = options()->palette(
	    KDecoration::ColorTitleBar, isActive()).color(QPalette::Active, QPalette::Button);

    for (int i = 0; i < BtnCount; i++)
        if (button[i]) {
           button[i]->setBg(c);
           button[i]->repaint();
        }
}

void B2Client::shadeChange()
{
    bottomSpacer->changeSize(10, thickness + (mustDrawHandle() ? 4 : 0),
	    QSizePolicy::Expanding, QSizePolicy::Minimum);
    g->activate();
    doShape();
    if (B2Button *b = button[BtnShade]) {
	b->setToolTip(isSetShade() ? i18n("Unshade") : i18n("Shade"));
    }
}

QSize B2Client::minimumSize() const
{
    int left, right, top, bottom;
    borders(left, right, top, bottom);
    return QSize(left + right + 2 * buttonSize, top + bottom);
}

void B2Client::resize(const QSize& s)
{
    widget()->resize(s);
}

void B2Client::borders(int &left, int &right, int &top, int &bottom) const
{
    left = right = thickness;
    top = buttonSize + 4;
    bottom = thickness + (mustDrawHandle() ? 4 : 0);
}

void B2Client::menuButtonPressed()
{
    static B2Client *lastClient = NULL;

    bool dbl = (lastClient == this &&
	        time.elapsed() <= QApplication::doubleClickInterval());
    lastClient = this;
    time.start();
    if (!dbl) {
	KDecorationFactory* f = factory();
	QRect menuRect = button[BtnMenu]->rect();
	QPoint menuTop = button[BtnMenu]->mapToGlobal(menuRect.topLeft());
	QPoint menuBottom = button[BtnMenu]->mapToGlobal(menuRect.bottomRight());
	showWindowMenu(QRect(menuTop, menuBottom));
	if (!f->exists(this)) // 'this' was destroyed
	    return;
	button[BtnMenu]->setDown(false);
    } else {
	switch (menu_dbl_click_op) {
	case B2::MinimizeOp:
	    minimize();
	    break;
	case B2::ShadeOp:
	    setShade(!isSetShade());
	    break;
	case B2::CloseOp:
	    closeWindow();
	    break;
	case B2::NoOp:
	default:
	    break;
	}
    }
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
    QRegion reg(QRect(0, 0, width(), buttonSize + 4));
    reg = unobscuredRegion(reg);
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
    int i;
    QPalette aGrp = options()->palette(KDecoration::ColorButtonBg, true);
    QPalette iGrp = options()->palette(KDecoration::ColorButtonBg, false);

    // close
    drawB2Rect(PIXMAP_A(P_CLOSE), aGrp.color( QPalette::Button ), false);
    drawB2Rect(PIXMAP_AH(P_CLOSE), aGrp.color( QPalette::Button ), true);
    drawB2Rect(PIXMAP_AD(P_CLOSE), aGrp.color( QPalette::Button ), true);

    drawB2Rect(PIXMAP_I(P_CLOSE), iGrp.color( QPalette::Button ), false);
    drawB2Rect(PIXMAP_IH(P_CLOSE), iGrp.color( QPalette::Button ), true);
    drawB2Rect(PIXMAP_ID(P_CLOSE), iGrp.color( QPalette::Button ), true);

    // shade
    QPixmap thinBox(buttonSize - 2, 6);
    for (i = 0; i < NumStates; i++) {
	bool is_act = (i < 2);
	bool is_down = ((i & 1) == 1);
	QPixmap *pix = pixmap[P_SHADE * NumStates + i];
	QColor color = is_act ? aGrp.color( QPalette::Button ) : iGrp.color( QPalette::Button );
	drawB2Rect(&thinBox, color, is_down);
	pix->fill(Qt::black);
	bitBlt(pix, 0, 0, &thinBox,
		0, 0, thinBox.width(), thinBox.height());
    }

    // maximize
    for (i = 0; i < NumStates; i++) {
	*pixmap[P_MAX * NumStates + i] = *pixmap[P_CLOSE * NumStates + i];
	pixmap[P_MAX * NumStates + i]->detach();
    }

    // normalize + iconify
    QPixmap smallBox( 10, 10 );
    QPixmap largeBox( 12, 12 );

    for (i = 0; i < NumStates; i++) {
	bool is_act = (i < 3);
	bool is_down = (i == Down || i == IDown);
	QPixmap *pix = pixmap[P_NORMALIZE * NumStates + i];
	drawB2Rect(&smallBox, is_act ? aGrp.color( QPalette::Button ) : iGrp.color( QPalette::Button ), is_down);
	drawB2Rect(&largeBox, is_act ? aGrp.color( QPalette::Button ) : iGrp.color( QPalette::Button ), is_down);
	pix->fill(options()->color(KDecoration::ColorTitleBar, is_act));
	bitBlt(pix, pix->width() - 12, pix->width() - 12, &largeBox,
	       0, 0, 12, 12);
	bitBlt(pix, 0, 0, &smallBox, 0, 0, 10, 10);

	bitBlt(pixmap[P_ICONIFY * NumStates + i], 0, 0,
	       &smallBox, 0, 0, 10, 10);
    }

    // resize
    for (i = 0; i < NumStates; i++) {
	bool is_act = (i < 3);
	bool is_down = (i == Down || i == IDown);
	*pixmap[P_RESIZE * NumStates + i] = *pixmap[P_CLOSE * NumStates + i];
	pixmap[P_RESIZE * NumStates + i]->detach();
	drawB2Rect(&smallBox, is_act ? aGrp.color( QPalette::Button ) : iGrp.color( QPalette::Button ), is_down);
	bitBlt(pixmap[P_RESIZE * NumStates + i],
		0, 0, &smallBox, 0, 0, 10, 10);
    }


    QPainter p;
    // x for close + menu + help
    for (int j = 0; j < 3; j++) {
        int pix;
        unsigned const char *light, *dark;
        switch (j) {
        case 0:
            pix = P_CLOSE; light = close_white_bits; dark = close_dgray_bits;
            break;
        case 1:
            pix = P_MENU; light = menu_white_bits; dark = menu_dgray_bits;
            break;
        default:
            pix = P_HELP; light = help_light_bits; dark = help_dark_bits;
            break;
        }
	int off = (pixmap[pix * NumStates]->width() - 16) / 2;
        for (i = 0; i < NumStates; i++) {
            p.begin(pixmap[pix * NumStates + i]);
            kColorBitmaps(&p, (i < 3) ? aGrp : iGrp, off, off, 16, 16, true,
                          light, NULL, NULL, dark, NULL, NULL);
            p.end();
        }
    }

    // pin
    for (i = 0; i < NumStates; i++) {
	bool isDown = (i == Down || i == IDown);
        unsigned const char *white = isDown ? pindown_white_bits : pinup_white_bits;
        unsigned const char *gray = isDown ? pindown_gray_bits : pinup_gray_bits;
        unsigned const char *dgray =isDown ? pindown_dgray_bits : pinup_dgray_bits;
        p.begin(pixmap[P_PINUP * NumStates + i]);
        kColorBitmaps(&p, (i < 3) ? aGrp : iGrp, 0, 0, 16, 16, true, white,
                      gray, NULL, dgray, NULL, NULL);
        p.end();
    }

    // Apply the hilight effect to the 'Hover' icons
    KIconEffect ie;
    QPixmap hilighted;
    for (i = 0; i < P_NUM_BUTTON_TYPES; i++) {
	int offset = i * NumStates;
	hilighted = ie.apply(*pixmap[offset + Norm],
		K3Icon::Small, K3Icon::ActiveState);
	*pixmap[offset + Hover] = hilighted;

	hilighted = ie.apply(*pixmap[offset + INorm],
		K3Icon::Small, K3Icon::ActiveState);
	*pixmap[offset + IHover] = hilighted;
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
		    titleGradient[i] = new QPixmap;
		}
		*titleGradient[i] = QPixmap(64, buttonSize + 3);
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
    if (cap.length() < 5) // make sure the titlebar has sufficiently wide
        cap = "XXXXX";    // area for dragging the window
    int textLen = fm.width(cap);

    QRect t = titlebar->captionSpacer->geometry();
    int titleWidth = titlebar->width() - t.width() + textLen + 2;
    if (titleWidth > width()) titleWidth = width();

    titlebar->resize(titleWidth, buttonSize + 4);
    titlebar->move(bar_x_ofs, 0);
}

// Transparent bound stuff.

static QRect *visible_bound;
static QPolygon bound_shape;

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
        // line width is 5 pixels, so compensate for the 2 outer pixels (#88657)
        QRect g = geom;
        g.setLeft(g.left() + 2);
        g.setTop(g.top() + 2);
        g.setRight(g.right() - 2);
        g.setBottom(g.bottom() - 2);
        frameTop += 2;
        barLeft += 2;
        barRight -= 2;

	bound_shape.putPoints(0, 8,
		g.left(), frameTop,
		barLeft, frameTop,
		barLeft, g.top(),
		barRight, g.top(),
		barRight, frameTop,
		g.right(), frameTop,
		g.right(), g.bottom(),
		g.left(), g.bottom());
    } else {
	*visible_bound = geom;
    }
/**
 * TODO: Replace by QRubberBand
 *   QPainter p(workspaceWidget());
 *   p.setPen(QPen(Qt::white, 5));
 *   p.setRasterOp(Qt::XorROP);
 *   p.drawPolygon(bound_shape);
 */
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

B2Button::B2Button(B2Client *_client, QWidget *parent,
	const QString& tip, const int realizeBtns)
   : Q3Button(parent, 0), hover(false)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setCursor(Qt::ArrowCursor);
    realizeButtons = realizeBtns;
    client = _client;
    useMiniIcon = false;
    setFixedSize(buttonSize, buttonSize);
    this->setToolTip(tip);
}


QSize B2Button::sizeHint() const
{
    return QSize(buttonSize, buttonSize);
}

QSizePolicy B2Button::sizePolicy() const
{
    return(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
}

void B2Button::drawButton(QPainter *p)
{
    QPixmap* gradient = titleGradient[client->isActive() ? 0 : 1];
    if (gradient) {
	p->drawTiledPixmap(0, 0, buttonSize, buttonSize, *gradient, 0, 2);
    } else {
	p->fillRect(rect(), bg);
    }
    if (useMiniIcon) {
        QPixmap miniIcon = client->icon().pixmap(
		style()->pixelMetric(QStyle::PM_SmallIconSize),
		client->isActive() ? QIcon::Normal : QIcon::Disabled);
        p->drawPixmap((width() - miniIcon.width()) / 2,
                      (height() - miniIcon.height()) / 2, miniIcon);
    } else {
	int type;
        if (client->isActive()) {
            if (isChecked() || isDown())
		type = Down;
            else if (hover)
		type = Hover;
	    else
		type = Norm;
        } else {
            if (isChecked() || isDown())
		type = IDown;
	    else if (hover)
		type = IHover;
            else
		type = INorm;
        }
	p->drawPixmap((width() - icon[type]->width()) / 2,
		(height() - icon[type]->height()) / 2, *icon[type]);
    }
}

void B2Button::setPixmaps(int button_id)
{
    button_id *= NumStates;
    for (int i = 0; i < NumStates; i++) {
	icon[i] = B2::pixmap[button_id + i];
    }
    repaint();
}

void B2Button::mousePressEvent(QMouseEvent * e)
{
    last_button = e->button();
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
	    (e->button() & realizeButtons) ? Qt::LeftButton : Qt::NoButton,
	    (e->button() & realizeButtons) ? Qt::LeftButton : Qt::NoButton,
	    e->modifiers());
    Q3Button::mousePressEvent(&me);
}

void B2Button::mouseReleaseEvent(QMouseEvent * e)
{
    last_button = e->button();
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
	    (e->button() & realizeButtons) ? Qt::LeftButton : Qt::NoButton,
	    (e->button() & realizeButtons) ? Qt::LeftButton : Qt::NoButton,
	    e->modifiers());
    Q3Button::mouseReleaseEvent(&me);
}

void B2Button::enterEvent(QEvent *e)
{
    hover = true;
    repaint();
    Q3Button::enterEvent(e);
}

void B2Button::leaveEvent(QEvent *e)
{
    hover = false;
    repaint();
    Q3Button::leaveEvent(e);
}

// =====================================

B2Titlebar::B2Titlebar(B2Client *parent)
    : QWidget(parent->widget(), Qt::WStyle_Customize | Qt::WNoAutoErase),
      client(parent),
      set_x11mask(false), isfullyobscured(false), shift_move(false)
{
    setAttribute(Qt::WA_NoSystemBackground);
    captionSpacer = new QSpacerItem(buttonSize, buttonSize + 4,
	    QSizePolicy::Expanding, QSizePolicy::Fixed);
}

bool B2Titlebar::x11Event(XEvent *e)
{
    if (!set_x11mask) {
	set_x11mask = true;
	XSelectInput(QX11Info::display(), winId(),
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
    switch (e->type) {
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
    QPixmap* gradient = titleGradient[state ? 0 : 1];

    QRect t = rect();
    // black titlebar frame
    p.setPen(Qt::black);
    p.drawLine(0, 0, 0, t.bottom());
    p.drawLine(0, 0, t.right(), 0);
    p.drawLine(t.right(), 0, t.right(), t.bottom());

    // titlebar fill
    const QPalette cg = options()->palette(KDecoration::ColorTitleBar, state);
    QBrush brush(cg.background());
    if (gradient) brush.setTexture(*gradient);
    qDrawShadeRect(&p, 1, 1, t.right() - 1, t.height() - 1,
		   cg, false, 1, 0, &brush);

    // and the caption
    p.setPen(options()->color(KDecoration::ColorFont, state));
    p.setFont(options()->font(state));
    t = captionSpacer->geometry();
    p.drawText(t, Qt::AlignLeft | Qt::AlignVCenter, client->caption());
}

void B2Titlebar::recalcBuffer()
{
    titleBuffer = QPixmap(width(), height());

    QPainter p(&titleBuffer);
    drawTitlebar(p, true);
    oldTitle = windowTitle();
}

void B2Titlebar::resizeEvent(QResizeEvent *)
{
    recalcBuffer();
    repaint();
}


void B2Titlebar::paintEvent(QPaintEvent *)
{
    if (client->isActive())
        bitBlt(this, 0, 0, &titleBuffer, 0, 0, titleBuffer.width(),
               titleBuffer.height());
    else {
        QPainter p(this);
	drawTitlebar(p, false);
    }
}

void B2Titlebar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && e->y() < height()) {
	client->titlebarDblClickOperation();
    }
}

void B2Titlebar::mousePressEvent(QMouseEvent * e)
{
    shift_move = e->modifiers() & Qt::ShiftModifier;
    if (shift_move) {
        moveOffset = e->globalPos();
    } else {
	e->ignore();
    }
}

void B2Titlebar::mouseReleaseEvent(QMouseEvent * e)
{
    if (shift_move) shift_move = false;
    else e->ignore();
}

void B2Titlebar::mouseMoveEvent(QMouseEvent * e)
{
    if (shift_move) {
	int oldx = mapFromGlobal(moveOffset).x();
        int xdiff = e->globalPos().x() - moveOffset.x();
        moveOffset = e->globalPos();
	if (oldx >= 0 && oldx <= rect().right()) {
            client->titleMoveRel(xdiff);
	}
    } else {
    	e->ignore();
    }
}

} // namespace B2

#include "b2client.moc"

// vim: sw=4 ts=8

#endif // CLIENTS/B2/B2CLIENT.CPP
