/*
  Gallium-Quartz KWin client

  Copyright 2001
    Karol Szwed <karlmail@usa.net>
    http://gallium.n3.net/

  Based upon the Win2K kwin client, which is based on the
  KDE default client.

  Includes mini titlebars for ToolWindow Support.
*/

#include "quartz.h"
#include <qlayout.h>
#include <qlabel.h>
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
        return(new QuartzClient(ws, w));
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
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00,
  0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

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
// =================================
// We don't make them dynamic, as we want this memory freed upon unloading
// this library from kwin. We get no notice upon being unloaded, so there's
// no way to free dynamic images. If they're not dynamic, the freeing of
// memory is made when the library in unloaded.

static KPixmap btnPix1; 
static KPixmap iBtnPix1;
static KPixmap btnDownPix1;
static KPixmap iBtnDownPix1;
static QPixmap defaultMenuPix( kdelogo );
static KPixmap titleBlocks;
static KPixmap ititleBlocks;
static bool    pixmaps_created = false;


// This does the colour transition magic. (You say "Oh, is that it?")
// In future, we may make this customisable, so we can have gears, blocks or
// any other eye candy via an alphapainter to perserve custom colour settings. 
static void drawBlocks( KPixmap *pi, KPixmap &p, const QColor &c1, const QColor &c2 )
{
   QPainter px;

   px.begin( pi );

   // Draw a background gradient first
   KPixmapEffect::gradient(p, c1, c2, KPixmapEffect::HorizontalGradient);

   px.fillRect( 2, 1, 3, 3, c1.light(120) );
   px.fillRect( 2, 5, 3, 3, c1 );
   px.fillRect( 2, 9, 3, 3, c1.light(110) );
   px.fillRect( 2, 13, 3, 3, c1 );

   px.fillRect( 6, 1, 3, 3, c1.light(110) );
   px.fillRect( 6, 5, 3, 3, c2.light(110) );
   px.fillRect( 6, 9, 3, 3, c1.light(120) );
   px.fillRect( 6, 13, 3, 3, c2.light(130) );

   px.fillRect( 10, 5, 3, 3, c1.light(110) );
   px.fillRect( 10, 9, 3, 3, c2.light(120) );
   px.fillRect( 10, 13, 3, 3, c2.light(150) );

   px.fillRect( 14, 1, 3, 3, c1.dark(110) );
   px.fillRect( 14, 9, 3, 3, c2.light(120) );
   px.fillRect( 14, 13, 3, 3, c1.dark(120) );

   px.fillRect( 18, 5, 3, 3, c1.light(110) );
   px.fillRect( 18, 13, 3, 3, c1.dark(110) );

   px.fillRect( 22, 9, 3, 3, c2.light(120));
   px.fillRect( 22, 13, 3, 3, c2.light(110) );
}



// This paints the button pixmaps upon loading the style.
static void create_pixmaps()
{
    if(pixmaps_created)
        return;

    pixmaps_created = true;

    // It does not matter that we don't resize thesei for mini-buttons
    btnPix1.resize(16, 16);
    btnDownPix1.resize(16, 16);
    iBtnPix1.resize(16, 16);
    iBtnDownPix1.resize(16, 16);

    // Fill the buttons with colour
    QColorGroup g = options->colorGroup(Options::TitleBlend, true );
    QColor c = g.background();
    btnPix1.fill(c.rgb());
    btnDownPix1.fill(c.rgb());

    g = options->colorGroup(Options::TitleBlend, false);
    c = g.background();
    iBtnPix1.fill(c.rgb());
    iBtnDownPix1.fill(c.rgb());

    // Obtain titlebar blend colours, and create the block stuff on pixmaps.
    QColorGroup g2 = options->colorGroup(Options::TitleBlend, true);
    QColor c2 = g2.background();
    g = options->colorGroup(Options::TitleBar, true );
    c = g.background().light(130);
    titleBlocks.resize( 25, 18 );
    drawBlocks( &titleBlocks, titleBlocks, c, c2 );

    g2 = options->colorGroup(Options::TitleBlend, false);
    c2 = g2.background();
    g = options->colorGroup(Options::TitleBar, false );
    c = g.background().light(130);
    ititleBlocks.resize( 25, 18 );
    drawBlocks( &ititleBlocks, ititleBlocks, c, c2 );
}



QuartzButton::QuartzButton(Client *parent, const char *name, bool largeButton,
                           const unsigned char *bitmap)
    : QButton(parent, name, WStyle_Customize | WRepaintNoErase |
                            WResizeNoErase | WStyle_NoBorder )
{
    // Eliminate any possible background flicker
    setBackgroundMode( QWidget::NoBackground ); 

    client = parent;

    large = largeButton;

    if ( large )
       setFixedSize(16, 16);
    else
       setFixedSize(10, 10);

    if(bitmap)
        setBitmap(bitmap);
}


QSize QuartzButton::sizeHint() const
{
   if ( large )
      return( QSize(16,16) );
   else
      return( QSize(10,10) );
}


void QuartzButton::reset()
{
    repaint(false);
}


void QuartzButton::setBitmap(const unsigned char *bitmap)
{
    pix.resize(0, 0);
    deco = QBitmap(10, 10, bitmap, true);
    deco.setMask(deco);
    repaint( false );   
}


void QuartzButton::setPixmap( const QPixmap &p )
{
    deco.resize(0, 0);
    pix = p;

    if ( large ) 
       setMask( QRect(0, 0, 16, 16) );
    else
       setMask( QRect(0, 0, 10, 10) );

    repaint( false );   
}


void QuartzButton::drawButton(QPainter *p)
{
    // Draw a button bg for bitmaps only
    if(pix.isNull())
    {
       if(client->isActive())
       {
          if(isDown())
             p->drawPixmap(0, 0, btnDownPix1);
          else
             p->drawPixmap(0, 0, btnPix1);
       } else
           {
              if(isDown())
                 p->drawPixmap(0, 0, iBtnDownPix1);
              else
                  p->drawPixmap(0, 0, iBtnPix1);
           }

        int xOff = (width()-10)/2;
        int yOff = (height()-10)/2;
        p->setPen( Qt::black );
        p->drawPixmap(isDown() ? xOff+2: xOff+1, isDown() ? yOff+2 : yOff+1, deco);
        p->setPen( options->color(Options::ButtonBg, client->isActive()).light(150) );
        p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, deco);

    }
    else
    {
        // This is the menu button, which has a pixmap.
        p->fillRect(0, 0, width(), height(),
            options->color(Options::TitleBar, client->isActive()).light(130) );
         
        // Shrink the miniIcon for tiny titlebars.
        if ( !large )
        {
           QPixmap tmpPix;

           // Smooth scale the image
           tmpPix.convertFromImage( pix.convertToImage().smoothScale(10, 10));
       
           p->drawPixmap( 0, 0, tmpPix );
        }
        else
           p->drawPixmap( 0, 0, pix );
          
    }
}


QuartzClient::QuartzClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WNorthWestGravity | WRepaintNoErase )
{
    setBackgroundMode( QWidget::NoBackground );

    // Finally, toolWindows look small
    if ( isTool() )
    {
       titleHeight  = 12; 
       largeButtons = false;
    }
    else
       {
          titleHeight = 18;    
          largeButtons = true;
       }

    lastButtonWidth = 0;

    // Create the button and titlebar pixmaps
    create_pixmaps();

    // Pack the windowWrapper() window within a grid
    QGridLayout* g = new QGridLayout(this, 0, 0, 0);
    g->setResizeMode(QLayout::FreeResize);
    g->addRowSpacing(0, 3);       // Top grab bar 
    g->addWidget(windowWrapper(), 3, 1);
    g->setRowStretch(3, 10);      // Wrapped window
    g->addRowSpacing(2, 1);       // line under titlebar
    g->addRowSpacing(4, 4);       // bottom handles
    g->addColSpacing(0, 4);
    g->addColSpacing(2, 4);

    // Create the required buttons
    button[BtnMenu]    = new QuartzButton(this, "menu", largeButtons, NULL);
    button[BtnClose]   = new QuartzButton(this, "close", largeButtons, close_bits);
    button[BtnIconify] = new QuartzButton(this, "iconify", largeButtons, iconify_bits);
    button[BtnMax]     = new QuartzButton(this, "maximize", largeButtons, maximize_bits);

    // Connect required stuff together
    connect( button[BtnMenu],    SIGNAL( pressed() ),    this, SLOT( menuButtonPressed() ));
    connect( button[BtnClose],   SIGNAL( clicked() ),    this, SLOT( closeWindow() ));
    connect( button[BtnIconify], SIGNAL( clicked() ),    this, SLOT( iconify() ));
    connect( button[BtnMax],     SIGNAL( clicked() ),    this, SLOT( slotMaximize() ));
    connect( options,            SIGNAL(resetClients()), this, SLOT( slotReset() ));

    // Pack the titlebar HBox with items
    hb = new QHBoxLayout();
    hb->setResizeMode( QLayout::FreeResize );
    g->addLayout ( hb, 1, 1 );
    hb->addWidget( button[BtnMenu] );  
    titlebar = new QSpacerItem( 10, titleHeight, QSizePolicy::Expanding,
                                QSizePolicy::Minimum );
    hb->addItem(titlebar);
    hb->addSpacing(2);

    // Add a help button if required
    if( providesContextHelp() )
    {
        button[BtnHelp] = new QuartzButton(this, "help", largeButtons, question_bits);
        connect( button[BtnHelp], SIGNAL( clicked() ), this, SLOT( contextHelp() ));
        hb->addWidget( button[BtnHelp] );
    } else
        button[BtnHelp] = NULL;

    hb->addWidget( button[BtnIconify] );
    hb->addWidget( button[BtnMax] );
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


void QuartzClient::slotReset()
{
    pixmaps_created = false;
    create_pixmaps();

    // ( 4 buttons - Help, Max, Iconify, Close )
    for(int i = QuartzClient::BtnHelp; i <= QuartzClient::BtnClose; i++)
        if(button[i])
            button[i]->reset();

    // iconChange() resets the menu button so we don't have to do this here.

    repaint( false );  
}


void QuartzClient::iconChange()
{
    if(!miniIcon().isNull())
       button[BtnMenu]->setPixmap(miniIcon());
    else
       button[BtnMenu]->setPixmap(defaultMenuPix);

    if (button[BtnMenu]->isVisible())
       button[BtnMenu]->repaint(false);
}


void QuartzClient::slotMaximize()
{
    if ( button[BtnMax]->last_button == MidButton )
	   maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
       maximize( MaximizeHorizontal );
    else
	   maximize();
}


void QuartzClient::resizeEvent( QResizeEvent* e)
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


void QuartzClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


// Quartz Paint magic goes here.
void QuartzClient::paintEvent( QPaintEvent* )
{
    QPainter p(this);

    // Obtain widget bounds.
    QRect r(rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width() - 1;
    int y2 = r.height() - 1;
    int w  = r.width();
    int h  = r.height();

    // Draw part of the frame that is the title color
    QColorGroup g = options->colorGroup(Options::TitleBar, isActive());
 
    // Draw outer highlights and lowlights
    p.setPen( g.light().light(120) );
    p.drawLine( x, y, x2-1, y );
    p.drawLine( x, y+1, x, y2-1 );
    p.setPen( g.dark().light(120) );
    p.drawLine( x2, y, x2, y2 );
    p.drawLine( x, y2, x2, y2 );

    // Fill out the border edges
    p.setPen( g.background().light(130) );
    p.drawRect( x+1, y+1, w-2, h-2 );
    p.drawRect( x+2, y+2, w-4, h-4 );

    // Draw a frame around the wrapped widget.
    p.setPen( g.background() );
    p.drawRect( x+3, y + titleHeight + 3, w-6, h-titleHeight-6 );

    // Highlight top corner
    p.setPen( g.light().light(160) );
    p.drawPoint( x, y );
    p.setPen( g.light().light(140) );
    p.drawPoint( x+1, y );
    p.drawPoint( x, y+1 );

    // Draw the title bar.
    // ===================
    r = titlebar->geometry();
    QFontMetrics fm(options->font(true));

    // Obtain titlebar blend colours
    QColor c1 = options->color(Options::TitleBar, isActive() ).light(130);
    QColor c2 = options->color(Options::TitleBlend, isActive() );

    // Create a disposable pixmap buffer for the titlebar 
    KPixmap* titleBuffer = new KPixmap;
    titleBuffer->resize( w-6, titleHeight );  

    QPainter p2( titleBuffer, this );

    p2.fillRect( 0, 0, r.width(), r.height(), c1 );
    p2.fillRect( r.width(), 0, w-r.width()-6, r.height(), c2 );

    // 8 bit displays will be a bit dithered, but they still look ok.
    if ( isActive() )
        p2.drawPixmap( r.width()-10, 0, titleBlocks );
    else
        p2.drawPixmap( r.width()-10, 0, ititleBlocks );

    // Draw the title text on the pixmap, and with a smaller font
    // for toolwindows than the default.
    QFont fnt = options->font(true);
    if ( !largeButtons )
    { 
       fnt.setPointSize( fnt.pointSize() - 2 );  // Shrink font by 2pt 
       fnt.setWeight( QFont::Normal );           // and disable bold
    }
    p2.setFont( fnt );

    p2.setPen( options->color(Options::Font, isActive() ));
    p2.drawText(r.x(), 0, r.width()-3, r.height()-1,
                AlignLeft | AlignVCenter, caption() );
    p2.end();

    p.drawPixmap( 3, 3, *titleBuffer );

    delete titleBuffer;
}


void QuartzClient::showEvent(QShowEvent *ev)
{
    calcHiddenButtons();  
    show();
    Client::showEvent(ev);
}


void QuartzClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
       workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}


void QuartzClient::maximizeChange(bool m)
{
    button[BtnMax]->setBitmap(m ? minmax_bits : maximize_bits);
}


void QuartzClient::activeChange(bool)
{

    if(!miniIcon().isNull())
        button[BtnMenu]->setPixmap(miniIcon()); 
    else
        button[BtnMenu]->setPixmap(kdelogo);

    for(int i=QuartzClient::BtnHelp; i < QuartzClient::BtnMenu; i++)
    {
        if(button[i])
            button[i]->reset();
    }

    repaint(false);
}



// The hiding button while shrinking, show button while expanding magic
void QuartzClient::calcHiddenButtons()
{
    // order of hiding is help, maximize, minimize, close, then menu;
    int minWidth = 32 + 16*4 + (providesContextHelp() ? 16*2 : 16 );

    if(lastButtonWidth > width())   // Shrinking
    { 
        lastButtonWidth = width();
        if(width() < minWidth)
        {
            hiddenItems = true;

            for(int i = QuartzClient::BtnHelp; i <= QuartzClient::BtnMenu; i++)
               if(button[i])
               {
                  if( !button[i]->isHidden() ) 
                      button[i]->hide();

                   minWidth -= button[i]->sizeHint().width();
                   if(width() >= minWidth)
                       return;
                }
        }
    } else 
       if(hiddenItems) // Expanding
       { 
          lastButtonWidth = width();
          int totalSize = 16*3;

          for(int i = QuartzClient::BtnMenu; i >= QuartzClient::BtnHelp; i--)
            if(button[i])
            {
                if(button[i]->sizeHint().width() + totalSize <= width()) 
                {
                    totalSize += button[i]->sizeHint().width();
                    button[i]->resize(button[i]->sizeHint());
                    button[i]->show();
                }
                else return;
            }

        // all items shown now
        hiddenItems = false;
      }
      else
          lastButtonWidth = width();
}


// Make sure the menu button follows double click conventions set in kcontrol
void QuartzClient::menuButtonPressed()
{
    static QTime* t = 0;
    static QuartzClient* tc = 0;
    if ( !t )
        t = new QTime;
 
    if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() )
    {
        QPoint menupoint ( button[BtnMenu]->rect().bottomLeft().x()-1, 
                           button[BtnMenu]->rect().bottomLeft().y()+2 );
        workspace()->clientPopup(this)->popup(button[BtnMenu]->mapToGlobal( menupoint ));
    }
    else closeWindow();

    t->start();
    tc = this;           
}

#include "quartz.moc"
