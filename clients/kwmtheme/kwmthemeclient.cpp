#include <kconfig.h>
#include "kwmthemeclient.h"
#include <kglobal.h>
#include <qlayout.h>
#include <qdrawutil.h>
#include <qpainter.h>
#include <kpixmapeffect.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <klocale.h>
#include <qbitmap.h>
#include <qstyle.h>
#include <qlabel.h>
#include <qtooltip.h>

namespace KWMTheme {


/* static QPixmap stretchPixmap(QPixmap& src, bool stretchVert){
  QPixmap dest;
  QBitmap *srcMask, *destMask;
  int w, h, w2, h2;
  QPainter p;

  if (src.isNull()) return src;

  w = src.width();
  h = src.height();

  if (stretchVert){
    w2 = w;
    for (h2=h; h2<100; h2=h2<<1)
      ;
  }
  else{
    h2 = h;
    for (w2=w; w2<100; w2=w2<<1)
      ;
  }
  if (w2==w && h2==h) return src;

  dest = src;
  dest.resize(w2, h2);

  p.begin(&dest);
  p.drawTiledPixmap(0, 0, w2, h2, src);
  p.end();

  srcMask = (QBitmap*)src.mask();
  if (srcMask){
    destMask = (QBitmap*)dest.mask();
    p.begin(destMask);
    p.drawTiledPixmap(0, 0, w2, h2, *srcMask);
    p.end();
  }
  return dest;
} */


inline const KDecorationOptions* options() { return KDecoration::options(); }

enum FramePixmap{FrameTop=0, FrameBottom, FrameLeft, FrameRight, FrameTopLeft,
   FrameTopRight, FrameBottomLeft, FrameBottomRight};

static QPixmap *framePixmaps[8];
static QPixmap *menuPix, *iconifyPix, *closePix, *maxPix, *minmaxPix,
    *pinupPix, *pindownPix;
static KPixmap *aTitlePix = 0;
static KPixmap *iTitlePix = 0;
static KPixmapEffect::GradientType grType;
static int maxExtent, titleAlign;
static bool titleGradient = true;
static bool pixmaps_created = false;
static bool titleSunken = false;
static bool titleTransparent;

static void create_pixmaps()
{
    const char *keys[] = {"wm_top", "wm_bottom", "wm_left", "wm_right",
    "wm_topleft", "wm_topright", "wm_bottomleft", "wm_bottomright"};

    if(pixmaps_created)
        return;
    pixmaps_created = true;
    
    KConfig *config = KGlobal::config();
    config->setGroup("General");
    QString tmpStr;

    for(int i=0; i < 8; ++i)
    {
        framePixmaps[i] = new QPixmap(locate("data",
                                      "kwin/pics/"+config->readEntry(keys[i], " ")));
        if(framePixmaps[i]->isNull())
            kdWarning() << "Unable to load frame pixmap for " << keys[i] << endl;
    }
/*
    *framePixmaps[FrameTop] = stretchPixmap(*framePixmaps[FrameTop], false);
    *framePixmaps[FrameBottom] = stretchPixmap(*framePixmaps[FrameBottom], false);
    *framePixmaps[FrameLeft] = stretchPixmap(*framePixmaps[FrameLeft], true);
    *framePixmaps[FrameRight] = stretchPixmap(*framePixmaps[FrameRight], true);
*/
    maxExtent = framePixmaps[FrameTop]->height();
    if(framePixmaps[FrameBottom]->height() > maxExtent)
        maxExtent = framePixmaps[FrameBottom]->height();
    if(framePixmaps[FrameLeft]->width() > maxExtent)
        maxExtent = framePixmaps[FrameLeft]->width();
    if(framePixmaps[FrameRight]->width() > maxExtent)
        maxExtent = framePixmaps[FrameRight]->width();

    maxExtent++;

    menuPix = new QPixmap(locate("data",
                                 "kwin/pics/"+config->readEntry("menu", " ")));
    iconifyPix = new QPixmap(locate("data",
                                    "kwin/pics/"+config->readEntry("iconify", " ")));
    maxPix = new QPixmap(locate("appdata",
                                "pics/"+config->readEntry("maximize", " ")));
    minmaxPix = new QPixmap(locate("data",
                                   "kwin/pics/"+config->readEntry("maximizedown", " ")));
    closePix = new QPixmap(locate("data",
                                  "kwin/pics/"+config->readEntry("close", " ")));
    pinupPix = new QPixmap(locate("data",
                                  "kwin/pics/"+config->readEntry("pinup", " ")));
    pindownPix = new QPixmap(locate("data",
                                    "kwin/pics/"+config->readEntry("pindown", " ")));
    if(menuPix->isNull())                           
        menuPix->load(locate("data", "kwin/pics/menu.png"));
    if(iconifyPix->isNull())
        iconifyPix->load(locate("data", "kwin/pics/iconify.png"));
    if(maxPix->isNull())
        maxPix->load(locate("data", "kwin/pics/maximize.png"));
    if(minmaxPix->isNull())
        minmaxPix->load(locate("data", "kwin/pics/maximizedown.png"));
    if(closePix->isNull())
        closePix->load(locate("data", "kwin/pics/close.png"));
    if(pinupPix->isNull())
        pinupPix->load(locate("data", "kwin/pics/pinup.png"));
    if(pindownPix->isNull())
        pindownPix->load(locate("data", "kwin/pics/pindown.png"));
    
    tmpStr = config->readEntry("TitleAlignment");
    if(tmpStr == "right")
        titleAlign = Qt::AlignRight | Qt::AlignVCenter;
    else if(tmpStr == "middle")
        titleAlign = Qt::AlignCenter;
    else
        titleAlign = Qt::AlignLeft | Qt::AlignVCenter;
    titleSunken = config->readBoolEntry("TitleFrameShaded", true);
    // titleSunken = true; // is this fixed?
    titleTransparent = config->readBoolEntry("PixmapUnderTitleText", true);

    tmpStr = config->readEntry("TitlebarLook");
    if(tmpStr == "shadedVertical"){
        aTitlePix = new KPixmap;
        aTitlePix->resize(32, 20);
        KPixmapEffect::gradient(*aTitlePix,
                                options()->color(KDecorationOptions::ColorTitleBar, true),
                                options()->color(KDecorationOptions::ColorTitleBlend, true),
                                KPixmapEffect::VerticalGradient);
        iTitlePix = new KPixmap;
        iTitlePix->resize(32, 20);
        KPixmapEffect::gradient(*iTitlePix,
                                options()->color(KDecorationOptions::ColorTitleBar, false),
                                options()->color(KDecorationOptions::ColorTitleBlend, false),
                                KPixmapEffect::VerticalGradient);
        titleGradient = false; // we can just tile this

    }
    else if(tmpStr == "shadedHorizontal")
        grType = KPixmapEffect::HorizontalGradient;
    else if(tmpStr == "shadedDiagonal")
        grType = KPixmapEffect::DiagonalGradient;
    else if(tmpStr == "shadedCrossDiagonal")
        grType = KPixmapEffect::CrossDiagonalGradient;
    else if(tmpStr == "shadedPyramid")
        grType = KPixmapEffect::PyramidGradient;
    else if(tmpStr == "shadedRectangle")
        grType = KPixmapEffect::RectangleGradient;
    else if(tmpStr == "shadedPipeCross")
        grType = KPixmapEffect::PipeCrossGradient;
    else if(tmpStr == "shadedElliptic")
        grType = KPixmapEffect::EllipticGradient;
    else{
        titleGradient = false;
        tmpStr = config->readEntry("TitlebarPixmapActive", "");
        if(!tmpStr.isEmpty()){
            aTitlePix = new KPixmap;
            aTitlePix->load(locate("data", "kwin/pics/" + tmpStr));
        }
        else
            aTitlePix = NULL;
        tmpStr = config->readEntry("TitlebarPixmapInactive", "");
        if(!tmpStr.isEmpty()){
            iTitlePix = new KPixmap;
            iTitlePix->load(locate("data", "kwin/pics/" + tmpStr));
        }
        else
            iTitlePix = NULL;
    }
}

static void delete_pixmaps()
{
   for(int i=0; i < 8; ++i)
        delete framePixmaps[i];

   delete menuPix;
   delete iconifyPix;
   delete closePix;
   delete maxPix;
   delete minmaxPix;
   delete pinupPix;
   delete pindownPix;
   delete aTitlePix;
   aTitlePix = 0;
   delete iTitlePix;
   iTitlePix = 0;

   titleGradient = true;
   pixmaps_created = false;
   titleSunken = false;
}

void MyButton::drawButtonLabel(QPainter *p)
{
    if(pixmap()){
	// If we have a theme who's button covers the entire width or
	// entire height, we shift down/right by 1 pixel so we have
	// some visual notification of button presses. i.e. for MGBriezh
	int offset = (isDown() && ((pixmap()->width() >= width()) || 
                         (pixmap()->height() >= height()))) ? 1 : 0;
        style().drawItem(p, QRect( offset, offset, width(), height() ), 
                         AlignCenter, colorGroup(),
                         true, pixmap(), QString());
    }
}

KWMThemeClient::KWMThemeClient( KDecorationBridge* b, KDecorationFactory* f )
    : KDecoration( b, f )
{
}

void KWMThemeClient::init()
{
    createMainWidget( WResizeNoErase | WStaticContents );
    widget()->installEventFilter( this );

    stickyBtn = maxBtn = mnuBtn = 0;
    layout = new QGridLayout(widget());
    layout->addColSpacing(0, maxExtent);
    layout->addColSpacing(2, maxExtent);

    layout->addRowSpacing(0, maxExtent);

    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed,
                                    QSizePolicy::Expanding));

    if( isPreview())
        layout->addWidget( new QLabel( i18n( "<center><b>KWMTheme</b></center>" ), widget()), 2, 1);
    else
        layout->addItem( new QSpacerItem( 0, 0 ), 2, 1);

    // Without the next line, shading flickers
    layout->addItem( new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding) );
    layout->addRowSpacing(3, maxExtent);
    layout->setRowStretch(2, 10);
    layout->setColStretch(1, 10);
    
    QBoxLayout* hb = new QBoxLayout(0, QBoxLayout::LeftToRight, 0, 0, 0);
    layout->addLayout( hb, 1, 1 );

    KConfig *config = KGlobal::config();
    config->setGroup("Buttons");
    QString val;
    MyButton *btn;
    int i;
    static const char *defaultButtons[]={"Menu","Sticky","Off","Iconify",
        "Maximize","Close"};
    static const char keyOffsets[]={"ABCDEF"};
    for(i=0; i < 6; ++i){
        if(i == 3){
            titlebar = new QSpacerItem(10, 20, QSizePolicy::Expanding,
                               QSizePolicy::Minimum );
            hb->addItem( titlebar );
        }
        QString key("Button");
        key += QChar(keyOffsets[i]);
        val = config->readEntry(key, defaultButtons[i]);
        if(val == "Menu"){
            mnuBtn = new MyButton(widget(), "menu");
            mnuBtn->setToolTip( i18n("Menu"));
            iconChange();
            hb->addWidget(mnuBtn);
            mnuBtn->setFixedSize(20, 20);
            connect(mnuBtn, SIGNAL(pressed()), this,
                    SLOT(menuButtonPressed()));
        }
        else if(val == "Sticky"){
            stickyBtn = new MyButton(widget(), "sticky");
            stickyBtn->setToolTip( i18n("Sticky"));
            if (isOnAllDesktops())
                stickyBtn->setPixmap(*pindownPix);
            else
                stickyBtn->setPixmap(*pinupPix);
            connect(stickyBtn, SIGNAL( clicked() ), this, SLOT(toggleOnAllDesktops()));
            hb->addWidget(stickyBtn);
            stickyBtn->setFixedSize(20, 20);
        }
        else if((val == "Iconify") && isMinimizable()){
            btn = new MyButton(widget(), "iconify");
            btn->setToolTip( i18n("Minimize"));
            btn->setPixmap(*iconifyPix);
            connect(btn, SIGNAL(clicked()), this, SLOT(minimize()));
            hb->addWidget(btn);
            btn->setFixedSize(20, 20);
        }
        else if((val == "Maximize") && isMaximizable()){
            maxBtn = new MyButton(widget(), "max");
            maxBtn->setToolTip( i18n("Maximize"));
            maxBtn->setPixmap(*maxPix);
            connect(maxBtn, SIGNAL(clicked()), this, SLOT(maximize()));
            hb->addWidget(maxBtn);
            maxBtn->setFixedSize(20, 20);
        }
        else if((val == "Close") && isCloseable()){
            btn = new MyButton(widget(), "close");
            btn->setToolTip( i18n("Close"));
            btn->setPixmap(*closePix);
            connect(btn, SIGNAL(clicked()), this, SLOT(closeWindow()));
            hb->addWidget(btn);
            btn->setFixedSize(20, 20);
        }
        else{
            if((val != "Off") && 
               ((val == "Iconify") && !isMinimizable()) &&
               ((val == "Maximize") && !isMaximizable()))
                kdWarning() << "KWin: Unrecognized button value: " << val << endl;

        }
    }
    if(titleGradient){
        aGradient = new KPixmap;
        iGradient = new KPixmap;
    }
    else{
        aGradient = 0;
        iGradient = 0;
    }
    widget()->setBackgroundMode(NoBackground);
}

void KWMThemeClient::drawTitle(QPainter &dest)
{
    QRect titleRect = titlebar->geometry();
    QRect r(0, 0, titleRect.width(), titleRect.height());
    QPixmap buffer;

    if(buffer.width() == r.width())
        return;

    buffer.resize(r.size());
    QPainter p;
    p.begin(&buffer);

    if(titleSunken){
        qDrawShadeRect(&p, r, options()->colorGroup(KDecorationOptions::ColorFrame, isActive()),
                       true, 1, 0);
        r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);
    }
    
    KPixmap *fill = isActive() ? aTitlePix : iTitlePix;
    if(fill)
        p.drawTiledPixmap(r, *fill);
    else if(titleGradient){
        fill = isActive() ? aGradient : iGradient;
        if(fill->width() != r.width()){
            fill->resize(r.width(), 20);
            KPixmapEffect::gradient(*fill,
                                    options()->color(KDecorationOptions::ColorTitleBar, isActive()),
                                    options()->color(KDecorationOptions::ColorTitleBlend, isActive()),
                                    grType);
        }
        p.drawTiledPixmap(r, *fill);
    }
    else{
        p.fillRect(r, options()->colorGroup(KDecorationOptions::ColorTitleBar, isActive()).
                   brush(QColorGroup::Button));
    }
    p.setFont(options()->font(isActive()));
    p.setPen(options()->color(KDecorationOptions::ColorFont, isActive()));
    // Add left & right margin
    r.setLeft(r.left()+5);
    r.setRight(r.right()-5);
    p.drawText(r, titleAlign, caption());
    p.end();

    dest.drawPixmap(titleRect.x(), titleRect.y(), buffer);
}


void KWMThemeClient::resizeEvent( QResizeEvent* )
{
    doShape();
    widget()->repaint();
}

void KWMThemeClient::captionChange()
{
    widget()->repaint( titlebar->geometry(), false );
}

void KWMThemeClient::paintEvent( QPaintEvent *)
{
    QPainter p;
    p.begin(widget());
    int x,y;
    // first the corners
    int w1 = framePixmaps[FrameTopLeft]->width();
    int h1 = framePixmaps[FrameTopLeft]->height();
    if (w1 > width()/2) w1 = width()/2;
    if (h1 > height()/2) h1 = height()/2;
    p.drawPixmap(0,0,*framePixmaps[FrameTopLeft],
		 0,0,w1, h1);
    int w2 = framePixmaps[FrameTopRight]->width();
    int h2 = framePixmaps[FrameTopRight]->height();
    if (w2 > width()/2) w2 = width()/2;
    if (h2 > height()/2) h2 = height()/2;
    p.drawPixmap(width()-w2,0,*framePixmaps[FrameTopRight],
		 framePixmaps[FrameTopRight]->width()-w2,0,w2, h2);

    int w3 = framePixmaps[FrameBottomLeft]->width();
    int h3 = framePixmaps[FrameBottomLeft]->height();
    if (w3 > width()/2) w3 = width()/2;
    if (h3 > height()/2) h3 = height()/2;
    p.drawPixmap(0,height()-h3,*framePixmaps[FrameBottomLeft],
		 0,framePixmaps[FrameBottomLeft]->height()-h3,w3, h3);

    int w4 = framePixmaps[FrameBottomRight]->width();
    int h4 = framePixmaps[FrameBottomRight]->height();
    if (w4 > width()/2) w4 = width()/2;
    if (h4 > height()/2) h4 = height()/2;
    p.drawPixmap(width()-w4,height()-h4,*(framePixmaps[FrameBottomRight]),
		 framePixmaps[FrameBottomRight]->width()-w4,
		 framePixmaps[FrameBottomRight]->height()-h4,
		 w4, h4);

    QPixmap pm;
    QMatrix m;
    int n,s,w;
    //top
    pm = *framePixmaps[FrameTop];

    if (pm.width() > 0){
    s = width()-w2-w1;
    n = s/pm.width();
    w = n>0?s/n:s;
    m.reset();
    m.scale(w/(float)pm.width(), 1);
    pm = pm.transformed(m);

    x = w1;
    while (1){
      if (pm.width() < width()-w2-x){
	p.drawPixmap(x,maxExtent-pm.height()-1,
		     pm);
	x += pm.width();
      }
      else {
	p.drawPixmap(x,maxExtent-pm.height()-1,
		     pm,
		     0,0,width()-w2-x,pm.height());
	break;
      }
    }
    }

    //bottom
    pm = *framePixmaps[FrameBottom];

    if (pm.width() > 0){
    s = width()-w4-w3;
    n = s/pm.width();
    w = n>0?s/n:s;
    m.reset();
    m.scale(w/(float)pm.width(), 1);
    pm = pm.transformed(m);

    x = w3;
    while (1){
      if (pm.width() < width()-w4-x){
	p.drawPixmap(x,height()-maxExtent+1,pm);
	x += pm.width();
      }
      else {
	p.drawPixmap(x,height()-maxExtent+1,pm,
		     0,0,width()-w4-x,pm.height());
	break;
      }
    }
    }

    //left
    pm = *framePixmaps[FrameLeft];

    if (pm.height() > 0){
    s = height()-h3-h1;
    n = s/pm.height();
    w = n>0?s/n:s;
    m.reset();
    m.scale(1, w/(float)pm.height());
    pm = pm.transformed(m);

    y = h1;
    while (1){
      if (pm.height() < height()-h3-y){
	p.drawPixmap(maxExtent-pm.width()-1, y,
		     pm);
	y += pm.height();
      }
      else {
	p.drawPixmap(maxExtent-pm.width()-1, y,
		     pm,
		     0,0, pm.width(),
		     height()-h3-y);
	break;
      }
    }
    }

    //right
    pm = *framePixmaps[FrameRight];

    if (pm.height() > 0){
    s = height()-h4-h2;
    n = s/pm.height();
    w = n>0?s/n:s;
    m.reset();
    m.scale(1, w/(float)pm.height());
    pm = pm.transformed(m);

    y = h2;
    while (1){
      if (pm.height() < height()-h4-y){
	p.drawPixmap(width()-maxExtent+1, y,
		     pm);
	y += pm.height();
      }
      else {
	p.drawPixmap(width()-maxExtent+1, y,
		     pm,
		     0,0, pm.width(),
		     height()-h4-y);
	break;
      }
    }
    }
    drawTitle(p);

    QColor c = widget()->colorGroup().background();

    // KWM evidently had a 1 pixel border around the client window. We
    // emulate it here, but should be removed at some point in order to
    // seamlessly mesh widget themes
    p.setPen(c);
    p.drawRect(maxExtent-1, maxExtent-1, width()-(maxExtent-1)*2,
               height()-(maxExtent-1)*2);

    // We fill the area behind the wrapped widget to ensure that
    // shading animation is drawn as smoothly as possible
    QRect r(layout->cellGeometry(2, 1));
    p.fillRect( r.x(), r.y(), r.width(), r.height(), c);
    p.end();
}

void KWMThemeClient::doShape()
{

    QBitmap shapemask(width(), height());
    shapemask.fill(color0);
    QPainter p;
    p.begin(&shapemask);
    p.setBrush(color1);
    p.setPen(color1);
    int x,y;
    // first the corners
    int w1 = framePixmaps[FrameTopLeft]->width();
    int h1 = framePixmaps[FrameTopLeft]->height();
    if (w1 > width()/2) w1 = width()/2;
    if (h1 > height()/2) h1 = height()/2;
    if (framePixmaps[FrameTopLeft]->mask())
        p.drawPixmap(0,0,*framePixmaps[FrameTopLeft]->mask(),
		     0,0,w1, h1);
    else
        p.fillRect(0,0,w1,h1,color1);
    int w2 = framePixmaps[FrameTopRight]->width();
    int h2 = framePixmaps[FrameTopRight]->height();
    if (w2 > width()/2) w2 = width()/2;
    if (h2 > height()/2) h2 = height()/2;
    if (framePixmaps[FrameTopRight]->mask())
        p.drawPixmap(width()-w2,0,*framePixmaps[FrameTopRight]->mask(),
		     framePixmaps[FrameTopRight]->width()-w2,0,w2, h2);
    else
        p.fillRect(width()-w2,0,w2, h2,color1);

    int w3 = framePixmaps[FrameBottomLeft]->width();
    int h3 = framePixmaps[FrameBottomLeft]->height();
    if (w3 > width()/2) w3 = width()/2;
    if (h3 > height()/2) h3 = height()/2;
    if (framePixmaps[FrameBottomLeft]->mask())
        p.drawPixmap(0,height()-h3,*framePixmaps[FrameBottomLeft]->mask(),
		     0,framePixmaps[FrameBottomLeft]->height()-h3,w3, h3);
    else
        p.fillRect(0,height()-h3,w3,h3,color1);

    int w4 = framePixmaps[FrameBottomRight]->width();
    int h4 = framePixmaps[FrameBottomRight]->height();
    if (w4 > width()/2) w4 = width()/2;
    if (h4 > height()/2) h4 = height()/2;
    if (framePixmaps[FrameBottomRight]->mask())
        p.drawPixmap(width()-w4,height()-h4,*framePixmaps[FrameBottomRight]->mask(),
		     framePixmaps[FrameBottomRight]->width()-w4,
		     framePixmaps[FrameBottomRight]->height()-h4,
		     w4, h4);
    else
        p.fillRect(width()-w4,height()-h4,w4,h4,color1);

    QPixmap pm;
    QMatrix m;
    int n,s,w;
    //top
    if (framePixmaps[FrameTop]->mask())
    {
        pm = *framePixmaps[FrameTop]->mask();

        s = width()-w2-w1;
        n = s/pm.width();
        w = n>0?s/n:s;
        m.reset();
        m.scale(w/(float)pm.width(), 1);
        pm = pm.transformed(m);

        x = w1;
        while (1){
            if (pm.width() < width()-w2-x){
                p.drawPixmap(x,maxExtent-pm.height()-1,
		             pm);
	        x += pm.width();
            }
            else {
	        p.drawPixmap(x,maxExtent-pm.height()-1,
		             pm,
		             0,0,width()-w2-x,pm.height());
	        break;
            }
        }
    }

    //bottom
    if (framePixmaps[FrameBottom]->mask())
    {
        pm = *framePixmaps[FrameBottom]->mask();

        s = width()-w4-w3;
        n = s/pm.width();
        w = n>0?s/n:s;
        m.reset();
        m.scale(w/(float)pm.width(), 1);
        pm = pm.transformed(m);

        x = w3;
        while (1){
          if (pm.width() < width()-w4-x){
	    p.drawPixmap(x,height()-maxExtent+1,pm);
	    x += pm.width();
          }
          else {
	    p.drawPixmap(x,height()-maxExtent+1,pm,
	    		 0,0,width()-w4-x,pm.height());
	    break;
          }
        }
    }

    //left
    if (framePixmaps[FrameLeft]->mask())
    {
        pm = *framePixmaps[FrameLeft]->mask();

        s = height()-h3-h1;
        n = s/pm.height();
        w = n>0?s/n:s;
        m.reset();
        m.scale(1, w/(float)pm.height());
        pm = pm.transformed(m);

        y = h1;
        while (1){
          if (pm.height() < height()-h3-y){
	    p.drawPixmap(maxExtent-pm.width()-1, y,
		         pm);
	    y += pm.height();
          }
          else {
	    p.drawPixmap(maxExtent-pm.width()-1, y,
		         pm,
		         0,0, pm.width(),
		         height()-h3-y);
	    break;
          }
        }
    }

    //right
    if (framePixmaps[FrameRight]->mask())
    {
        pm = *framePixmaps[FrameRight]->mask();

        s = height()-h4-h2;
        n = s/pm.height();
        w = n>0?s/n:s;
        m.reset();
        m.scale(1, w/(float)pm.height());
        pm = pm.transformed(m);

        y = h2;
        while (1){
          if (pm.height() < height()-h4-y){
	    p.drawPixmap(width()-maxExtent+1, y,
		         pm);
	    y += pm.height();
          }
          else {
	    p.drawPixmap(width()-maxExtent+1, y,
		         pm,
		         0,0, pm.width(),
		         height()-h4-y);
	    break;
          }
        }
    }
    p.fillRect(maxExtent-1, maxExtent-1, width()-2*maxExtent+2, height()-2*maxExtent+2, color1);
    setMask(shapemask);
}


void KWMThemeClient::showEvent(QShowEvent *)
{
    doShape();
    widget()->repaint(false);
}

void KWMThemeClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (e->button() == LeftButton && titlebar->geometry().contains( e->pos() ) )
        titlebarDblClickOperation();
}

void KWMThemeClient::desktopChange()
{
    if (stickyBtn) {
       bool on = isOnAllDesktops();
       stickyBtn->setPixmap(on ? *pindownPix : *pinupPix);
       QToolTip::remove( stickyBtn );
       stickyBtn->setToolTip( on ? i18n("Unsticky") : i18n("Sticky") );
    }
}

void KWMThemeClient::maximizeChange()
{
    if (maxBtn) {
       bool m = maximizeMode() == MaximizeFull;
       maxBtn->setPixmap(m ? *minmaxPix : *maxPix);
       QToolTip::remove( maxBtn );
       maxBtn->setToolTip( m ? i18n("Restore") : i18n("Maximize"));
    }
}

void KWMThemeClient::slotMaximize()
{
    maximize( maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull );
}

void KWMThemeClient::activeChange()
{
    widget()->update();
}

KDecoration::Position KWMThemeClient::mousePosition(const QPoint &p) const
{
    Position m = KDecoration::mousePosition(p);
    // corners
    if(p.y() < framePixmaps[FrameTop]->height() &&
       p.x() < framePixmaps[FrameLeft]->width()){
        m = PositionTopLeft;
    }
    else if(p.y() < framePixmaps[FrameTop]->height() &&
            p.x() > width()-framePixmaps[FrameRight]->width()){
        m = PositionTopRight;
    }
    else if(p.y() > height()-framePixmaps[FrameBottom]->height() &&
            p.x() < framePixmaps[FrameLeft]->width()){
        m = PositionBottomLeft;
    }
    else if(p.y() > height()-framePixmaps[FrameBottom]->height() &&
            p.x() > width()-framePixmaps[FrameRight]->width()){
        m = PositionBottomRight;
    } // edges
    else if(p.y() < framePixmaps[FrameTop]->height())
        m = PositionTop;
    else if(p.y() > height()-framePixmaps[FrameBottom]->height())
        m = PositionBottom;
    else if(p.x()  < framePixmaps[FrameLeft]->width())
        m = PositionLeft;
    else if(p.x() > width()-framePixmaps[FrameRight]->width())
        m = PositionRight;
    return(m);
}

void KWMThemeClient::menuButtonPressed()
{
    mnuBtn->setDown(false); // will stay down if I don't do this
    QPoint pos = mnuBtn->mapToGlobal(mnuBtn->rect().bottomLeft());
    showWindowMenu( pos );
}

void KWMThemeClient::iconChange()
{
    if(mnuBtn){
        if( icon().pixmap( QIcon::Small, QIcon::Normal ).isNull()){
            mnuBtn->setPixmap(*menuPix);
        }
        else{
            mnuBtn->setPixmap(icon().pixmap( QIcon::Small, QIcon::Normal ));
        }
    }
}

bool KWMThemeClient::eventFilter( QObject* o, QEvent* e )
{
	if ( o != widget() )
		return false;

	switch ( e->type() )
	{
		case QEvent::Resize:
			resizeEvent( static_cast< QResizeEvent* >( e ) );
			return true;

		case QEvent::Paint:
			paintEvent( static_cast< QPaintEvent* >( e ) );
			return true;

		case QEvent::MouseButtonDblClick:
			mouseDoubleClickEvent( static_cast< QMouseEvent* >( e ) );
			return true;

		case QEvent::MouseButtonPress:
			processMousePressEvent( static_cast< QMouseEvent* >( e ) );
			return true;

		case QEvent::Show:
			showEvent( static_cast< QShowEvent* >( e ) );
			return true;

		default:
	    		return false;
	}
}

QSize KWMThemeClient::minimumSize() const
{
    return widget()->minimumSize().expandedTo( QSize( 100, 50 ));
}

void KWMThemeClient::resize( const QSize& s )
{
    widget()->resize( s );
}

void KWMThemeClient::borders( int& left, int& right, int& top, int& bottom ) const
{
    left =
    right =
    top =
    bottom =
    
TODO
}

KWMThemeFactory::KWMThemeFactory()
{
    create_pixmaps();
}

KWMThemeFactory::~KWMThemeFactory()
{
    delete_pixmaps();
}

KDecoration* KWMThemeFactory::createDecoration( KDecorationBridge* b )
{
    return new KWMThemeClient( b, this );
}

bool KWMThemeFactory::reset( unsigned long mask )
{
    bool needHardReset = false;

TODO

    // doesn't obey the Border size setting
    if( mask & ( SettingFont | SettingButtons ))
        needHardReset = true;

    if( mask & ( SettingFont | SettingColors )) {
        KWMTheme::delete_pixmaps();
        KWMTheme::create_pixmaps();
    }
    
    if( !needHardReset )
        resetDecorations( mask );
    return needHardReset;
}

}

extern "C"
{
	KDE_EXPORT KDecorationFactory *create_factory()
	{
                return new KWMTheme::KWMThemeFactory();
	}
}

#include "kwmthemeclient.moc"
