// Daniel M. DULEY <mosfet@kde.org>               original work
// Melchior FRANZ  <a8603365@unet.univie.ac.at>   configuration options

#include <kconfig.h>
#include <kglobal.h>
#include <klocale.h>
#include <qlayout.h>
#include <qdrawutil.h>
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

static unsigned char minmax_bits[] = {
    0x0c, 0x18, 0x33, 0x67, 0xcf, 0x9f, 0x3f, 0x3f};

static unsigned char unsticky_bits[] = {
    0x3c, 0x42, 0x99, 0xbd, 0xbd, 0x99, 0x42, 0x3c};

static unsigned char sticky_bits[] = {
    0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c};

static unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};

static unsigned char btnhighcolor_mask_bits[] = {
    0xe0,0x41,0xf8,0x07,0xfc,0x0f,0xfe,0xdf,0xfe,0x1f,0xff,0x3f,0xff,0xff,0xff,
    0x3f,0xff,0x3f,0xff,0xff,0xff,0xff,0xfe,0x9f,0xfe,0x1f,0xfc,0x0f,0xf0,0x03,
    0x00,0x40,0x80,0x00,0x00,0x00,0x39,0x00,0x00,0x00,0x20,0x99,0x0f,0x08,0xc4,
    0x00,0x00,0x00,0x67,0x00,0x00,0x00,0x58,0x5f,0x43,0x68,0x61,0x6e,0x67,0x65 };

static KPixmap *aUpperGradient=0;
static KPixmap *iUpperGradient=0;
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

static QString *button_pattern = NULL;
static bool show_handle;
static int handle_size;
static int handle_width;
static int border_width;
static int title_height;

static inline const KDecorationOptions* options()
{
    return KDecoration::options();
}

static void make_button_fx(const QColorGroup &g, QPixmap *pix, bool light=false)
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
        btnColor.hsv(&destH, &destS, &destV);
        QImage btnDest(14, 15, 32);

        unsigned int *srcData = (unsigned int *)btnSource->bits();
        unsigned int *destData = (unsigned int *)btnDest.bits();
        QColor srcColor;
        for(i=0; i < btnSource->width()*btnSource->height(); ++i){
            srcColor.setRgb(srcData[i]);
            srcColor.hsv(&srcH, &srcS, &srcV);
            srcColor.setHsv(destH, destS, srcV);
            destData[i] = srcColor.rgb();
        }
        pix->convertFromImage(btnDest);

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
        aUpperGradient = new KPixmap;
        aUpperGradient->resize(32, title_height+2);
        iUpperGradient = new KPixmap;
        iUpperGradient->resize(32, title_height+2);
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
    QColorGroup btnColor(options()->colorGroup(KDecoration::ColorButtonBg, true));
    buttonPix = new QPixmap(14, 15);
    make_button_fx(btnColor, buttonPix);
    buttonPixDown = new QPixmap(14, 15);
    make_button_fx(btnColor, buttonPixDown, true);

    btnColor = options()->colorGroup(KDecoration::ColorButtonBg, false);
    iButtonPix = new QPixmap(14, 15);
    make_button_fx(btnColor, iButtonPix);
    iButtonPixDown = new QPixmap(14, 15);
    make_button_fx(btnColor, iButtonPixDown, true);


    if(qGray(btnColor.background().rgb()) < 150)
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

bool ModernSysFactory::read_config()
{
    bool showh;
    int hsize, hwidth, bwidth, theight;
    QString bpatt;

    KConfig c("kwinmodernsysrc");
    c.setGroup("General");
    showh = c.readBoolEntry("ShowHandle", true);

    hwidth = c.readUnsignedNumEntry("HandleWidth", 6);
    hsize = c.readUnsignedNumEntry("HandleSize", 30);
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

    if (options()->customButtonPositions()) {
        bpatt = "2" + options()->titleButtonsLeft() + "3t3"
                + options()->titleButtonsRight() + "2";
    }
    else
        bpatt = "2X3t3HSIA2";

    if (showh == show_handle && hwidth == handle_width && hsize == handle_size
            && bwidth == border_width && theight == title_height
            && bpatt == *button_pattern)
        return false;

    show_handle = showh;
    handle_width = hwidth;
    handle_size = hsize;
    border_width = bwidth;
    title_height = theight;
    *button_pattern = bpatt;
    return true;
}

QValueList< ModernSysFactory::BorderSize > ModernSysFactory::borderSizes() const
{ // the list must be sorted
  return QValueList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge;
   // as long as the buttons don't scale don't offer the largest two sizes.
   //   BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

ModernButton::ModernButton(ModernSys *parent, const char *name,
                           const unsigned char *bitmap, const QString& tip, const int realizeBtns)
    : QButton(parent->widget(), name)
{
    setBackgroundMode( NoBackground );
    setCursor( arrowCursor );
    realizeButtons = realizeBtns;
    QBitmap mask(14, 15, QPixmap::defaultDepth() > 8 ?
                 btnhighcolor_mask_bits : lowcolor_mask_bits, true);
    resize(14, 15);

    if(bitmap)
        setBitmap(bitmap);
    setMask(mask);
    hide();
    client = parent;
    QToolTip::add( this, tip );

}

QSize ModernButton::sizeHint() const
{
    return(QSize(14, 15));
}

void ModernButton::reset()
{
    repaint(false);
}

void ModernButton::setBitmap(const unsigned char *bitmap)
{
    deco = QBitmap(8, 8, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void ModernButton::drawButton(QPainter *p)
{
    if(client->isActive()){
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

void ModernButton::mousePressEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), (e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
    QButton::mousePressEvent( &me );
}

void ModernButton::mouseReleaseEvent( QMouseEvent* e )
{
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), (e->button()&realizeButtons)?LeftButton:NoButton, e->state() );
    QButton::mouseReleaseEvent( &me );
}


void ModernSys::reset( unsigned long )
{
    titleBuffer.resize(0, 0);
    recalcTitleBuffer();
    for (int i = 0; i < 5; button[i++]->reset());
    widget()->repaint();
}

ModernSys::ModernSys( KDecorationBridge* b, KDecorationFactory* f )
    : KDecoration( b, f )
{
}

void ModernSys::init()
{
    createMainWidget( WResizeNoErase );
    widget()->installEventFilter( this );
	bool reverse = QApplication::reverseLayout();

    bool help = providesContextHelp();

    QGridLayout* g = new QGridLayout(widget(), 0, 0, 2);
    if( isPreview())
        g->addWidget( new QLabel( i18n( "<center><b>ModernSys preview</b></center>" ), widget()), 1, 1 );
    else
        g->addItem( new QSpacerItem( 0, 0 ), 1, 1 ); // no widget in the middle
    g->setRowStretch(1, 10);
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, border_width-2 + (reverse ? handle_width : 0));
    g->addColSpacing(2, border_width-2 + (reverse ? 0 : handle_width));

    g->addRowSpacing(2, border_width-2 + handle_width);

	QBoxLayout* hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0);
    hb->setResizeMode(QLayout::FreeResize);
    titlebar = new QSpacerItem(10, title_height, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);

    button[BtnClose] = new ModernButton(this, "close", close_bits, i18n("Close"));
    button[BtnSticky] = new ModernButton(this, "sticky", NULL, isOnAllDesktops()?i18n("Un-Sticky"):i18n("Sticky"));
    button[BtnMinimize] = new ModernButton(this, "iconify", iconify_bits, i18n("Minimize"));
    button[BtnMaximize] = new ModernButton(this, "maximize", maximize_bits, i18n("Maximize"), LeftButton|MidButton|RightButton);
    button[BtnHelp] = new ModernButton(this, "help", question_bits, i18n("Help"));

    connect( button[BtnClose], SIGNAL(clicked()), this, SLOT( closeWindow() ) );
    connect( button[BtnSticky], SIGNAL(clicked()), this, SLOT( toggleOnAllDesktops() ) );
    connect( button[BtnMinimize], SIGNAL(clicked()), this, SLOT( minimize() ) );
    connect( button[BtnMaximize], SIGNAL(clicked()), this, SLOT( maxButtonClicked() ) );
    connect( button[BtnHelp], SIGNAL(clicked()), this, SLOT( showContextHelp() ) );

    for (int i = 0; i < (int)button_pattern->length();) {
        QChar c = (*button_pattern)[i++];
        if (c == '_')
            c = '3';

        if (c.isDigit()) {
            hb->addSpacing(int(c - '0'));
            continue;
        }
        else if (c == 'X' && isCloseable()) {
            hb->addWidget(button[BtnClose]);
            button[BtnClose]->show();
        }
        else if (c == 'S') {
            if(isOnAllDesktops())
                button[BtnSticky]->setBitmap(unsticky_bits);
            else
                button[BtnSticky]->setBitmap(sticky_bits);
            hb->addWidget(button[BtnSticky]);
            button[BtnSticky]->show();
        }
        else if (c == 'I' && isMinimizable()) {
            hb->addWidget(button[BtnMinimize]);
            button[BtnMinimize]->show();
        }
        else if (c == 'A' && isMaximizable()) {
            hb->addWidget(button[BtnMaximize]);
            button[BtnMaximize]->show();
        }
        else if (help && c == 'H') {
            hb->addWidget(button[BtnHelp]);
            button[BtnHelp]->show();
        }
        else if (c == 't')
            hb->addItem(titlebar);

        if ((*button_pattern)[i] >= 'A' && (*button_pattern)[i] <= 'Z')
            hb->addSpacing(1);
    }

    g->addLayout( hb, 0, 1 );
    widget()->setBackgroundMode(NoBackground);
    recalcTitleBuffer();
    widget()->layout()->activate();
}


void ModernSys::maxButtonClicked( )
{
    switch ( button[BtnMaximize]->last_button ) {
    case MidButton:
       maximize( maximizeMode() ^ MaximizeVertical );
       break;
    case RightButton:
       maximize( maximizeMode() ^ MaximizeHorizontal );
       break;
    default: //LeftButton:
       maximize( maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull );
       break;
    }
}

void ModernSys::resizeEvent( QResizeEvent* )
{
    recalcTitleBuffer();
    doShape();
}

void ModernSys::recalcTitleBuffer()
{
    if(oldTitle == caption() && width() == titleBuffer.width())
        return;

    QFontMetrics fm(options()->font(true));
    titleBuffer.resize(width(), title_height+2);
    QPainter p;
    p.begin(&titleBuffer);
    if(aUpperGradient)
        p.drawTiledPixmap(0, 0, width(), title_height+2, *aUpperGradient);
    else
        p.fillRect(0, 0, width(), title_height+2,
                   options()->colorGroup(ColorTitleBar, true).
                   brush(QColorGroup::Button));

    QRect t = titlebar->geometry();
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
               0, fm.width(caption())+8, title_height+2, AlignCenter, caption());
    p.setClipping(false);
    p.end();
    oldTitle = caption();
}

void ModernSys::captionChange()
{
    recalcTitleBuffer();
    widget()->repaint( titlebar->geometry(), false );
}

void ModernSys::drawRoundFrame(QPainter &p, int x, int y, int w, int h)
{
    kDrawRoundButton(&p, x, y, w, h,
                     options()->colorGroup(ColorFrame, isActive()), false);

}

void ModernSys::paintEvent( QPaintEvent* )
{
    int hs = handle_size;
    int hw = handle_width;

    QPainter p( widget() );
    QRect t = titlebar->geometry();

    QBrush fillBrush(widget()->colorGroup().brush(QColorGroup::Background).pixmap() ?
                     widget()->colorGroup().brush(QColorGroup::Background) :
                     options()->colorGroup(ColorFrame, isActive()).
                     brush(QColorGroup::Button));

    p.fillRect(1, 16, width()-2, height()-16, fillBrush);
    p.fillRect(width()-6, 0, width()-1, height(), fillBrush);

    t.setTop( 2 );
    t.setLeft( t.left() );
    t.setRight( t.right() - 2 );

    int w = width() - hw; // exclude handle
    int h = height() - hw;

    // titlebar
    QColorGroup g = options()->colorGroup(ColorTitleBar, isActive());
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
        p.drawText(t, AlignCenter, caption() );
    }

    // titlebar highlight
    p.setPen(g.light());
    p.drawLine(1, 1, 1, title_height+3);
    p.drawLine(1, 1, w-3, 1);
    p.setPen(g.dark());
    p.drawLine(w-2, 1, w-2, title_height+3);
    p.drawLine(0, title_height+2, w-2, title_height+2);

    // frame
    g = options()->colorGroup(ColorFrame, isActive());
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

void ModernSys::doShape()
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

void ModernSys::showEvent(QShowEvent *)
{
    doShape();
    widget()->repaint();
}

void ModernSys::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        titlebarDblClickOperation();
}

void ModernSys::desktopChange()
{
    bool sticky_on = isOnAllDesktops();
    button[BtnSticky]->setBitmap(sticky_on ? unsticky_bits : sticky_bits);
    QToolTip::remove( button[BtnSticky] );
    QToolTip::add( button[BtnSticky], sticky_on ? i18n("Un-Sticky") : i18n("Sticky"));
}

void ModernSys::maximizeChange()
{
    bool m = ( maximizeMode() == MaximizeFull );
    button[BtnMaximize]->setBitmap(m ? minmax_bits : maximize_bits);
    QToolTip::remove( button[BtnMaximize] );
    QToolTip::add( button[BtnMaximize], m ? i18n("Restore") : i18n("Maximize"));
}

void ModernSys::activeChange()
{
    widget()->repaint(false);
    for (int i = 0; i < 5; button[i++]->reset());
}


ModernSys::Position ModernSys::mousePosition( const QPoint& p) const
{
    Position m = KDecoration::mousePosition( p );

    const int range = 14 + 3*border_width/2;
    const int border = show_handle ? handle_width + border_width : border_width;
    const int range2 = show_handle ? handle_width + border_width : range;
    const int range3 = show_handle ? handle_width + range : range;

    if ( ( p.x() > border_width && p.x() < width() - border )
         && ( p.y() > 4 && p.y() < height() - border ) )
        m = PositionCenter;
    else if ( p.y() <= range && p.x() <= range)
        m = PositionTopLeft;
    else if ( p.y() >= height()-range2 && p.x() >= width()-range2)
        m = PositionBottomRight;
    else if ( p.y() >= height()-range3 && p.x() <= range)
        m = PositionBottomLeft;
    else if ( p.y() <= range && p.x() >= width()-range3)
        m = PositionTopRight;
    else if ( p.y() <= 4 )
        m = PositionTop;
    else if ( p.y() >= height()-border )
        m = PositionBottom;
    else if ( p.x() <= border_width )
        m = PositionLeft;
    else if ( p.x() >= width()-border )
        m = PositionRight;
    else
        m = PositionCenter;

    return m;
}

void ModernSys::resize( const QSize& s )
{
    widget()->resize( s );
}

void ModernSys::iconChange()
{
}

void ModernSys::shadeChange()
{
}

QSize ModernSys::minimumSize() const
{
    return QSize( 50, 50 ); // FRAME
}

void ModernSys::borders( int& left, int& right, int& top, int& bottom ) const
{
    bool reverse = QApplication::reverseLayout();
    left = border_width + (reverse ? handle_width : 0);
    right = border_width + (reverse ? 0 : handle_width);
    top = 4 + titlebar->geometry().height(); // FRAME is this ok?
    bottom = border_width + handle_width;
// FRAME this below needs doShape() changes
//    if( isShade())
//        bottom = 0;
//    if( ( maximizeMode() & MaximizeHorizontal ) && !options()->moveResizeMaximizedWindows())
//        left = right = 0;
//    if( ( maximizeMode() & MaximizeVertical ) && !options()->moveResizeMaximizedWindows())
//        bottom = 0;
}

bool ModernSys::eventFilter( QObject* o, QEvent* e )
{
    if( o != widget())
	return false;
    switch( e->type())
	{
	case QEvent::Resize:
	    resizeEvent( static_cast< QResizeEvent* >( e ));
	    return true;
	case QEvent::Paint:
	    paintEvent( static_cast< QPaintEvent* >( e ));
	    return true;
	case QEvent::MouseButtonDblClick:
	    mouseDoubleClickEvent( static_cast< QMouseEvent* >( e ));
	    return true;
	case QEvent::MouseButtonPress:
	    processMousePressEvent( static_cast< QMouseEvent* >( e ));
	    return true;
	case QEvent::Show:
	    showEvent( static_cast< QShowEvent* >( e ));
	    return true;
	default:
	    break;
	}
    return false;
}

ModernSysFactory::ModernSysFactory()
{
    button_pattern = new QString;
    read_config();
    create_pixmaps();
}

ModernSysFactory::~ModernSysFactory()
{
    ModernSystem::delete_pixmaps();
    delete ModernSystem::button_pattern;
}

KDecoration* ModernSysFactory::createDecoration( KDecorationBridge* b )
{
    return(new ModernSys(b, this));
}

bool ModernSysFactory::reset( unsigned long changed )
{
    bool ret = read_config();
    if( changed & (SettingColors | SettingBorder) )
    {
        delete_pixmaps();
        create_pixmaps();
    }
    if( ret )
        return true;
    else
    {
        resetDecorations( changed );
        return false; // no recreating of decorations
    }
}

}

// KWin extended plugin interface
extern "C" KDecorationFactory* create_factory()
{
    return new ModernSystem::ModernSysFactory();
}



#include "modernsys.moc"
// vim:ts=4:sw=4
