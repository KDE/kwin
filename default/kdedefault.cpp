
#include <kconfig.h> // up here to avoid X11 header conflict :P
#include "kdedefault.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <kglobal.h>
#include <kapp.h>
#include <qbitmap.h>
#include "../workspace.h"
#include "../options.h"

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
        return(new KDEClient(ws, w));
    }
}

static const char *kdelogo[] = {
/* columns rows colors chars-per-pixel */
"16 16 8 1",
" 	c None",
".	c #000000",
"+	c #A0A0A4",
"@	c #FFFFFF",
"#	c #585858",
"$	c #C0C0C0",
"%	c #808080",
"&	c #DCDCDC",
"                ",
"     ..    ..   ",
"    .+@.  .@#.  ",
"   .@@@. .@@@#  ",
"   .@@@..$@@$.  ",
"   .@@@.@@@$.   ",
"   .@@@%@@$.    ",
"   .@@@&@@.     ",
"   .@@@@@@.     ",
"   .@@@$@@&.    ",
"   .@@@.@@@.    ",
"   .@@@.+@@@.   ",
"   .@@@..$@@&.  ",
"   .@@%. .@@@.  ",
"   ....   ...   ",
"                "};

static unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x78, 0x00,
  0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char close_bits[] = {
  0x00, 0x00, 0x84, 0x00, 0xce, 0x01, 0xfc, 0x00, 0x78, 0x00, 0x78, 0x00,
  0xfc, 0x00, 0xce, 0x01, 0x84, 0x00, 0x00, 0x00};


static unsigned char maximize_bits[] = {
  0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0x86, 0x01, 0x86, 0x01, 0x86, 0x01,
  0x86, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0x00, 0x00};

static unsigned char minmax_bits[] = {
  0x7f, 0x00, 0x7f, 0x00, 0x63, 0x00, 0xfb, 0x03, 0xfb, 0x03, 0x1f, 0x03,
  0x1f, 0x03, 0x18, 0x03, 0xf8, 0x03, 0xf8, 0x03};

static unsigned char question_bits[] = {
  0x00, 0x00, 0x78, 0x00, 0xcc, 0x00, 0xc0, 0x00, 0x60, 0x00, 0x30, 0x00,
  0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00};

static unsigned char pindown_white_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x1f, 0xa0, 0x03,
  0xb0, 0x01, 0x30, 0x01, 0xf0, 0x00, 0x70, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pindown_gray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c,
  0x00, 0x0e, 0x00, 0x06, 0x00, 0x00, 0x80, 0x07, 0xc0, 0x03, 0xe0, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pindown_dgray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x10, 0x70, 0x20, 0x50, 0x20,
  0x48, 0x30, 0xc8, 0x38, 0x08, 0x1f, 0x08, 0x18, 0x10, 0x1c, 0x10, 0x0e,
  0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pindown_mask_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x1f, 0xf0, 0x3f, 0xf0, 0x3f,
  0xf8, 0x3f, 0xf8, 0x3f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf0, 0x1f, 0xf0, 0x0f,
  0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_white_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x11,
  0x3f, 0x15, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_gray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x0a, 0xbf, 0x0a, 0x80, 0x15, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_dgray_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0x40, 0x31, 0x40, 0x2e,
  0x40, 0x20, 0x40, 0x20, 0x7f, 0x2a, 0x40, 0x3f, 0xc0, 0x31, 0xc0, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char pinup_mask_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0xc0, 0x31, 0xc0, 0x3f,
  0xff, 0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xc0, 0x3f, 0xc0, 0x31, 0xc0, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


static QPixmap *titlePix=0;
static KPixmap *aUpperGradient=0;
static KPixmap *iUpperGradient=0;
// buttons active, inactive, and mouse over for both
static KPixmap *btnPix1=0;
static KPixmap *iBtnPix1=0;
static KPixmap *btnDownPix1=0;
static KPixmap *iBtnDownPix1=0;
static KPixmap *pinupPix=0;
static KPixmap *pindownPix=0;
static KPixmap *iPinupPix=0;
static KPixmap *iPindownPix=0;
static KPixmap *pinupPixMO=0;
static KPixmap *pindownPixMO=0;
static KPixmap *iPinupPixMO=0;
static KPixmap *iPindownPixMO=0;

static QColor btnForeground;
static QColor btnMouseOver;
static QPixmap *defaultMenuPix=0;

static bool pixmaps_created = false;

static int titleHeight = 14; // configurable title height not implemented yet


static void drawButtonFrame(KPixmap *pix, const QColorGroup &g, bool sunken)
{
    QPainter p;
    int w = pix->width();
    int h = pix->height();
    int x2 = w-1;
    int y2 = h-1;
    p.begin(pix);

    // outer frame
    p.setPen(g.mid());
    p.drawLine(0, 0, x2, 0);
    p.drawLine(0, 0, 0, y2);
    p.setPen(g.light());
    p.drawLine(x2, 0, x2, y2);
    p.drawLine(0, x2, y2, x2);
    p.setPen(g.dark());
    p.drawRect(1, 1, w-2, h-2);
    p.setPen(sunken ? g.mid() : g.light());
    p.drawLine(2, 2, x2-2, 2);
    p.drawLine(2, 2, 2, y2-2);
    p.setPen(sunken ? g.light() : g.mid());
    p.drawLine(x2-2, 2, x2-2, y2-2);
    p.drawLine(2, x2-2, y2-2, x2-2);
}

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    bool highcolor = QPixmap::defaultDepth() > 8;
    // titlebar
    QPainter p;
    QPainter maskPainter;
    int i, x, y;
    titlePix = new QPixmap(33, 18);
    QBitmap mask(33, 18);
    mask.fill(Qt::color0);

    p.begin(titlePix);
    maskPainter.begin(&mask);
    maskPainter.setPen(Qt::color1);
    for(i=0, y=2; i < 4; ++i, y+=4){
        for(x=1; x <= 33; x+=3){
            p.setPen(options->color(Options::TitleBar, true).light(150));
            p.drawPoint(x, y);
            maskPainter.drawPoint(x, y);
            p.setPen(options->color(Options::TitleBar, true).dark(150));
            p.drawPoint(x+1, y+1);
            maskPainter.drawPoint(x+1, y+1);
        }
    }
    p.end();
    maskPainter.end();
    titlePix->setMask(mask);

    if(highcolor){
        aUpperGradient = new KPixmap;
        aUpperGradient->resize(32, 18);
        iUpperGradient = new KPixmap;
        iUpperGradient->resize(32, 18);
        QColorGroup bgColor = options->colorGroup(Options::TitleBar, true);
        KPixmapEffect::gradient(*aUpperGradient,
                                bgColor.midlight(),
                                bgColor.mid(),
                                KPixmapEffect::VerticalGradient);
        bgColor = options->colorGroup(Options::TitleBar, false);
        KPixmapEffect::gradient(*iUpperGradient,
                                bgColor.midlight(),
                                bgColor.mid(),
                                KPixmapEffect::VerticalGradient);
    }
    // pinup and pindown
    pinupPix = new KPixmap;
    pinupPix->resize(16, 16);
    pindownPix = new KPixmap;
    pindownPix->resize(16, 16);
    iPinupPix = new KPixmap;
    iPinupPix->resize(16, 16);
    iPindownPix = new KPixmap;
    iPindownPix->resize(16, 16);
    QBitmap pinupMask(16, 16, pinup_mask_bits, true);
    QBitmap pindownMask(16, 16, pindown_mask_bits, true);

    p.begin(pinupPix);
    if(highcolor)
        p.drawPixmap(0, 0, *aUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, true));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, true), 0, 0,
                  16, 16, true, pinup_white_bits, pinup_gray_bits, NULL,
                  pinup_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iPinupPix);
    if(highcolor)
        p.drawPixmap(0, 0, *iUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, false));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, false), 0, 0,
                  16, 16, true, pinup_white_bits, pinup_gray_bits, NULL,
                  pinup_dgray_bits, NULL, NULL);
    p.end();
    p.begin(pindownPix);
    if(highcolor)
        p.drawPixmap(0, 0, *aUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, true));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, true), 0, 0,
                  16, 16, true, pindown_white_bits, pindown_gray_bits, NULL,
                  pindown_dgray_bits, NULL, NULL);
    p.end();
    p.begin(iPindownPix);
    if(highcolor)
        p.drawPixmap(0, 0, *iUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, false));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, false), 0, 0,
                  16, 16, true, pindown_white_bits, pindown_gray_bits, NULL,
                  pindown_dgray_bits, NULL, NULL);
    p.end();

    // pin mouse over
    pinupPixMO = new KPixmap;
    pinupPixMO->resize(16, 16);
    pindownPixMO = new KPixmap;
    pindownPixMO->resize(16, 16);
    iPinupPixMO = new KPixmap;
    iPinupPixMO->resize(16, 16);
    iPindownPixMO = new KPixmap;
    iPindownPixMO->resize(16, 16);

    p.begin(pinupPixMO);
    if(highcolor)
        p.drawPixmap(0, 0, *aUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, true));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, true), 0, 0,
                  16, 16, true, pinup_white_bits, pinup_dgray_bits,
                  pinup_gray_bits, NULL, NULL, NULL);
    p.end();
    p.begin(iPinupPixMO);
    if(highcolor)
        p.drawPixmap(0, 0, *iUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, false));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, false), 0, 0,
                  16, 16, true, pinup_white_bits, pinup_dgray_bits,
                  pinup_gray_bits, NULL, NULL, NULL);
    p.end();
    p.begin(pindownPixMO);
    if(highcolor)
        p.drawPixmap(0, 0, *aUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, true));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, true), 0, 0,
                  16, 16, true, pindown_white_bits, pindown_dgray_bits,
                  pindown_gray_bits, NULL, NULL, NULL);
    p.end();
    p.begin(iPindownPixMO);
    if(highcolor)
        p.drawPixmap(0, 0, *iUpperGradient);
    else
        p.fillRect(0, 0, 16, 16, options->color(Options::TitleBar, false));
    kColorBitmaps(&p, options->colorGroup(Options::TitleBar, false), 0, 0,
                  16, 16, true, pindown_white_bits, pindown_dgray_bits,
                  pindown_gray_bits, NULL, NULL);
    p.end();

    // buttons (active/inactive, sunken/unsunken, 2 sizes each)
    QColorGroup g = options->colorGroup(Options::ButtonBg, true);
    QColor c = g.background();
    btnPix1 = new KPixmap;
    btnPix1->resize(16, 16);
    btnDownPix1 = new KPixmap;
    btnDownPix1->resize(16, 16);
    iBtnPix1 = new KPixmap;
    iBtnPix1->resize(16, 16);
    iBtnDownPix1 = new KPixmap;
    iBtnDownPix1->resize(16, 16);
    if(highcolor){
        KPixmapEffect::gradient(*btnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*btnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        KPixmapEffect::gradient(*iBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
    }
    else{
        btnPix1->fill(c.rgb());
        btnDownPix1->fill(c.rgb());
        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        iBtnPix1->fill(c.rgb());
        iBtnDownPix1->fill(c.rgb());
    }
    g = options->colorGroup(Options::ButtonBg, true);
    c = g.background();
    drawButtonFrame(btnPix1, g, false);
    drawButtonFrame(btnDownPix1, g, true);
    g = options->colorGroup(Options::ButtonBg, false);
    c = g.background();
    drawButtonFrame(iBtnPix1, g, false);
    drawButtonFrame(iBtnDownPix1, g, true);

    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 128){
        btnForeground = Qt::black;
        btnMouseOver = Qt::darkGray;
    }
    else{
        btnForeground = Qt::white;
        btnMouseOver = Qt::lightGray;

    }
}


KDEDefaultClientButton::KDEDefaultClientButton(Client *parent, const char *name,
                           const unsigned char *bitmap)
    : QToolButton(parent, name)
{
    isMouseOver = false;
    client = parent;
    setFixedSize(16, 16);
    resize(16, 16);
    if(bitmap)
        setBitmap(bitmap);
}

QSize KDEDefaultClientButton::sizeHint() const
{
    return(QSize(16, 16));
}

void KDEDefaultClientButton::reset()
{
    repaint(false);
}

void KDEDefaultClientButton::setBitmap(const unsigned char *bitmap)
{
    pix.resize(0, 0);
    deco = QBitmap(10, 10, bitmap, true);
    deco.setMask(deco);
    repaint();
}

void KDEDefaultClientButton::setPixmap(const QPixmap &p,
                                       const QPixmap &mouseOverPix)
{
    deco.resize(0, 0);
    pix = p;
    moPix = mouseOverPix;
    clearMask();
    repaint();
}

void KDEDefaultClientButton::drawButton(QPainter *p)
{
    if(pix.isNull()){
        if(btnPix1){
            if(client->isActive()){
                if(isDown())
                    p->drawPixmap(0, 0, *btnDownPix1);
                else
                    p->drawPixmap(0, 0, *btnPix1);
            }
            else{
                if(isDown())
                    p->drawPixmap(0, 0, *iBtnDownPix1);
                else
                    p->drawPixmap(0, 0, *iBtnPix1);
            }
        }
        else{
            QColorGroup g = options->colorGroup(Options::ButtonBg,
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
        p->setPen(isMouseOver ? btnMouseOver : btnForeground);
        int xOff = (width()-10)/2;
        int yOff = (height()-10)/2;
        p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, deco);
    }
    else{
        if(QPixmap::defaultDepth() > 8)
            p->drawPixmap(0, 0, client->isActive()? *aUpperGradient : *iUpperGradient);
        else
            p->fillRect(0, 0, width(), height(),
                        options->color(Options::TitleBar, client->isActive()));
        p->drawPixmap(0, 0, isMouseOver ? moPix : pix);
    }

}

void KDEClient::slotReset()
{
    delete titlePix;
    delete pinupPix;
    delete pindownPix;
    delete iPinupPix;
    delete iPindownPix;
    delete pinupPixMO;
    delete pindownPixMO;
    delete iPinupPixMO;
    delete iPindownPixMO;


    if(aUpperGradient){
        delete aUpperGradient;
        delete iUpperGradient;
        delete btnPix1;
        delete btnDownPix1;
        delete iBtnPix1;
        delete iBtnDownPix1;
    }
    pixmaps_created = false;
    create_pixmaps();
    int i;
    for(i=0; i < 5; ++i){
        if(button[i])
            button[i]->reset();
    }
    repaint();
}

KDEClient::KDEClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WNorthWestGravity )
{
    if(!defaultMenuPix)
        defaultMenuPix = new QPixmap(kdelogo);

    lastButtonWidth = 0;
    lastBufferWidth = 0;

    create_pixmaps();
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    bool help = providesContextHelp();


    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);
    g->addRowSpacing(2, 1);
    g->addWidget(windowWrapper(), 3, 1);
    // without the next line, unshade flickers
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );
    g->setRowStretch(3, 10);
    g->addRowSpacing(4, 8); // bottom handles
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    int th = titleHeight;
    if ( isTool() )
	th -= 2;

    button[BtnMenu] = new KDEDefaultClientButton(this, "menu");
    if(!miniIcon().isNull()){
        lightIcon = miniIcon();
        lightIcon.detach();
        lightIcon = KPixmapEffect::intensity(lightIcon, 1.0);
        if(miniIcon().mask())
            lightIcon.setMask(*miniIcon().mask());
        button[BtnMenu]->setPixmap(miniIcon(), lightIcon);
    }
    else{
        lightIcon = *defaultMenuPix;
        lightIcon.detach();
        lightIcon = KPixmapEffect::intensity(lightIcon, .50);
        lightIcon.setMask(*defaultMenuPix->mask());
        button[BtnMenu]->setPixmap(*defaultMenuPix, lightIcon);
    }
    connect(button[BtnMenu], SIGNAL(pressed()), this,
            SLOT(menuButtonPressed()));
    button[BtnClose] = new KDEDefaultClientButton(this, "close", close_bits);
    button[BtnSticky] = new KDEDefaultClientButton(this, "sticky");

    if(isSticky()){
        if(isActive())
            button[BtnSticky]->setPixmap(*pindownPix, *pindownPixMO);
        else
            button[BtnSticky]->setPixmap(*iPindownPix, *iPindownPixMO);

    }
    else{
        if(isActive())
            button[BtnSticky]->setPixmap(*pinupPix, *pinupPixMO);
        else
            button[BtnSticky]->setPixmap(*iPinupPix, *iPinupPixMO);
    }
    button[BtnIconify] = new KDEDefaultClientButton(this, "iconify",
                                          iconify_bits);
    button[BtnMax] = new KDEDefaultClientButton(this, "maximize",
                                      maximize_bits);
    if(help){
        button[BtnHelp] = new KDEDefaultClientButton(this, "help",
                                     question_bits);
        connect(button[BtnHelp], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }
    else
        button[BtnHelp] = NULL;

    connect( button[BtnClose], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[BtnSticky], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    connect( button[BtnIconify], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[BtnMax], SIGNAL( clicked() ), this, ( SLOT( slotMaximize() ) ) );

    hb = new QHBoxLayout();
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 1, 1 );
    hb->addWidget( button[BtnMenu]);
    hb->addSpacing(2);
    hb->addWidget( button[BtnSticky]);
    hb->addSpacing(1);
    titlebar = new QSpacerItem(10, th, QSizePolicy::Expanding,
                               QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(2);
    if(help){
        hb->addWidget( button[BtnHelp]);
    }
    hb->addWidget( button[BtnIconify]);
    hb->addWidget( button[BtnMax]);
    hb->addSpacing(1);
    hb->addWidget( button[BtnClose]);

    if ( isTransient() || isTool() )
	button[BtnSticky]->hide();
    if ( !isMinimizable() )
	button[BtnIconify]->hide();
    if ( !isMaximizable() )
	button[BtnMax]->hide();

    hiddenItems = false;
    bufferDirty = true;
}

void KDEClient::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
	maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
	maximize( MaximizeHorizontal );
    else
	maximize();
}

void KDEClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

    doShape();
    bool titlebar_moved = FALSE;
    if ( e->oldSize().width() != width() ) {
	QRect t = titlebar->geometry();
 	calcHiddenButtons();
	// make layout update titlebar->geometry() in case some buttons
	// where shown or hidden in calcHiddenButtons() above
	QApplication::sendPostedEvents( this, QEvent::LayoutHint );
	titlebar_moved = t != titlebar->geometry();
    }

    if ( !isVisible() )
	return;

    // we selected WResizeNoErase and WNorthWestGravity. That means:
    // on a resize event, we do not get a full paint event and the
    // background is not erased. This makes it possible for us to
    // reduce flicker. All we have to do is to generate the necessary
    // paint events manually. We do this using update(). The titlebar
    // is even more special: it draws every pixel, so there is no need
    // to erase the background. For that reasons we do not use
    // update(), but post a paint event with the erased flag set to
    // FALSE.


    if ( e->oldSize().width() != width() ) {
 	int dx =  (width() - titlebar->geometry().right()) + QABS( e->oldSize().width() -  width() );
 	if ( dx )
 	    update( width() - dx + 1, 0, dx, height() );
	int dx2 = width() - e->oldSize().width();
 	// titlebar needs no background
	if ( titlebar_moved )
	    QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
	else if ( dx2 > 0 ) {
	    dx2 += 2;
	    QApplication::postEvent( this, new QPaintEvent( QRect( titlebar->geometry().right()-dx2, titlebar->geometry().top(), dx2, titlebar->geometry().height()), FALSE ) );
	}
    }
    if ( e->oldSize().height() != height() ) {
	int dy = 8 + QABS( e->oldSize().height() -  height() );
	update( 0, height() - dy + 1, width(), dy );
    }
}

void KDEClient::captionChange( const QString& )
{
    bufferDirty = true;
    repaint( titlebar->geometry(), false );
}

void KDEClient::paintEvent( QPaintEvent* )
{
    QPainter p(this);

    QRect r(rect());
    p.setPen(Qt::black);
    p.drawRect(r);

    // calc the length of the titlebar (vs frame) color
    int titleX2 = r.width() - 6 -
        (button[BtnHelp] && button[BtnHelp]->isVisible() ? 16 : 0) -
        (button[BtnMax]->isVisible() ? 16 : 0) -
        (button[BtnIconify]->isVisible() ? 16 : 0) -
        (button[BtnClose]->isVisible() ? 16 : 0);

    // draw the part of the frame shaded to the titlebar color
    // is the height large enough to handle the extended left titlebar color?
    int leftFrameStart = r.height() > 54 ? r.y()+42 : r.y()+18;
    QColorGroup g = options->colorGroup(Options::TitleBar, isActive());
    p.setPen(g.light());
    p.drawLine(r.x()+1, r.y()+1, titleX2-1, r.y()+1);
    p.drawLine(r.x()+1, r.y()+1, r.x()+1, leftFrameStart);
    if(r.height() > 42){
        p.setPen(Qt::black);
        p.drawLine(r.x()+1, leftFrameStart, r.x()+2, leftFrameStart-1);
        p.setPen(options->color(Options::Frame, isActive()));
        p.drawPoint(r.x()+2, leftFrameStart);
        p.setPen(g.mid());
        p.drawLine(r.x()+2, r.y()+19, r.x()+2, leftFrameStart-2);
    }
    p.setPen(g.dark());
    p.drawLine(titleX2-1, r.y()+1, titleX2-1, r.y()+18);

    // part of the frame that is the frame color
    g = options->colorGroup(Options::Frame, isActive());
    p.setPen(g.light());
    p.drawLine(titleX2, r.y()+1, r.right()-1, r.y()+1);
    p.drawLine(r.x()+1, leftFrameStart+1, r.x()+1, r.bottom()-1);
    p.setPen(g.dark());
    p.drawLine(r.right()-1, r.y()+1, r.right()-1, r.bottom()-1);
    p.drawLine(r.x()+1, r.bottom()-1, r.right()-1, r.bottom()-1);


    int th = titleHeight;
    if ( isTool() )
	th -= 2;

    // inner rect
    p.drawRect(r.x()+3, r.y()+16+3, r.width()-6,
               r.height()-16-10);
    // handles
    if(r.width() > 44){
        qDrawShadePanel(&p, r.x()+1, r.bottom()-6, 20,
                        6, g, false, 1, &g.brush(QColorGroup::Mid));
        qDrawShadePanel(&p, r.x()+21, r.bottom()-6, r.width()-42, 6,
                        g, false, 1, isActive() ?
                        &g.brush(QColorGroup::Background) :
                        &g.brush(QColorGroup::Mid));
        qDrawShadePanel(&p, r.right()-20, r.bottom()-6, 20, 6,
                        g, false, 1, &g.brush(QColorGroup::Mid));
    }
    else
        qDrawShadePanel(&p, r.x()+1, r.bottom()-6, r.width()-2, 6, g,
                        false, 1, isActive() ?
                        &g.brush(QColorGroup::Background) :
                        &g.brush(QColorGroup::Mid));

    r.setRight(r.right()-1);

    if(isActive()){
        /*
        updateActiveBuffer();
        p.drawPixmap(r.x(), r.y(), activeBuffer);
        */
        if(aUpperGradient){
            p.drawTiledPixmap(r.x()+2, r.y()+2, titleX2-3, r.y()+17,
                              *aUpperGradient);
        }
        else{
            p.fillRect(QRect(QPoint(r.x()+2, r.y()+2),
                             QPoint(titleX2-2, r.y()+18)),
                       options->color(Options::TitleBar, true));
        }
        r = titlebar->geometry();
        QFontMetrics fm(options->font(true));
        p.setFont(options->font(true));
        p.setPen(options->color(Options::Font, true));
        p.drawText(r.x(), r.y(), r.width(), r.height()-1,
                   AlignLeft, caption() );
        p.drawTiledPixmap(r.x()+fm.width(caption()), 2,
                          r.width()-fm.width(caption())-2,
                          r.height(), *titlePix);

    }
    else{
        if(iUpperGradient){
            p.drawTiledPixmap(r.x()+2, r.y()+2, titleX2-3, r.y()+17,
                              *iUpperGradient);
        }
        else{
            p.fillRect(QRect(QPoint(r.x()+2, r.y()+2),
                             QPoint(titleX2-2, r.y()+18)),
                       options->color(Options::TitleBar, false));
        }
        r = titlebar->geometry();
        QFontMetrics fm(options->font(true)); // font(false) doesn't work yet.
        p.setFont(options->font(true));
        p.setPen(options->color(Options::Font, false));
        p.drawText(r.x(), r.y(), r.width(), r.height()-1,
                   AlignLeft, caption() );
    }
}

#define QCOORDARRLEN(x) sizeof(x)/(sizeof(QCOORD)*2)

void KDEClient::doShape()
{
    QRegion mask(QRect(0, 0, width(), height()));
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-1, 0, 1, 1);
    mask -= QRect(0, height()-1, 1, 1);
    mask -= QRect(width()-1, height()-1, 1, 1);

    setMask(mask);
}

void KDEClient::showEvent(QShowEvent *ev)
{
    Client::showEvent(ev);
    doShape();
    calcHiddenButtons();
    repaint();
}

void KDEClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}

void KDEClient::stickyChange(bool on)
{
    if(on){
        if(isActive())
            button[BtnSticky]->setPixmap(*pindownPix, *pindownPixMO);
        else
            button[BtnSticky]->setPixmap(*iPindownPix, *iPindownPixMO);

    }
    else{
        if(isActive())
            button[BtnSticky]->setPixmap(*pinupPix, *pinupPixMO);
        else
            button[BtnSticky]->setPixmap(*iPinupPix, *iPinupPixMO);
    }
}

void KDEClient::maximizeChange(bool m)
{
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
}

void KDEClient::init()
{
    //
}

void KDEClient::activeChange(bool)
{
    repaint(false);
    int i;
    stickyChange(isSticky()); // redo gradient
    if(!miniIcon().isNull()){
        button[BtnMenu]->setPixmap(miniIcon(), lightIcon); // reset gradient
    }
    else
        button[BtnMenu]->setPixmap(kdelogo, lightIcon);
    // TODO KLOGO reset?
    for(i=0; i < 6; ++i){
        if(button[i])
            button[i]->reset();
    }
}


void KDEClient::calcHiddenButtons()
{
    // order of hiding is help, sticky, maximize, minimize, close, then menu;
    // buttons can have
    int minWidth = 32 + 16*4 + (providesContextHelp() ? 16*2 :
                                       16);

    if(lastButtonWidth > width()){ // shrinking
        lastButtonWidth = width();
        if(width() < minWidth){
            hiddenItems = true;
            int i;
            for(i=0; i<6; ++i){
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
        for(i=5; i>=0; --i){
            if(button[i]){
                if(button[i]->sizeHint().width() + totalSize <= width()){
                    totalSize+=button[i]->sizeHint().width();
                    if(button[i]->isHidden() &&
		       ( !isTransient() || !isTransient() || i != BtnSticky ) &&
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

// not used right now
void KDEClient::updateActiveBuffer( )
{
    int titleX2 = width() - 6 -
        (button[BtnHelp] && button[BtnHelp]->isVisible() ? 16 : 0) -
        (button[BtnMax]->isVisible() ? 16 : 0) -
        (button[BtnIconify]->isVisible() ? 16 : 0) -
        (button[BtnClose]->isVisible() ? 16 : 0);

    if(!bufferDirty && (lastBufferWidth == titleX2))
        return;
    lastBufferWidth = titleX2;
    bufferDirty = false;

    activeBuffer.resize(titleX2-3, 16);
    QPainter p(&activeBuffer);
    p.drawTiledPixmap(0, 0, activeBuffer.width(), activeBuffer.height(),
                      *aUpperGradient);
    QRect r = titlebar->geometry();
    QFontMetrics fm(options->font(true));
    p.setFont(options->font(true));
    p.setPen(options->color(Options::Font, true));
    p.drawText(r.x(), r.y(), r.width(), r.height()-1,
               AlignLeft, caption() );
    p.drawTiledPixmap(r.x()+fm.width(caption()), 2,
                      r.width()-fm.width(caption())-2,
                      r.height(), *titlePix);
    p.end();
}

  Client::MousePosition
KDEClient::mousePosition( const QPoint& p ) const
{
  MousePosition m = Nowhere;

  if (p.y() < (height() - 7))
    m = Client::mousePosition(p);

  else {
    if (p.x() >= (width() - 20))
      m = BottomRight;
    else if (p.x() <= 20)
      m = BottomLeft;
    else
      m = Bottom;
  }

  return m;
}

void KDEClient::menuButtonPressed()
{
   //workspace()->clientPopup(this)->
   //     popup(button[BtnMenu]->mapToGlobal(button[BtnMenu]->
   //                                        rect().bottomLeft()));
    static QTime* t = 0;
    static KDEClient* tc = 0;
    if ( !t )
        t = new QTime;

    if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() ){
        workspace()->clientPopup(this)->
            popup(button[BtnMenu]->mapToGlobal(button[BtnMenu]->
                                               rect().bottomLeft()));
    }
    else {
        button[BtnMenu]->setPopup( 0 );
        closeWindow();
    }
    t->start();
    tc = this;
}

#include "kdedefault.moc"
