// $Id$
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
#include "../../workspace.h"
#include "../../options.h"
#include "modernsys.h"

#include "buttondata.h"
#include "btnhighcolor.h"

using namespace KWinInternal;


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
        aUpperGradient->resize(32, 18);
        iUpperGradient = new KPixmap;
        iUpperGradient->resize(32, 18);
        KPixmapEffect::gradient(*aUpperGradient,
                                options->color(Options::TitleBar, true).light(130),
                                options->color(Options::TitleBlend, true),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iUpperGradient,
                                options->color(Options::TitleBar, false).light(130),
                                options->color(Options::TitleBlend, false),
                                KPixmapEffect::VerticalGradient);
    }
    // buttons
    QColorGroup btnColor(options->colorGroup(Options::ButtonBg, true));
    buttonPix = new QPixmap(14, 15);
    make_button_fx(btnColor, buttonPix);
    buttonPixDown = new QPixmap(14, 15);
    make_button_fx(btnColor, buttonPixDown, true);

    btnColor = options->colorGroup(Options::ButtonBg, false);
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

static bool read_config()
{
    bool showh;
    int hsize, hwidth;
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

    if (options->customButtonPositions()) {
        bpatt = "2" + options->titleButtonsLeft() + "3t3"
                + options->titleButtonsRight() + "2";
    }
    else
        bpatt = "2X3t3HSIA2";

    if (showh == show_handle && hwidth == handle_width && hsize == handle_size
            && bpatt == *button_pattern)
        return false;
    
    show_handle = showh;
    handle_width = hwidth;
    handle_size = hsize;
    *button_pattern = bpatt;
    return true;
}

ModernButton::ModernButton(Client *parent, const char *name,
                           const unsigned char *bitmap, const QString& tip)
    : KWinButton(parent, name, tip)
{
    setBackgroundMode( NoBackground );
    QBitmap mask(14, 15, QPixmap::defaultDepth() > 8 ?
                 btnhighcolor_mask_bits : lowcolor_mask_bits, true);
    resize(14, 15);

    if(bitmap)
        setBitmap(bitmap);
    setMask(mask);
    hide();
	client = parent;
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
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
    KWinButton::mousePressEvent( &me );
}

void ModernButton::mouseReleaseEvent( QMouseEvent* e )
{
    QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
    KWinButton::mouseReleaseEvent( &me );
}


void ModernSys::slotReset()
{
    workspace()->slotResetAllClientsDelayed();
    titleBuffer.resize(0, 0);
    recalcTitleBuffer();
    for (int i = 0; i < 5; button[i++]->reset());
    repaint();
}

ModernSys::ModernSys( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    bool help = providesContextHelp();

    QGridLayout* g = new QGridLayout(this, 0, 0, 2);
    g->addWidget(windowWrapper(), 1, 1 );
    g->setRowStretch(1, 10);
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, 2);
    g->addColSpacing(2, 2 + handle_width);

    g->addRowSpacing(2, 2 + handle_width);

    QHBoxLayout* hb = new QHBoxLayout(0);
    hb->setResizeMode(QLayout::FreeResize);
    titlebar = new QSpacerItem(10, 16, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);

    button[BtnClose] = new ModernButton(this, "close", close_bits, i18n("Close"));
    button[BtnSticky] = new ModernButton(this, "sticky", NULL, i18n("Sticky"));
    button[BtnMinimize] = new ModernButton(this, "iconify", iconify_bits, i18n("Minimize"));
    button[BtnMaximize] = new ModernButton(this, "maximize", maximize_bits, i18n("Maximize"));
    button[BtnHelp] = new ModernButton(this, "help", question_bits, i18n("Help"));

    connect( button[BtnClose], SIGNAL(clicked()), this, SLOT( closeWindow() ) );
    connect( button[BtnSticky], SIGNAL(clicked()), this, SLOT( toggleSticky() ) );
    connect( button[BtnMinimize], SIGNAL(clicked()), this, SLOT( iconify() ) );
    connect( button[BtnMaximize], SIGNAL(clicked()), this, SLOT( maxButtonClicked() ) );
    connect( button[BtnHelp], SIGNAL(clicked()), this, SLOT( contextHelp() ) );

    for (int i = 0; i < (int)button_pattern->length();) {
        QChar c = (*button_pattern)[i++];
        if (c == '_')
            c = '3';

        if (c.isDigit()) {
            hb->addSpacing(int(c - '0'));
            continue;
        }
        else if (c == 'X') {
            hb->addWidget(button[BtnClose]);
            button[BtnClose]->show();
        }
        else if (c == 'S') {
            if(isSticky())
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
    setBackgroundMode(NoBackground);
    recalcTitleBuffer();
}


void ModernSys::maxButtonClicked( )
{
    switch ( button[BtnMaximize]->last_button ) {
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

void ModernSys::resizeEvent( QResizeEvent* )
{
    recalcTitleBuffer();
    doShape();
}

void ModernSys::recalcTitleBuffer()
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
                   options->colorGroup(Options::TitleBar, true).
                   brush(QColorGroup::Button));

    QRect t = titlebar->geometry();
    t.setTop( 2 );
    t.setLeft( t.left() );
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

void ModernSys::captionChange( const QString &)
{
    recalcTitleBuffer();
    repaint( titlebar->geometry(), false );
}

void ModernSys::drawRoundFrame(QPainter &p, int x, int y, int w, int h)
{
    kDrawRoundButton(&p, x, y, w, h,
                     options->colorGroup(Options::Frame, isActive()), false);

}

void ModernSys::paintEvent( QPaintEvent* )
{
    int hs = handle_size;
    int hw = handle_width;

    QPainter p( this );
    QRect t = titlebar->geometry();

    QBrush fillBrush(colorGroup().brush(QColorGroup::Background).pixmap() ?
                     colorGroup().brush(QColorGroup::Background) :
                     options->colorGroup(Options::Frame, isActive()).
                     brush(QColorGroup::Button));

    p.fillRect(1, 16, width()-2, height()-16, fillBrush);
    p.fillRect(width()-6, 0, width()-1, height(), fillBrush);

    t.setTop( 2 );
    t.setLeft( t.left() );
    t.setRight( t.right() - 2 );

    int w = width() - hw; // exclude handle
    int h = height() - hw;

    // titlebar
    QColorGroup g = options->colorGroup(Options::TitleBar, isActive());
    if(isActive()){
        p.drawPixmap(1, 1, titleBuffer, 0, 0, w-2, 18);
    }
    else{
        if(iUpperGradient)
            p.drawTiledPixmap(1, 1, w-2, 18, *iUpperGradient);
        else
            p.fillRect(1, 1, w-2, 18, fillBrush);
        p.setPen(options->color(Options::Font, isActive()));
        p.setFont(options->font(isActive()));
        p.drawText(t, AlignCenter, caption() );
    }

    // titlebar highlight
    p.setPen(g.light());
    p.drawLine(1, 1, 1, 19);
    p.drawLine(1, 1, w-3, 1);
    p.setPen(g.dark());
    p.drawLine(w-2, 1, w-2, 19);
    p.drawLine(0, 18, w-2, 18);

    // frame
    g = options->colorGroup(Options::Frame, isActive());
    p.setPen(g.light());
    p.drawLine(1, 19, 1, h-2);
    p.setPen(g.dark());
    p.drawLine(2, h-2, w-2, h-2);
    p.drawLine(w-2, 19, w-2, h-2);
    //p.drawPoint(w-3, 19);
    //p.drawPoint(2, 19);

    qDrawShadePanel(&p, 3, 19, w-6, h-22, g, true);

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

void ModernSys::showEvent(QShowEvent *ev)
{
    Client::showEvent(ev);
    doShape();
    repaint();
}

void ModernSys::windowWrapperShowEvent( QShowEvent* )
{
    doShape();
}

void ModernSys::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}

void ModernSys::stickyChange(bool on)
{
    button[BtnSticky]->setBitmap(on ? unsticky_bits : sticky_bits);
    button[BtnSticky]->setTipText(on ? i18n("Un-Sticky") : i18n("Sticky"));
}

void ModernSys::maximizeChange(bool m)
{
    button[BtnMaximize]->setBitmap(m ? minmax_bits : maximize_bits);
    button[BtnMaximize]->setTipText(m ? i18n("Restore") : i18n("Maximize"));
}

void ModernSys::init()
{
    //
}

void ModernSys::activeChange(bool)
{
    repaint(false);
    for (int i = 0; i < 5; button[i++]->reset());
}


Client::MousePosition ModernSys::mousePosition( const QPoint& p) const
{
    MousePosition m = Client::mousePosition( p );

    if ( show_handle && m == Center ) {
       int border = handle_width + 4;
	   bool hx = (p.x() >= width() - border);
	   bool hy = (p.y() >= height() - border);

       if (hx && hy)
           m = BottomRight;
       else if (hx)
           m =  Right;
       else if (hy)
           m =  Bottom;
    }
    return m;
}


// KWin extended plugin interface
extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
        return(new ModernSys(ws, w));
    }
    void init()
    {
        button_pattern = new QString;
        create_pixmaps();
        read_config();
    }
    void reset()
    {
        read_config();
		delete_pixmaps();
		create_pixmaps();
    }
    void deinit()
    {
        delete_pixmaps();
        delete button_pattern;
    }
}

#include "modernsys.moc"
// vim:ts=4:sw=4
