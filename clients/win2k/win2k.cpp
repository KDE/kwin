/*
  Gallium-Win2k KWin client

  Copyright 2001
    Karol Szwed <karlmail@usa.net>
    http://gallium.n3.net/

  Based on the default KWin client.

  Major code cleanups, bug fixes and updates to support toolwindows 3/2001 - KS
*/

#include "win2k.h"  
#include <qlayout.h>
#include <qdrawutil.h>
#include <qdatetime.h>
#include <kpixmapeffect.h>
#include <kdrawutil.h>
#include <qbitmap.h>
#include "../../workspace.h"
#include "../../options.h"

using namespace KWinInternal;

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int)
    {
        return(new GalliumClient(ws, w));
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
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00};

static unsigned char close_bits[] = {
  0x00, 0x00, 0x86, 0x01, 0xcc, 0x00, 0x78, 0x00, 0x30, 0x00, 0x78, 0x00,
  0xcc, 0x00, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00};

static unsigned char maximize_bits[] = {
  0xff, 0x01, 0xff, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0xff, 0x01, 0x00, 0x00};

static unsigned char minmax_bits[] = {
  0xfc, 0x00, 0xfc, 0x00, 0x84, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0xe1, 0x00,
  0x21, 0x00, 0x21, 0x00, 0x3f, 0x00, 0x00, 0x00};

static unsigned char question_bits[] = {
  0x00, 0x00, 0x3c, 0x00, 0x66, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00,
  0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00};


// Up / Down titlebar button images
// Since we have no idea when this client will be unloaded from kwin, there is
// no way to free dynamic memory. Hence, these pixmaps must be static.
static KPixmap btnPix1; 
static KPixmap iBtnPix1;
static KPixmap btnDownPix1;
static KPixmap iBtnDownPix1;

static KPixmap miniBtnPix1; 
static KPixmap iMiniBtnPix1;
static KPixmap miniBtnDownPix1;
static KPixmap iMiniBtnDownPix1;

static QPixmap defaultMenuPix(kdelogo);
static QColor  btnForeground;
static bool    pixmaps_created = false;


static void drawButtonFrame( KPixmap *pix, const QColorGroup &g, bool sunken )
{
    QPainter p;
    int x2 = pix->width() - 1;
    int y2 = pix->height() - 1;
    p.begin(pix);

    // Win2k titlebar button frame
    p.setPen( sunken ? Qt::black : g.light());
    p.drawLine(0, 0, x2-1, 0);
    p.drawLine(0, 0, 0, y2-1);

    if (sunken)
    {
       p.setPen( g.dark() );
       p.drawLine(1, 1, x2-2, 1);
       p.drawLine(1, 1, 1, y2-2);
    }

    p.setPen( sunken ? g.light() : g.dark());
    p.drawLine(1, y2-1, x2-1, y2-1);
    p.drawLine(x2-1, 1, x2-1, y2-1);

    p.setPen( sunken ? g.light() : Qt::black);
    p.drawLine(0, y2, x2, y2);
    p.drawLine(x2, 0, x2, y2);
}


static void create_pixmaps( )
{
    if(pixmaps_created)
        return;

    pixmaps_created = true;

    bool highcolor = QPixmap::defaultDepth() > 8;

    // buttons (active/inactive, sunken/unsunken)
    QColorGroup g = options->colorGroup(Options::ButtonBg, true);
    QColor c = g.background();
    btnPix1.resize(16, 14);
    btnDownPix1.resize(16, 14);
    iBtnPix1.resize(16, 14);
    iBtnDownPix1.resize(16, 14);

    miniBtnPix1.resize(12, 12);
    miniBtnDownPix1.resize(12, 12);
    iMiniBtnPix1.resize(12, 12);
    iMiniBtnDownPix1.resize(12, 12);

    // Give Win2k what it never had ;)
    if(highcolor)
    {
        KPixmapEffect::gradient(btnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(btnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);

        KPixmapEffect::gradient(miniBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(miniBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);

        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        KPixmapEffect::gradient(iBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(iBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(iMiniBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(iMiniBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
    }
    else
    {
        btnPix1.fill(c.rgb());
        btnDownPix1.fill(c.rgb());
        miniBtnPix1.fill(c.rgb());
        miniBtnDownPix1.fill(c.rgb());

        g = options->colorGroup(Options::ButtonBg, false);
        c = g.background();
        iBtnPix1.fill(c.rgb());
        iBtnDownPix1.fill(c.rgb());
        iMiniBtnPix1.fill(c.rgb());
        iMiniBtnDownPix1.fill(c.rgb());
    }

    g = options->colorGroup(Options::ButtonBg, true);   
    drawButtonFrame(&btnPix1, g, false);
    drawButtonFrame(&btnDownPix1, g, true);
    drawButtonFrame(&miniBtnPix1, g, false);
    drawButtonFrame(&miniBtnDownPix1, g, true);

    g = options->colorGroup(Options::ButtonBg, false);   
    drawButtonFrame(&iBtnPix1, g, false);
    drawButtonFrame(&iBtnDownPix1, g, true);
    drawButtonFrame(&iMiniBtnPix1, g, false);
    drawButtonFrame(&iMiniBtnDownPix1, g, true);

    // Make sure button pixmaps contrast with the current colour scheme.
    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 127)
        btnForeground = Qt::black;
    else
        btnForeground = Qt::white;
}


GalliumButton::GalliumButton(Client *parent, const char *name,
                           const unsigned char *bitmap, bool menuButton, bool isMini)
    : QButton(parent, name, WStyle_Customize | WRepaintNoErase |
                            WResizeNoErase | WStyle_NoBorder )
{
    // Eliminate background flicker
    setBackgroundMode( QWidget::NoBackground ); 

    client = parent;

    menuBtn = menuButton;
    miniBtn = isMini;

    // Use larger button for the menu, or mini-buttons for toolwindows.
    if ( isMini ) 
    {
       setFixedSize(12, 12);
       resize(12, 12);
    } else
       if ( menuButton ) 
       {
          setFixedSize(16, 16);
          resize(16, 16);
       }
       else 
       {
          setFixedSize(16, 14);
          resize(16, 14);
       }

    if(bitmap)
        setBitmap(bitmap);
}


QSize GalliumButton::sizeHint() const
{
   if ( miniBtn )
      return(QSize(12, 12));

   if ( menuBtn )
      return(QSize(16, 16));
   else
      return(QSize(16, 14));
}


void GalliumButton::reset()
{
    repaint(false);
}


void GalliumButton::setBitmap(const unsigned char *bitmap)
{
    pix.resize(0, 0);
    deco = QBitmap(10, 10, bitmap, true);
    deco.setMask(deco);
    repaint( false );   
}


void GalliumButton::setPixmap( const QPixmap &p )
{
    deco.resize(0, 0);
    pix = p;

    if (miniBtn)
       setMask(QRect(0, 0, 12, 12));
    else if (menuBtn)
       setMask(QRect(0, 0, 16, 16));
    else
       setMask(QRect(0, 0, 16, 14));

    repaint( false );   
}


void GalliumButton::drawButton(QPainter *p)
{
    if(pix.isNull())
    {
        if(client->isActive())
        {
           if(isDown())
               p->drawPixmap(0, 0, miniBtn ? miniBtnDownPix1 : btnDownPix1);
           else
               p->drawPixmap(0, 0, miniBtn ? miniBtnPix1 : btnPix1);
         }
         else
           {
              if(isDown())
                  p->drawPixmap(0, 0, miniBtn ? iMiniBtnDownPix1 : iBtnDownPix1);
              else
                  p->drawPixmap(0, 0, miniBtn ? iMiniBtnPix1 : iBtnPix1);
           }

        p->setPen( btnForeground );
        int xOff = (width()-10)/2;
        int yOff = (height()-10)/2;
        p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, deco);
    }
    else
    {
        p->fillRect(0, 0, width(), height(),
                    options->color(Options::TitleBar, client->isActive()));

        if ( menuBtn && miniBtn )
        {
           QPixmap tmpPix; 

           // Smooth scale the menu button pixmap
           tmpPix.convertFromImage( pix.convertToImage().smoothScale(12, 12));

           p->drawPixmap( 0, 0, tmpPix );            
        } else
            p->drawPixmap( 0, 0, pix );
    }
}


GalliumClient::GalliumClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WNorthWestGravity |
                                   WRepaintNoErase )
{
    setBackgroundMode( QWidget::NoBackground );

    // Finally, toolwindows look small
    if ( isTool() ) {
       titleHeight = 14;
       smallButtons = true;
    } else {
       titleHeight = 18;
       smallButtons = false;
    }

    lastButtonWidth = 0;

    create_pixmaps();

    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 4);        // Top grab bar 
    g->addWidget(windowWrapper(), 3, 1);
    g->setRowStretch(3, 10);      // Wrapped window
    g->addRowSpacing(4, 4);       // bottom handles
    g->addRowSpacing(2, 1);       // Line below title bar
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    button[BtnMenu]    = new GalliumButton(this, "menu", NULL, true, smallButtons);
    button[BtnClose]   = new GalliumButton(this, "close", close_bits, false, smallButtons);
    button[BtnIconify] = new GalliumButton(this, "iconify", iconify_bits, false, smallButtons);
    button[BtnMax]     = new GalliumButton(this, "maximize", maximize_bits, false, smallButtons);
    
    // Connect required stuff together
    connect( button[BtnMenu],    SIGNAL(pressed()),      this, SLOT( menuButtonPressed() ));
    connect( button[BtnClose],   SIGNAL( clicked() ),    this, SLOT( closeWindow() ));
    connect( button[BtnIconify], SIGNAL( clicked() ),    this, SLOT( iconify() ));
    connect( button[BtnMax],     SIGNAL( clicked() ),    this, SLOT( slotMaximize() ));
    connect( options,            SIGNAL(resetClients()), this, SLOT( slotReset() ));

    // Pack the titleBar hbox with items
    hb = new QHBoxLayout();
    hb->setResizeMode(QLayout::FreeResize);
    g->addLayout( hb, 1, 1 );
    hb->addSpacing(2);
    hb->addWidget( button[BtnMenu] );  
    titlebar = new QSpacerItem(10, titleHeight, QSizePolicy::Expanding, QSizePolicy::Minimum);
    hb->addItem(titlebar);
    hb->addSpacing(2);

    if( providesContextHelp() )
    {
        button[BtnHelp] = new GalliumButton(this, "help", question_bits, false, smallButtons);
        connect( button[BtnHelp], SIGNAL( clicked() ), this, SLOT( contextHelp() ));
        hb->addWidget( button[BtnHelp] );
    }
    else
        button[BtnHelp] = NULL;

    hb->addWidget( button[BtnIconify] );
    hb->addWidget( button[BtnMax] );
    hb->addSpacing(2);
    hb->addWidget( button[BtnClose] );
    hb->addSpacing(2);

    // Hide buttons which are not required
    // We can un-hide them if required later
    if ( !isMinimizable() )
        button[BtnIconify]->hide();
    if ( !isMaximizable() )
        button[BtnMax]->hide();

    hiddenItems = false;

    // Make sure that the menu button uses the correct mini-icon
    iconChange();
}


void GalliumClient::slotReset()
{
    pixmaps_created = false;
    create_pixmaps();

    // 0 to 3   ( 4 buttons - Help, Max, Iconify, Close )
    for(int i = GalliumClient::BtnHelp; i <= GalliumClient::BtnClose; i++)
        if(button[i])
            button[i]->reset();

    // The menu is reset by iconChange()

    repaint( false );  
}


void GalliumClient::iconChange()
{
    if(!miniIcon().isNull())
        button[BtnMenu]->setPixmap(miniIcon());
    else
        button[BtnMenu]->setPixmap(defaultMenuPix);

    if (button[BtnMenu]->isVisible())
       button[BtnMenu]->repaint(false);
}


void GalliumClient::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
       maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
       maximize( MaximizeHorizontal );
    else
       maximize();
}


void GalliumClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

    calcHiddenButtons();

    if (isVisibleToTLW()) 
    {
        update(rect());
        int dx = 0;
        int dy = 0;

	    if ( e->oldSize().width() != width() )
	       dx = 32 + QABS( e->oldSize().width() -  width() );

	    if ( e->oldSize().height() != height() )
	       dy = 8 + QABS( e->oldSize().height() -  height() );

	    if ( dy )
	       update( 0, height() - dy + 1, width(), dy );

        if ( dx ) 
        {
	       update( width() - dx + 1, 0, dx, height() );
	       update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	       update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
           // Titlebar needs no paint event
	       QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
	    }
    } 
}


void GalliumClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


void GalliumClient::paintEvent( QPaintEvent* )
{
    bool hicolor = QPixmap::defaultDepth() > 8;
    int fontoffset = 1;

    QPainter p(this);

    // Obtain widget bounds.
    QRect r(rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width()-1;
    int y2 = r.height()-1;
    int w  = r.width();
    int h  = r.height();

    // Draw part of the frame that is the frame color
    // ==============================================
    QColorGroup g = options->colorGroup(Options::Frame, isActive());
    p.setPen( g.background() );
    p.drawLine( x, y, x2-1, y );
    p.drawLine( x, y, x, y2-1 );
 
    // Draw line under title bar
    p.drawLine( x + 4, y + titleHeight + 4, x2 - 4, y + titleHeight + 4 );

    // Fill out the border edges
    p.drawRect( x+2, y+2, w-4, h-4 );
    p.drawRect( x+3, y+3, w-6, h-6 );

    // Draw highlights and lowlights
    p.setPen(g.light());
    p.drawLine( x+1, y+1, x2-2, y+1);
    p.drawLine( x+1, y+1, x+1, y2-2);

    p.setPen(g.dark());
    p.drawLine( x2-1, y+1, x2-1, y2-1);
    p.drawLine( x+1, y2-1, x2-1, y2-1);

    // Draw black edges
    p.setPen( Qt::black );
    p.drawLine(x2, y, x2, y2);
    p.drawLine(x, y2, x2, y2);

    // Draw the title bar.
    // ===================
    r = titlebar->geometry();
    QFontMetrics fm(options->font(true));

    // Obtain blend colours.
    QColor c1 = options->color(Options::TitleBar, isActive() );
    QColor c2 = options->color(Options::TitleBlend, isActive() );

    // Paint without a buffer if the colours are the same to
    // improve performance, and only draw gradients on hicolor displays.
    if ((c1 != c2) && hicolor) 
    {
        // KS - Add gradient caching if needed at a later stage.

        // Create a disposable pixmap buffer for the title blend
        KPixmap* titleBuffer = new KPixmap;
        titleBuffer->resize(w-8, titleHeight);

        KPixmapEffect::gradient(*titleBuffer, c1, c2,
                                KPixmapEffect::HorizontalGradient);

        QPainter p2( titleBuffer, this );

        // Since drawing the gradient is (relatively) slow, it is best 
        // to draw the title text on the pixmap.

        // Reduce the font size and weight for toolwindows.
        QFont fnt = options->font(true);
        if ( smallButtons )
        {
           fnt.setPointSize( fnt.pointSize() - 2 ); // Shrink font by 2 pt.
           fnt.setWeight( QFont::Normal ); 
           fontoffset = 0;
        }
        p2.setFont( fnt );
        p2.setPen( options->color(Options::Font, isActive() ));
        p2.drawText( r.x(), fontoffset, r.width()-3, r.height()-1,
                     AlignLeft | AlignVCenter, caption() );
        p2.end();

        p.drawPixmap( 4, 4, *titleBuffer );

        delete titleBuffer; 

    } else 
    {  
       // Assume lower ended hardware, so don't use buffers.
       // Don't draw a gradient either.
       p.fillRect( 4, 4, w-8, titleHeight, c1 );
    
       // Draw the title text.
       QFont fnt = options->font(true);
       if ( smallButtons )
       {
          fnt.setPointSize( fnt.pointSize() - 2 ); // Shrink font by 2 pt.
          fnt.setWeight( QFont::Normal ); 
          fontoffset = 0;
       }
       p.setFont( fnt );
       p.setPen(options->color(Options::Font, isActive() ));
       p.drawText(r.x()+4, r.y()+fontoffset, r.width()-3, r.height()-1,
                  AlignLeft | AlignVCenter, caption() );
   }

}


void GalliumClient::showEvent(QShowEvent *ev)
{
    calcHiddenButtons();  
    show();
    Client::showEvent(ev);
}


void GalliumClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}


void GalliumClient::maximizeChange(bool m)
{
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
}


void GalliumClient::activeChange(bool)
{

    if(!miniIcon().isNull())
    {
        button[BtnMenu]->setPixmap(miniIcon()); 
    }
    else
        button[BtnMenu]->setPixmap(kdelogo);

    for(int i=GalliumClient::BtnHelp; i < GalliumClient::BtnMenu; i++)
    {
        if(button[i])
            button[i]->reset();
    }
    // Reset the menu button ?

    repaint(false);
}


void GalliumClient::calcHiddenButtons()
{
    // order of hiding is help, maximize, minimize, close, then menu;
    int minWidth = 32 + 16*4 + (providesContextHelp() ? 16*2 : 16 );

    if(lastButtonWidth > width())   // Shrinking
    { 
        lastButtonWidth = width();
        if(width() < minWidth)
        {
            hiddenItems = true;

            for(int i = GalliumClient::BtnHelp; i <= GalliumClient::BtnMenu; i++)
            {
                if(button[i])
                {
                    if( !button[i]->isHidden() ) 
                    {
                        button[i]->hide();
                    }
                    minWidth -= button[i]->sizeHint().width();
                    if(width() >= minWidth)
                        return;
                }
            }
        }
    }
    else 
    if(hiddenItems) // Expanding
    { 
        lastButtonWidth = width();
        int totalSize = 16*3;

        for(int i = GalliumClient::BtnMenu; i >= GalliumClient::BtnHelp; i--)
        {
            if(button[i])
            {
                if(button[i]->sizeHint().width() + totalSize <= width()) 
                {
                    totalSize += button[i]->sizeHint().width();
                    button[i]->resize(button[i]->sizeHint());
                    button[i]->show();
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


void GalliumClient::menuButtonPressed()
{
    static QTime* t = 0;
    static GalliumClient* tc = 0;
    if ( !t )
        t = new QTime;
 
    if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() )
    {
        // KS - move the menu left by 3 pixels, and down 2 pixels.
        QPoint menupoint ( button[BtnMenu]->rect().bottomLeft().x()-3, 
                           button[BtnMenu]->rect().bottomLeft().y()+2 );
        workspace()->clientPopup(this)->
            popup(button[BtnMenu]->mapToGlobal( menupoint ));
    }
    else {
        closeWindow();
    }
    t->start();
    tc = this;           
}

#include "win2k.moc" 
