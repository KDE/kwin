/********************************************************************
  Laptop KWin Decoration
 
  Copyright (c) 2005 Sandro Giessl <sandro@giessl.com>
  Port of this decoration to KDE 3.2, accessibility enhancement are
  Copyright (c) 2003 Luciano Montanaro <mikelima@cirulla.net>
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include <kconfig.h> // up here to avoid X11 header conflict :P
#include "laptopclient.h"
#include <QPixmap>
#include <QPaintEvent>
#include <QBitmap>
#include <QPainter>
#include <kglobal.h>
#include <klocale.h>

namespace Laptop {

static const unsigned char iconify_bits[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18};

static const unsigned char close_bits[] = {
    0x42, 0xe7, 0x7e, 0x3c, 0x3c, 0x7e, 0xe7, 0x42};

static const unsigned char maximize_bits[] = {
    0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0xff, 0xff };

static const unsigned char r_minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};

static const unsigned char l_minmax_bits[] = {
    0x30, 0x18, 0xcc, 0xe6, 0xf3, 0xf9, 0xfc, 0xfc};

static const unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};

static const unsigned char unsticky_bits[] = {
   0x3c, 0x42, 0x99, 0xbd, 0xbd, 0x99, 0x42, 0x3c};

static const unsigned char sticky_bits[] = {
   0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c};

static QPixmap *titlePix;
static QPixmap *aUpperGradient;
static QPixmap *iUpperGradient;
// buttons active, inactive, up, down, and 2 sizes :P
static QPixmap *btnPix1;
static QPixmap *iBtnPix1;
static QPixmap *btnDownPix1;
static QPixmap *iBtnDownPix1;
static QPixmap *btnPix2;
static QPixmap *btnDownPix2;
static QPixmap *iBtnPix2;
static QPixmap *iBtnDownPix2;
static QColor btnForeground;

static int titleHeight = 14;
static int btnWidth1 = 17;
static int btnWidth2 = 27;

static int handleSize = 8;	// the resize handle size in pixels

static bool pixmaps_created = false;

// =====================================

KWIN_DECORATION(Laptop::LaptopClientFactory)


// =====================================

static inline const KDecorationOptions* options()
{
    return KDecoration::options();
}

static void gradientFill(QPixmap *pixmap, const QColor &color1, const QColor &color2, bool diagonal = false)
{
	QPainter p(pixmap);
	QLinearGradient gradient(0, 0, diagonal ? pixmap->width() : 0, pixmap->height());
	gradient.setColorAt(0.0, color1);
	gradient.setColorAt(1.0, color2);
	QBrush brush(gradient);
	p.fillRect(pixmap->rect(), brush);
}

static void drawButtonFrame(QPixmap *pix, const QPalette &g, bool sunken)
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
        p.setPen(g.color(QPalette::Dark ));
        p.drawRect(0, 0, w-2, h-2);
        p.setPen(g.color(QPalette::Light));
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

    titleHeight = QFontMetrics(options()->font(true)).height();
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
    titlePix->fill( Qt::transparent );
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
        aUpperGradient = new QPixmap(32, titleHeight+2);
        iUpperGradient = new QPixmap(32, titleHeight+2);
        QColor bgColor = options()->color(KDecoration::ColorTitleBar, true);
	gradientFill(aUpperGradient, bgColor.light(120), bgColor.dark(120));
        bgColor = options()->color(KDecoration::ColorTitleBar, false);
	gradientFill(iUpperGradient, bgColor.light(120), bgColor.dark(120));
    }
    // buttons (active/inactive, sunken/unsunken, 2 sizes each)
    QPalette g = options()->palette(KDecoration::ColorButtonBg, true);
    g.setCurrentColorGroup( QPalette::Active );
    QColor c = g.color( QPalette::Background );
    btnPix1 = new QPixmap(btnWidth1, titleHeight);
    btnDownPix1 = new QPixmap(btnWidth1, titleHeight);
    btnPix2 = new QPixmap(btnWidth2, titleHeight);
    btnDownPix2 = new QPixmap(btnWidth2, titleHeight);
    iBtnPix1 = new QPixmap(btnWidth1, titleHeight);
    iBtnDownPix1 = new QPixmap(btnWidth1, titleHeight);
    iBtnPix2 = new QPixmap(btnWidth2, titleHeight);
    iBtnDownPix2 = new QPixmap(btnWidth2, titleHeight);
    if(QPixmap::defaultDepth() > 8){
        gradientFill(btnPix1, c.light(120), c.dark(130), true);
        gradientFill(btnPix2, c.light(120), c.dark(130), true);
        gradientFill(btnDownPix1, c.light(120), c.dark(130), true);
        gradientFill(btnDownPix2, c.light(120), c.dark(130), true);
        g = options()->palette(KDecoration::ColorButtonBg, false);
        g.setCurrentColorGroup( QPalette::Active );
        c = g.color(QPalette::Background);
        gradientFill(iBtnPix1, c.light(120), c.dark(130), true);
        gradientFill(iBtnPix2, c.light(120), c.dark(130), true);
        gradientFill(iBtnDownPix1, c.light(120), c.dark(130), true);
        gradientFill(iBtnDownPix2, c.light(120), c.dark(130), true);
    }
    else{
        btnPix1->fill(c.rgb());
        btnDownPix1->fill(c.rgb());
        btnPix2->fill(c.rgb());
        btnDownPix2->fill(c.rgb());
        g = options()->palette(KDecoration::ColorButtonBg, false);
        g.setCurrentColorGroup( QPalette::Active );
        c = g.background().color();
        iBtnPix1->fill(c.rgb());
        iBtnDownPix1->fill(c.rgb());
        iBtnPix2->fill(c.rgb());
        iBtnDownPix2->fill(c.rgb());
    }
    g = options()->palette(KDecoration::ColorButtonBg, true);
    g.setCurrentColorGroup( QPalette::Active );
    c = g.background().color();
    drawButtonFrame(btnPix1, g, false);
    drawButtonFrame(btnDownPix1, g, true);
    drawButtonFrame(btnPix2, g, false);
    drawButtonFrame(btnDownPix2, g, true);
    g = options()->palette(KDecoration::ColorButtonBg, false);
    g.setCurrentColorGroup( QPalette::Active );
    c = g.background().color();
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

LaptopButton::LaptopButton(ButtonType type, LaptopClient *parent, const char *name)
    : KCommonDecorationButton(type, parent)
{
    Q_UNUSED( name );
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void LaptopButton::reset(unsigned long changed)
{
    if (changed&DecorationReset || changed&ManualReset || changed&SizeChange || changed&StateChange) {
        switch (type() ) {
            case CloseButton:
                setBitmap(close_bits);
                break;
            case HelpButton:
                setBitmap(question_bits);
                break;
            case MinButton:
                setBitmap(iconify_bits);
                break;
            case MaxButton:
                if (isChecked() ) {
                    setBitmap(isLeft() ? l_minmax_bits : r_minmax_bits);
                } else {
                    setBitmap(maximize_bits);
                }
                break;
            case OnAllDesktopsButton:
                setBitmap( isChecked() ? unsticky_bits : sticky_bits );
                break;
            default:
                setBitmap(0);
                break;
        }

        this->update();
    }
}

void LaptopButton::setBitmap(const unsigned char *bitmap)
{
    if (bitmap)
        deco = QBitmap::fromData( QSize(8,  8),  bitmap);
    else {
        deco = QBitmap(8,8);
        deco.fill(Qt::color0);
    }
    deco.setMask(deco);
    repaint();
}

void LaptopButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    drawButton(&p);
}

void LaptopButton::drawButton(QPainter *p)
{
    bool smallBtn = width() == btnWidth1;
    if(btnPix1){
        if(decoration()->isActive()){
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
        QPalette g = options()->palette(KDecoration::ColorButtonBg, decoration()->isActive());
        g.setCurrentColorGroup( QPalette::Active );
        int w = width();
        int h = height();
        p->fillRect(1, 1, w-2, h-2, isDown() ? g.color(QPalette::Mid) : g.color(QPalette::Button) );
        p->setPen(isDown() ? g.color( QPalette::Dark ) : g.color( QPalette::Light ));
        p->drawLine(0, 0, w-1, 0);
        p->drawLine(0, 0, 0, w-1);
        p->setPen(isDown() ? g.color( QPalette::Light ) : g.color( QPalette::Dark ));
        p->drawLine(w-1, 0, w-1, h-1);
        p->drawLine(0, h-1, w-1, h-1);
    }

    QPainterPath path;
    path.addRegion( deco );
    
    QPoint offset( (width()-8)/2, (height()-8)/2 );
    if( isDown() ) offset += QPoint( 1, 1 );
    p->translate( offset );
    p->setPen( Qt::NoPen );
    p->setBrush( btnForeground );
    p->drawPath( path );
    
}

// =====================================

void LaptopClient::reset(unsigned long changed)
{
    KCommonDecoration::reset(changed);
}

LaptopClient::LaptopClient(KDecorationBridge *b, KDecorationFactory *f)
    : KCommonDecoration(b, f)
{
}

LaptopClient::~LaptopClient()
{
}

QString LaptopClient::visibleName() const
{
    return i18n("Laptop");
}

QString LaptopClient::defaultButtonsLeft() const
{
    return "X";
}

QString LaptopClient::defaultButtonsRight() const
{
    return "HSIA";
}

bool LaptopClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
        case DB_MenuClose:
            return false;

        case DB_WindowMask:
            return true;

        case DB_ButtonHide:
            return true;

        default:
            return KCommonDecoration::decorationBehaviour(behaviour);
    }
}

int LaptopClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
    switch (lm) {
        case LM_TitleEdgeLeft:
        case LM_TitleEdgeRight:
        case LM_BorderLeft:
        case LM_BorderRight:
            return 4;

        case LM_BorderBottom:
            return mustDrawHandle() ? handleSize : 4;

        case LM_TitleEdgeTop:
            return 3;

        case LM_TitleEdgeBottom:
            return 1;

        case LM_TitleBorderLeft:
        case LM_TitleBorderRight:
            return 0;

        case LM_ButtonWidth:
        {
            if (btn && (btn->type()==HelpButton||btn->type()==OnAllDesktopsButton) ) {
                return btnWidth1;
            } else {
                return btnWidth2;
            }
        }

        case LM_ButtonHeight:
        case LM_TitleHeight:
            if (isToolWindow() )
                return titleHeight-2;
            else
                return titleHeight;

        case LM_ButtonSpacing:
            return 0;

        default:
            return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
}

KCommonDecorationButton *LaptopClient::createButton(ButtonType type)
{
    switch (type) {
        case OnAllDesktopsButton:
            return new LaptopButton(OnAllDesktopsButton, this, "on_all_desktops");

        case HelpButton:
            return new LaptopButton(HelpButton, this, "help");

        case MinButton:
            return new LaptopButton(MinButton, this, "minimize");

        case MaxButton:
            return new LaptopButton(MaxButton, this, "maximize");

        case CloseButton:
            return new LaptopButton(CloseButton, this, "close");

        default:
            return 0;
    }
}

void LaptopClient::init()
{
    bufferDirty = true;

    KCommonDecoration::init();
}

void LaptopClient::captionChange()
{
    bufferDirty = true;

    KCommonDecoration::captionChange();
}

void LaptopClient::paintEvent( QPaintEvent* )
{
    QPainter p(widget());
    QPalette g = options()->palette(KDecoration::ColorFrame, isActive());
    g.setCurrentColorGroup( QPalette::Active );

    QRect r(widget()->rect());
    p.setPen(Qt::black);
    p.drawRect(r.adjusted(0, 0, -1, -1));

    // fill mid frame...
    p.setPen(g.background().color());
    p.drawLine(r.x()+2, r.y()+2, r.right()-2, r.y()+2);
    p.drawLine(r.left()+2, r.y()+3, r.left()+2, r.bottom()-layoutMetric(LM_BorderBottom)+1);
    p.drawLine(r.right()-2, r.y()+3, r.right()-2, r.bottom()-layoutMetric(LM_BorderBottom)+1);
    p.drawLine(r.left()+3, r.y()+3, r.left()+3, r.y()+layoutMetric(LM_TitleEdgeTop)+layoutMetric(LM_TitleHeight)+layoutMetric(LM_TitleEdgeTop) );
    p.drawLine(r.right()-3, r.y()+3, r.right()-3, r.y()+layoutMetric(LM_TitleEdgeTop)+layoutMetric(LM_TitleHeight)+layoutMetric(LM_TitleEdgeTop) );
    if (!mustDrawHandle() )
        p.drawLine(r.left()+1, r.bottom()-2, r.right()-1, r.bottom()-2);

    // outer frame
    p.setPen(g.color(QPalette::Light));
    p.drawLine(r.x()+1, r.y()+1, r.right()-1, r.y()+1);
    p.drawLine(r.x()+1, r.y()+1, r.x()+1, r.bottom()-1);
    p.setPen(g.dark().color());
    p.drawLine(r.right()-1, r.y()+1, r.right()-1, r.bottom()-1);
    p.drawLine(r.x()+1, r.bottom()-1, r.right()-1, r.bottom()-1);

    int th = titleHeight;
    int bb = handleSize + 2; // Bottom border
    int bs = handleSize - 2; // inner size of bottom border
    if (!mustDrawHandle()) {
	bb = 6;
	bs = 0;
    }
    if ( isToolWindow() )
	th -= 2;

    // inner rect
    p.drawRect(r.x() + 3, r.y() + th + 3, r.width() - 7, r.height() - th - bb - 1);

    // handles
    if (mustDrawHandle()) {
	if (r.width() > 3*handleSize + 20) {
	    int range = 8 + 3*handleSize/2;
	    qDrawShadePanel(&p, r.x() + 1, r.bottom() - bs, range,
                            handleSize - 2, g, false, 1, &g.brush(QPalette::Mid));
	    qDrawShadePanel(&p, r.x() + range + 1, r.bottom() - bs,
		    r.width() - 2*range - 2, handleSize - 2, g, false, 1,
		    isActive() ? &g.brush(QPalette::Background) :
				 &g.brush(QPalette::Mid));
	    qDrawShadePanel(&p, r.right() - range, r.bottom() - bs,
		    range, bs, g, false, 1, &g.brush(QPalette::Mid));
	} else {
	    qDrawShadePanel(&p, r.x() + 1, r.bottom() - bs,
		    r.width() - 2, bs, g, false, 1,
		    isActive() ?  &g.brush(QPalette::Background) :
				  &g.brush(QPalette::Mid));
	}
    }

    r = titleRect();

    if(isActive()){
        updateActiveBuffer();
        p.drawPixmap(r.x(), r.y(), activeBuffer);
        p.setPen(g.background().color());
        p.drawPoint(r.x(), r.y());
        p.drawPoint(r.right(), r.y());
        p.drawLine(r.right()+1, r.y(), r.right()+1, r.bottom());
    }
    else{
        if(iUpperGradient)
            p.drawTiledPixmap(r.x(), r.y(), r.width(), r.height()-1,
                              *iUpperGradient);
        else
            p.fillRect(r.x(), r.y(), r.width(), r.height()-1,
                       options()->color(KDecoration::ColorTitleBar, false));

        p.setFont(options()->font(false, isToolWindow() ));
        QFontMetrics fm(options()->font(false));
        g = options()->palette(KDecoration::ColorTitleBar, false);
        g.setCurrentColorGroup( QPalette::Active );
        if(iUpperGradient)
            p.drawTiledPixmap(r.x()+((r.width()-fm.width(caption()))/2)-4,
                              r.y(), fm.width(caption())+8, r.height()-1,
                              *iUpperGradient);
        else
            p.fillRect(r.x()+((r.width()-fm.width(caption()))/2)-4, r.y(),
                       fm.width(caption())+8, r.height()-1,
                       g.brush(QPalette::Background));
        p.setPen(g.mid().color());
        p.drawLine(r.x(), r.y(), r.right(), r.y());
        p.drawLine(r.x(), r.y(), r.x(), r.bottom());
        p.setPen(g.color(QPalette::Button));
        p.drawLine(r.right(), r.y(), r.right(), r.bottom());
        p.drawLine(r.x(), r.bottom(), r.right(), r.bottom());
        p.setPen(options()->color(KDecoration::ColorFont, false));
        p.drawText(r.x(), r.y()+1, r.width(), r.height()-1,
                   Qt::AlignCenter, caption() );
        g = options()->palette(KDecoration::ColorFrame, true);
        g.setCurrentColorGroup( QPalette::Active );
        p.setPen(g.background().color());
        p.drawPoint(r.x(), r.y());
        p.drawPoint(r.right(), r.y());
        p.drawLine(r.right()+1, r.y(), r.right()+1, r.bottom());
    }
}

QRegion LaptopClient::cornerShape(WindowCorner corner)
{
    switch (corner) {
        case WC_TopLeft:
            return QRect(0, 0, 1, 1);

        case WC_TopRight:
            return QRect(width()-1, 0, 1, 1);

        case WC_BottomLeft:
            return QRect(0, height()-1, 1, 1);

        case WC_BottomRight:
            return QRect(width()-1, height()-1, 1, 1);

        default:
            return QRegion();
    }

}

bool LaptopClient::mustDrawHandle() const
{
    bool drawSmallBorders = !options()->moveResizeMaximizedWindows();
    if (drawSmallBorders && (maximizeMode() & MaximizeVertical)) {
	return false;
    } else {
	return isResizable();
    }
}

void LaptopClient::updateActiveBuffer( )
{
    QRect rTitle = titleRect();
    if( !bufferDirty && (lastBufferWidth == rTitle.width()))
        return;
    if ( rTitle.width() <= 0 || rTitle.height() <= 0 )
	return;
    lastBufferWidth = rTitle.width();
    bufferDirty = false;

    activeBuffer = QPixmap(rTitle.width(), rTitle.height());
    QPainter p;
    QRect r(0, 0, activeBuffer.width(), activeBuffer.height());
    p.begin(&activeBuffer);
    if(aUpperGradient){
        p.drawTiledPixmap(r, *aUpperGradient);
    }
    else{
        p.fillRect(r, options()->color(KDecoration::ColorTitleBar, true));
    }
    if(titlePix)
        p.drawTiledPixmap(r, *titlePix);

    p.setFont(options()->font(true, isToolWindow() ));
    QFontMetrics fm(options()->font(true));
    QPalette g = options()->palette(KDecoration::ColorTitleBar, true);
    g.setCurrentColorGroup( QPalette::Active );
    if(aUpperGradient)
        p.drawTiledPixmap(r.x()+((r.width()-fm.width(caption()))/2)-4,
                          r.y(), fm.width(caption())+8, r.height()-1,
                          *aUpperGradient);
    else
        p.fillRect(r.x()+((r.width()-fm.width(caption()))/2)-4, 0,
                   fm.width(caption())+8, r.height(),
                   g.brush(QPalette::Background));
    p.setPen(g.mid().color());
    p.drawLine(r.x(), r.y(), r.right(), r.y());
    p.drawLine(r.x(), r.y(), r.x(), r.bottom());
    p.setPen(g.color(QPalette::Button));
    p.drawLine(r.right(), r.y(), r.right(), r.bottom());
    p.drawLine(r.x(), r.bottom(), r.right(), r.bottom());
    p.setPen(options()->color(KDecoration::ColorFont, true));
    p.drawText(r.x(), r.y()+1, r.width(), r.height()-1,
               Qt::AlignCenter, caption() );
    g = options()->palette(KDecoration::ColorFrame, true);
    g.setCurrentColorGroup( QPalette::Active );
    p.setPen(g.background().color());
    p.drawPoint(r.x(), r.y());
    p.drawPoint(r.right(), r.y());
    p.drawLine(r.right()+1, r.y(), r.right()+1, r.bottom());
    p.end();
}

static const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask |
    NET::DesktopMask | NET::DockMask | NET::ToolbarMask | NET::MenuMask |
    NET::DialogMask | /*NET::OverrideMask |*/ NET::TopMenuMask |
    NET::UtilityMask | NET::SplashMask;

bool LaptopClient::isTransient() const
{
    NET::WindowType type = windowType(SUPPORTED_WINDOW_TYPES_MASK);
    return type == NET::Dialog;
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
    return (new Laptop::LaptopClient(b, this))->decoration();
}

bool LaptopClientFactory::reset(unsigned long changed)
{
    findPreferredHandleSize();

    // TODO Do not recreate decorations if it is not needed. Look at
    // ModernSystem for how to do that
    Laptop::delete_pixmaps();
    Laptop::create_pixmaps();

    bool needHardReset = true;
    if ((changed & ~SettingButtons) == 0) {
		// handled by KCommonDecoration
        needHardReset = false;
    }

    if (needHardReset) {
        return true;
    } else {
        resetDecorations(changed);
        return false;
    }
}

bool LaptopClientFactory::supports( Ability ability ) const
{
    switch( ability )
    {
        // announce
        case AbilityAnnounceButtons:
        case AbilityAnnounceColors:
        // buttons
        case AbilityButtonOnAllDesktops:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonSpacer:
        // colors
        case AbilityColorTitleBack:
        case AbilityColorTitleFore:
        case AbilityColorButtonBack:
            return true;
        default:
            return false;
    };
}

QList< LaptopClientFactory::BorderSize >
LaptopClientFactory::borderSizes() const
{
    // the list must be sorted
    return QList< BorderSize >() << BorderNormal << BorderLarge <<
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

// vim: sw=4
