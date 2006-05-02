// Daniel M. DULEY <mosfet@kde.org>               original work
// Melchior FRANZ  <a8603365@unet.univie.ac.at>   configuration options

#include <kconfig.h>
#include <kglobal.h>
#include <klocale.h>
#include <qlayout.h>
#include <qdrawutil.h>
//Added by qt3to4:
#include <QPixmap>
#include <QPaintEvent>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <qbitmap.h>
#include <qtooltip.h>
#include <qapplication.h>
#include <qlabel.h>
#include "modernsys.h"

#include "buttondata.h"
#include "btnhighcolor.h"
#include <qimage.h>

namespace ModernSystem {

static unsigned char iconify_bits[] = {
    0x00, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00};

static unsigned char close_bits[] = {
    0x00, 0x66, 0x7e, 0x3c, 0x3c, 0x7e, 0x66, 0x00};

static unsigned char maximize_bits[] = {
    0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00};

static unsigned char r_minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};

static unsigned char l_minmax_bits[] = {
    0x30, 0x18, 0xcc, 0xe6, 0xf3, 0xf9, 0xfc, 0xfc};

static unsigned char unsticky_bits[] = {
    0x3c, 0x42, 0x99, 0xbd, 0xbd, 0x99, 0x42, 0x3c};

static unsigned char sticky_bits[] = {
    0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c};

static unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};

static unsigned char above_on_bits[] = {
    0x7e, 0x00, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00};

static unsigned char above_off_bits[] = {
    0x18, 0x3c, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00};

static unsigned char below_off_bits[] = {
    0x00, 0x00, 0x00, 0x7e, 0x00, 0x7e, 0x3c, 0x18};

static unsigned char below_on_bits[] = {
    0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x00, 0x7e};

static unsigned char shade_off_bits[] = {
    0x00, 0x7e, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char shade_on_bits[] = {
    0x00, 0x7e, 0x7e, 0x42, 0x42, 0x42, 0x7e, 0x00};

static unsigned char menu_bits[] = {
        0xff, 0x81, 0x81, 0xff, 0x81, 0xff, 0x81, 0xff};

static unsigned char btnhighcolor_mask_bits[] = {
    0xe0,0x41,0xf8,0x07,0xfc,0x0f,0xfe,0xdf,0xfe,0x1f,0xff,0x3f,0xff,0xff,0xff,
    0x3f,0xff,0x3f,0xff,0xff,0xff,0xff,0xfe,0x9f,0xfe,0x1f,0xfc,0x0f,0xf0,0x03,
    0x00,0x40,0x80,0x00,0x00,0x00,0x39,0x00,0x00,0x00,0x20,0x99,0x0f,0x08,0xc4,
    0x00,0x00,0x00,0x67,0x00,0x00,0x00,0x58,0x5f,0x43,0x68,0x61,0x6e,0x67,0x65 };

static QPixmap *aUpperGradient=0;
static QPixmap *iUpperGradient=0;
static QPixmap *buttonPix=0;
static QPixmap *buttonPixDown=0;
static QPixmap *iButtonPix=0;
static QPixmap *iButtonPixDown=0;

static QColor *buttonFg;
static bool pixmaps_created = false;

static QBitmap *lcDark1;
static QBitmap *lcDark2;
static QBitmap *lcDark3;
static QBitmap *lcLight1;
static QImage *btnSource;

static bool show_handle;
static int handle_size;
static int handle_width;
static int border_width;
static int title_height;

static inline const KDecorationOptions* options()
{
    return KDecoration::options();
}

static void make_button_fx(const QPalette &g, QPixmap *pix, bool light=false)
{
    pix->fill(g.background());
    QPainter p(pix);

    if(QPixmap::defaultDepth() > 8){
        int i, destH, destS, destV, srcH, srcS, srcV;
        QColor btnColor = g.background();

        if(btnSource->depth() < 32)
            *btnSource = btnSource->convertDepth(32);
        if(light)
            btnColor = btnColor.light(120);
        btnColor.getHsv(&destH, &destS, &destV);
        QImage btnDest(14, 15, 32);

        unsigned int *srcData = (unsigned int *)btnSource->bits();
        unsigned int *destData = (unsigned int *)btnDest.bits();
        QColor srcColor;
        for(i=0; i < btnSource->width()*btnSource->height(); ++i){
            srcColor.setRgb(srcData[i]);
            srcColor.getHsv(&srcH, &srcS, &srcV);
            srcColor.setHsv(destH, destS, srcV);
            destData[i] = srcColor.rgb();
        }
        *pix = QPixmap::fromImage(btnDest);

    }
    else{
        if(!lcDark1->mask()){
            lcDark1->setMask(*lcDark1);
            lcDark2->setMask(*lcDark2);
            lcDark3->setMask(*lcDark3);
            lcLight1->setMask(*lcLight1);
        }
        p.setPen(g.dark());
        p.drawPixmap(0, 0, *lcDark2);
        p.drawPixmap(0, 0, *lcDark1);
        p.setPen(g.mid());
        p.drawPixmap(0, 0, *lcDark3);
        p.setPen(g.light());
        p.drawPixmap(0, 0, *lcLight1);
    }
}


static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    lcDark1 = new QBitmap(14, 15, lowcolor_6a696a_bits, true);
    lcDark2 = new QBitmap(14, 15, lowcolor_949194_bits, true);
    lcDark3 = new QBitmap(14, 15, lowcolor_b4b6b4_bits, true);
    lcLight1 = new QBitmap(14, 15, lowcolor_e6e6e6_bits, true);
    btnSource = new QImage(btnhighcolor_xpm);

    if(QPixmap::defaultDepth() > 8){
        aUpperGradient = new QPixmap( 32, title_height+2 );
        iUpperGradient = new QPixmap( 32, title_height+2);;
        KPixmapEffect::gradient(*aUpperGradient,
                                options()->color(KDecoration::ColorTitleBar, true).light(130),
                                options()->color(KDecoration::ColorTitleBlend, true),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iUpperGradient,
                                options()->color(KDecoration::ColorTitleBar, false).light(130),
                                options()->color(KDecoration::ColorTitleBlend, false),
                                KPixmapEffect::VerticalGradient);
    }
    // buttons
    QPalette btnColor(options()->palette(KDecoration::ColorButtonBg, true) );
    btnColor.setCurrentColorGroup(QPalette::Active);
    buttonPix = new QPixmap(14, 15);
    make_button_fx(btnColor, buttonPix);
    buttonPixDown = new QPixmap(14, 15);
    make_button_fx(btnColor, buttonPixDown, true);

    btnColor = options()->palette(KDecoration::ColorButtonBg, false);
    btnColor.setCurrentColorGroup(QPalette::Active);
    iButtonPix = new QPixmap(14, 15);
    make_button_fx(btnColor, iButtonPix);
    iButtonPixDown = new QPixmap(14, 15);
    make_button_fx(btnColor, iButtonPixDown, true);


    if(qGray(btnColor.background().color().rgb()) < 150)
        buttonFg = new QColor(Qt::white);
    else
        buttonFg = new QColor(Qt::black);

    delete lcDark1;
    delete lcDark2;
    delete lcDark3;
    delete lcLight1;
    delete btnSource;
}

static void delete_pixmaps()
{
    if(aUpperGradient){
        delete aUpperGradient;
        delete iUpperGradient;
    }
    delete buttonPix;
    delete buttonPixDown;
    delete iButtonPix;
    delete iButtonPixDown;

    delete buttonFg;

    pixmaps_created = false;
}

void ModernSysFactory::read_config()
{
    bool showh;
    int hsize, hwidth, bwidth, theight;

    KConfig c("kwinmodernsysrc");
    c.setGroup("General");
    showh = c.readEntry("ShowHandle", true);

    hwidth = c.readEntry("HandleWidth", 6);
    hsize = c.readEntry("HandleSize", 30);
    if (!(showh && hsize && hwidth)) {
        showh = false;
        hwidth = hsize = 0;
    }

    switch(options()->preferredBorderSize( this )) {
      case BorderLarge:
          bwidth = 8;
          hwidth = hwidth * 7/5;
          hsize  = hsize  * 7/5;
          break;
      case BorderVeryLarge:
          bwidth = 12;
          hwidth = hwidth * 17/10 + 2;
          hsize  = hsize  * 17/10;
          break;
      case BorderHuge:
          bwidth = 18;
          hwidth = hwidth * 2 + 6;
          hsize  = hsize  * 2;
          break;
      /*
      // If we allow these large sizes we need to change the
      // correlation between the border width and the handle size.
      case BorderVeryHuge:
          bwidth = 27;
          hwidth = hwidth * 5/2 + 15;
          hsize  = hsize  * 5/2;
          break;
      case BorderOversized:
          bwidth = 40;
          hwidth = hwidth * 3 + 22;
          hsize  = hsize  * 3;
          break;
      */
      case BorderNormal:
      default:
          bwidth = 4;
    }

    theight = QFontMetrics(options()->font(true)).height() + 2;
    if (theight < 16)
        theight = 16;
    if (theight < bwidth)
        theight = bwidth;

    show_handle = showh;
    handle_width = hwidth;
    handle_size = hsize;
    border_width = bwidth;
    title_height = theight;
}

QList< ModernSysFactory::BorderSize > ModernSysFactory::borderSizes() const
{ // the list must be sorted
  return QList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge;
   // as long as the buttons don't scale don't offer the largest two sizes.
   //   BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

ModernButton::ModernButton(ButtonType type, ModernSys *parent, const char *name)
    : KCommonDecorationButton(type, parent)
{
    setObjectName( name );
    setAttribute(Qt::WA_NoSystemBackground, true);

    QBitmap mask(14, 15, QPixmap::defaultDepth() > 8 ?
                 btnhighcolor_mask_bits : lowcolor_mask_bits, true);
    resize(14, 15);

    setMask(mask);
}

void ModernButton::reset(unsigned long changed)
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
                setBitmap( isChecked() ? (isLeft()?l_minmax_bits:r_minmax_bits) : maximize_bits );
                break;
            case OnAllDesktopsButton:
                setBitmap( isChecked() ? unsticky_bits : sticky_bits );
                break;
            case ShadeButton:
                setBitmap( isChecked() ? shade_on_bits : shade_off_bits );
                break;
            case AboveButton:
                setBitmap( isChecked() ? above_on_bits : above_off_bits );
                break;
            case BelowButton:
                setBitmap( isChecked() ? below_on_bits : below_off_bits );
                break;
            case MenuButton:
                setBitmap(menu_bits);
                break;
            default:
                setBitmap(0);
                break;
        }

        this->update();
    }
}

void ModernButton::setBitmap(const unsigned char *bitmap)
{
    if (bitmap)
        deco = QBitmap(8, 8, bitmap, true);
    else {
        deco = QBitmap(8,8);
        deco.fill(Qt::color0);
    }
    deco.setMask(deco);
}

void ModernButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    drawButton(&p);
}

void ModernButton::drawButton(QPainter *p)
{
    if(decoration()->isActive()){
        if(buttonPix)
            p->drawPixmap(0, 0, isDown() ? *buttonPixDown : *buttonPix);
    }
    else{
        if(iButtonPix)
            p->drawPixmap(0, 0, isDown() ? *iButtonPixDown : *iButtonPix);
    }
    if(!deco.isNull()){
        p->setPen(*buttonFg);
        p->drawPixmap(isDown() ? 4 : 3, isDown() ? 5 : 4, deco);
    }
}

void ModernSys::reset( unsigned long changed)
{
    KCommonDecoration::reset(changed);

    titleBuffer = QPixmap();
    recalcTitleBuffer();
    resetButtons();
    widget()->update();
}

ModernSys::ModernSys( KDecorationBridge* b, KDecorationFactory* f )
    : KCommonDecoration( b, f )
{
}

QString ModernSys::visibleName() const
{
    return i18n("Modern System");
}

QString ModernSys::defaultButtonsLeft() const
{
    return "X";
}

QString ModernSys::defaultButtonsRight() const
{
    return "HSIA";
}

bool ModernSys::decorationBehaviour(DecorationBehaviour behaviour) const
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

int ModernSys::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
    // bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();

    switch (lm) {
        case LM_BorderLeft:
            return border_width + (reverse ? handle_width : 0);

        case LM_BorderRight:
            return border_width + (reverse ? 0 : handle_width);

        case LM_BorderBottom:
            return border_width + handle_width;

        case LM_TitleEdgeLeft:
            return layoutMetric(LM_BorderLeft,respectWindowState)+3;
        case LM_TitleEdgeRight:
            return layoutMetric(LM_BorderRight,respectWindowState)+3;

        case LM_TitleEdgeTop:
            return 2;

        case LM_TitleEdgeBottom:
            return 2;

        case LM_TitleBorderLeft:
        case LM_TitleBorderRight:
            return 4;

        case LM_TitleHeight:
            return title_height;

        case LM_ButtonWidth:
            return 14;
        case LM_ButtonHeight:
            return 15;

        case LM_ButtonSpacing:
            return 1;

        case LM_ExplicitButtonSpacer:
            return 3;

        default:
            return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
}

KCommonDecorationButton *ModernSys::createButton(ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new ModernButton(MenuButton, this, "menu");

        case OnAllDesktopsButton:
            return new ModernButton(OnAllDesktopsButton, this, "on_all_desktops");

        case HelpButton:
            return new ModernButton(HelpButton, this, "help");

        case MinButton:
            return new ModernButton(MinButton, this, "minimize");

        case MaxButton:
            return new ModernButton(MaxButton, this, "maximize");

        case CloseButton:
            return new ModernButton(CloseButton, this, "close");

        case AboveButton:
            return new ModernButton(AboveButton, this, "above");

        case BelowButton:
            return new ModernButton(BelowButton, this, "below");

        case ShadeButton:
            return new ModernButton(ShadeButton, this, "shade");

        default:
            return 0;
    }
}

void ModernSys::init()
{
    reverse = QApplication::isRightToLeft();

    KCommonDecoration::init();

    recalcTitleBuffer();
}

void ModernSys::recalcTitleBuffer()
{
    if(oldTitle == caption() && width() == titleBuffer.width())
        return;

    QFontMetrics fm(options()->font(true));
    titleBuffer = QPixmap(width(), title_height+2);
    QPainter p;
    p.begin(&titleBuffer);

    QPalette pt = options()->palette(ColorTitleBar, true);
    pt.setCurrentColorGroup( QPalette::Active );
    if(aUpperGradient)
        p.drawTiledPixmap(0, 0, width(), title_height+2, *aUpperGradient);
    else
        p.fillRect(0, 0, width(), title_height+2,
                   pt.brush(QPalette::Button));

    QRect t = titleRect(); // titlebar->geometry();
    t.setTop( 2 );
    t.setLeft( t.left() );
    t.setRight( t.right() - 2 );

    QRegion r(t.x(), 0, t.width(), title_height+2);
    r -= QRect(t.x()+((t.width()-fm.width(caption()))/2)-4,
               0, fm.width(caption())+8, title_height+2);
    p.setClipRegion(r);
    int i, ly;
    ly = (title_height % 3 == 0) ? 3 : 4;
    for(i=0; i < (title_height-2)/3; ++i, ly+=3){
        p.setPen(options()->color(ColorTitleBar, true).light(150));
        p.drawLine(0, ly, width()-1, ly);
        p.setPen(options()->color(ColorTitleBar, true).dark(120));
        p.drawLine(0, ly+1, width()-1, ly+1);
    }
    p.setClipRect(t);
    p.setPen(options()->color(ColorFont, true));
    p.setFont(options()->font(true));

    p.drawText(t.x()+((t.width()-fm.width(caption()))/2)-4,
               0, fm.width(caption())+8, title_height+2, Qt::AlignCenter, caption());
    p.setClipping(false);
    p.end();
    oldTitle = caption();
}

void ModernSys::updateCaption()
{
    widget()->update(titleRect() );
}

void ModernSys::drawRoundFrame(QPainter &p, int x, int y, int w, int h)
{
    QPalette pt = options()->palette(ColorFrame, isActive());
    pt.setCurrentColorGroup( QPalette::Active );
    kDrawRoundButton(&p, x, y, w, h,
                     pt, false);

}

void ModernSys::paintEvent( QPaintEvent* )
{
    // update title buffer...
    if (oldTitle != caption() || width() != titleBuffer.width() )
        recalcTitleBuffer();

    int hs = handle_size;
    int hw = handle_width;

    QPainter p( widget() );
    QRect t = titleRect(); // titlebar->geometry();

    QPalette pt = options()->palette(ColorFrame, isActive());
    pt.setCurrentColorGroup( QPalette::Active );
    QBrush fillBrush(widget()->palette().brush(QPalette::Background).pixmap() ?
                     widget()->palette().brush(QPalette::Background) :
                     pt.brush(QPalette::Button));

    p.fillRect(1, title_height+3, width()-2, height()-(title_height+3), fillBrush);
    p.fillRect(width()-6, 0, width()-1, height(), fillBrush);

    t.setTop( 2 );
    t.setLeft( t.left() );
    t.setRight( t.right() - 2 );

    int w = width() - hw; // exclude handle
    int h = height() - hw;

    // titlebar
    QPalette g = options()->palette(ColorTitleBar, isActive());
    g.setCurrentColorGroup( QPalette::Active );
    if(isActive()){
        p.drawPixmap(1, 1, titleBuffer, 0, 0, w-2, title_height+2);
    }
    else{
        if(iUpperGradient)
            p.drawTiledPixmap(1, 1, w-2, title_height+2, *iUpperGradient);
        else
            p.fillRect(1, 1, w-2, title_height+2, fillBrush);
        p.setPen(options()->color(ColorFont, isActive()));
        p.setFont(options()->font(isActive()));
        p.drawText(t, Qt::AlignCenter, caption() );
    }

    // titlebar highlight
    p.setPen(g.light());
    p.drawLine(1, 1, 1, title_height+3);
    p.drawLine(1, 1, w-3, 1);
    p.setPen(g.dark());
    p.drawLine(w-2, 1, w-2, title_height+3);
    p.drawLine(0, title_height+2, w-2, title_height+2);

    // frame
    g = options()->palette(ColorFrame, isActive());
    g.setCurrentColorGroup(QPalette::Active);
    p.setPen(g.light());
    p.drawLine(1, title_height+3, 1, h-2);
    p.setPen(g.dark());
    p.drawLine(2, h-2, w-2, h-2);
    p.drawLine(w-2, title_height+3, w-2, h-2);
    //p.drawPoint(w-3, title_height+3);
    //p.drawPoint(2, title_height+3);

    qDrawShadePanel(&p, border_width-1, title_height+3, w-2*border_width+2, h-title_height-border_width-2, g, true);

    if (show_handle) {
        p.setPen(g.dark());
        p.drawLine(width()-3, height()-hs-1, width()-3, height()-3);
        p.drawLine(width()-hs-1, height()-3, width()-3, height()-3);

        p.setPen(g.light());
        p.drawLine(width()-hw, height()-hs-1, width()-hw, height()-hw);
        p.drawLine(width()-hs-1, height()-hw, width()-hw, height()-hw);
        p.drawLine(width()-hw, height()-hs-1, width()-4, height()-hs-1);
        p.drawLine(width()-hs-1, height()-hw, width()-hs-1, height()-4);

        p.setPen(Qt::black);
        p.drawRect(0, 0, w, h);

        // handle outline
        p.drawLine(width()-hw, height()-hs, width(), height()-hs);
        p.drawLine(width()-2, height()-hs, width()-2, height()-2);
        p.drawLine(width()-hs, height()-2, width()-2, height()-2);
        p.drawLine(width()-hs, height()-hw, width()-hs, height()-2);
    } else {
        p.setPen(Qt::black);
        p.drawRect(0, 0, w, h);
    }
}

void ModernSys::updateWindowShape()
{
    int hs = handle_size;
    int hw = handle_width;
    QRegion mask;
    mask += QRect(0, 0, width()-hw, height()-hw);
    //single points
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-hw-1, 0, 1, 1);
    mask -= QRect(0, height()-hw-1, 1, 1);

    if (show_handle) {
        mask += QRect(width()-hs, height()-hs, hs-1, hs-1);
        mask -= QRect(width()-2, height()-2, 1, 1);
        mask -= QRect(width()-2, height()-hs, 1, 1);
        mask -= QRect(width()-hs, height()-2, 1, 1);
    } else
        mask -= QRect(width()-1, height()-1, 1, 1);

    setMask(mask);
}

ModernSysFactory::ModernSysFactory()
{
    read_config();
    create_pixmaps();
}

ModernSysFactory::~ModernSysFactory()
{
    ModernSystem::delete_pixmaps();
}

KDecoration* ModernSysFactory::createDecoration( KDecorationBridge* b )
{
    return(new ModernSys(b, this));
}

bool ModernSysFactory::reset( unsigned long changed )
{
    read_config();

    bool needHardReset = true;
    if( changed & (SettingColors | SettingBorder | SettingFont) )
    {
        delete_pixmaps();
        create_pixmaps();
        needHardReset = false;
    } else if (changed & SettingButtons) {
        // handled by KCommonDecoration
        needHardReset = false;
    }

    if( needHardReset )
        return true;
    else
    {
        resetDecorations( changed );
        return false; // no recreating of decorations
    }
}

bool ModernSysFactory::supports( Ability ability )
{
    switch( ability )
    {
        case AbilityAnnounceButtons:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonSpacer:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
        case AbilityButtonMenu:
            return true;
        default:
            return false;
    };
}

}

// KWin extended plugin interface
extern "C" KDE_EXPORT KDecorationFactory* create_factory()
{
    return new ModernSystem::ModernSysFactory();
}

// vim:ts=4:sw=4
