/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#include "kde1client.h"
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qbitmap.h>
#include <kdrawutil.h>
#include <klocale.h>
#include <qdatetime.h>
#include <qimage.h>
#include "../../workspace.h"
#include "../../options.h"

#include "kde1client_bitmaps.h"

using namespace KWinInternal;



static QPixmap* close_pix = 0;
static QPixmap* maximize_pix = 0;
static QPixmap* minimize_pix = 0;
static QPixmap* normalize_pix = 0;
static QPixmap* pinup_pix = 0;
static QPixmap* pindown_pix = 0;
static QPixmap* menu_pix = 0;
static QPixmap* question_mark_pix = 0;

static QPixmap* dis_close_pix = 0;
static QPixmap* dis_maximize_pix = 0;
static QPixmap* dis_minimize_pix = 0;
static QPixmap* dis_normalize_pix = 0;
static QPixmap* dis_pinup_pix = 0;
static QPixmap* dis_pindown_pix = 0;
static QPixmap* dis_menu_pix = 0;
static QPixmap* dis_question_mark_pix = 0;


static bool pixmaps_created = FALSE;

static void create_pixmaps()
{
    if ( pixmaps_created )
        return;
    pixmaps_created = true;
    QColorGroup aGrp = options->colorGroup(Options::ButtonBg, true);
    QColorGroup iGrp = options->colorGroup(Options::ButtonBg, false);

    QPainter aPainter, iPainter;
    close_pix = new QPixmap(16, 16);
    dis_close_pix = new QPixmap(16, 16);
    aPainter.begin(close_pix); iPainter.begin(dis_close_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, close_white_bits,
                  NULL, NULL, close_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, close_white_bits,
                  NULL, NULL, close_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    close_pix->setMask(QBitmap(16, 16, close_mask_bits, true));
    dis_close_pix->setMask(*close_pix->mask());

    minimize_pix = new QPixmap(16, 16);
    dis_minimize_pix = new QPixmap(16, 16);
    aPainter.begin(minimize_pix); iPainter.begin(dis_minimize_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, iconify_white_bits,
                  NULL, NULL, iconify_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, iconify_white_bits,
                  NULL, NULL, iconify_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    minimize_pix->setMask(QBitmap(16, 16, iconify_mask_bits, true));
    dis_minimize_pix->setMask(*minimize_pix->mask());

    maximize_pix = new QPixmap(16, 16);
    dis_maximize_pix = new QPixmap(16, 16);
    aPainter.begin(maximize_pix); iPainter.begin(dis_maximize_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, maximize_white_bits,
                  NULL, NULL, maximize_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, maximize_white_bits,
                  NULL, NULL, maximize_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    maximize_pix->setMask(QBitmap(16, 16, maximize_mask_bits, true));
    dis_maximize_pix->setMask(*maximize_pix->mask());

    normalize_pix = new QPixmap(16, 16);
    dis_normalize_pix = new QPixmap(16, 16);
    aPainter.begin(normalize_pix); iPainter.begin(dis_normalize_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, maximizedown_white_bits,
                  NULL, NULL, maximizedown_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, maximizedown_white_bits,
                  NULL, NULL, maximizedown_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    normalize_pix->setMask(QBitmap(16, 16, maximizedown_mask_bits, true));
    dis_normalize_pix->setMask(*normalize_pix->mask());

    menu_pix = new QPixmap(16, 16);
    dis_menu_pix = new QPixmap(16, 16);
    aPainter.begin(menu_pix); iPainter.begin(dis_menu_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, menu_white_bits,
                  NULL, NULL, menu_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, menu_white_bits,
                  NULL, NULL, menu_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    menu_pix->setMask(QBitmap(16, 16, menu_mask_bits, true));
    dis_menu_pix->setMask(*menu_pix->mask());

    pinup_pix = new QPixmap(16, 16);
    dis_pinup_pix = new QPixmap(16, 16);
    aPainter.begin(pinup_pix); iPainter.begin(dis_pinup_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, pinup_white_bits,
                  pinup_gray_bits, NULL, pinup_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, pinup_white_bits,
                  pinup_gray_bits, NULL, pinup_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    pinup_pix->setMask(QBitmap(16, 16, pinup_mask_bits, true));
    dis_pinup_pix->setMask(*pinup_pix->mask());

    pindown_pix = new QPixmap(16, 16);
    dis_pindown_pix = new QPixmap(16, 16);
    aPainter.begin(pindown_pix); iPainter.begin(dis_pindown_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, pindown_white_bits,
                  pindown_gray_bits, NULL, pindown_dgray_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, pindown_white_bits,
                  pindown_gray_bits, NULL, pindown_dgray_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    pindown_pix->setMask(QBitmap(16, 16, pindown_mask_bits, true));
    dis_pindown_pix->setMask(*pindown_pix->mask());

    question_mark_pix = new QPixmap(16, 16);
    dis_question_mark_pix = new QPixmap(16, 16);
    aPainter.begin(question_mark_pix); iPainter.begin(dis_question_mark_pix);
    kColorBitmaps(&aPainter, aGrp, 0, 0, 16, 16, true, help_light_bits,
                  NULL, NULL, help_dark_bits, NULL, NULL);
    kColorBitmaps(&iPainter, iGrp, 0, 0, 16, 16, true, help_light_bits,
                  NULL, NULL, help_dark_bits, NULL, NULL);
    aPainter.end(); iPainter.end();
    question_mark_pix->setMask(QBitmap(16, 16, help_mask_bits, true));
    dis_question_mark_pix->setMask(*question_mark_pix->mask());
}

static void delete_pixmaps()
{
    delete close_pix;
    delete maximize_pix;
    delete minimize_pix;
    delete normalize_pix;
    delete pinup_pix;
    delete pindown_pix;
    delete menu_pix;
    delete question_mark_pix;
    delete dis_close_pix;
    delete dis_maximize_pix;
    delete dis_minimize_pix;
    delete dis_normalize_pix;
    delete dis_pinup_pix;
    delete dis_pindown_pix;
    delete dis_menu_pix;
    delete dis_question_mark_pix;
    pixmaps_created = false;
}

static void drawGradient
(
  QPainter & p,
  const QRect & t,
  const QColor & c1,
  const QColor & c2
)
{
  // Don't draw a million vertical lines if we don't need to.

  if (c1 == c2  || QPixmap::defaultDepth() <= 8)
  {
    p.fillRect(t, c1);
    return;
  }

  // rikkus: Adapted from KPixmapEffect::gradient().

  int rca = c1.red();
  int gca = c1.green();
  int bca = c1.blue();

  int rDiff = c2.red()   - rca;
  int gDiff = c2.green() - gca;
  int bDiff = c2.blue()  - bca;

  int rl = rca << 16;
  int gl = gca << 16;
  int bl = bca << 16;

  int rcdelta = ((1 << 16) / t.width()) * rDiff;
  int gcdelta = ((1 << 16) / t.width()) * gDiff;
  int bcdelta = ((1 << 16) / t.width()) * bDiff;

  for (int x = 0; x < t.width(); x++)
  {
    rl += rcdelta;
    gl += gcdelta;
    bl += bcdelta;

    p.setPen(QColor(rl >> 16, gl >> 16, bl >> 16));

    p.drawLine(t.x() + x, 0, t.x() + x, t.y() + t.height() - 1);
  }
}

void StdClient::slotReset()
{
    if(miniIcon().isNull())
        button[0]->setIconSet(isActive() ? *menu_pix : *dis_menu_pix);
    button[1]->setIconSet(isSticky() ? isActive() ? *pindown_pix : *dis_pindown_pix :
                          isActive() ? *pinup_pix : *dis_pinup_pix );
    button[3]->setIconSet(isActive() ? *minimize_pix : *dis_minimize_pix);
    button[4]->setIconSet(isActive() ? *maximize_pix : *dis_maximize_pix);
    button[5]->setIconSet(isActive() ? *close_pix : *dis_close_pix);
    if (button[6])
        button[6]->setIconSet(isActive() ? *question_mark_pix : *dis_question_mark_pix);

    setFont(options->font(true));
}


StdClient::StdClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    setFont(options->font(isActive() ));
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

    QGridLayout* g = new QGridLayout( this, 0, 0, 3, 2 );
    g->setRowStretch( 1, 10 );
    g->addWidget( windowWrapper(), 1, 1 );
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, 1);
    g->addColSpacing(2, 1);
    g->addRowSpacing(2, 1);


    button[0] = new KWinToolButton( this, 0, i18n("Menu") );
    button[1] = new KWinToolButton( this, 0, i18n("Sticky")  );
//    button[2] = new KWinToolButton( this, 0, i18n("?") );
    button[2] = NULL;
    button[3] = new KWinToolButton( this, 0, i18n("Minimize") );
    button[4] = new ThreeButtonButton( this, 0, i18n("Maximize") );
    button[5] = new KWinToolButton( this, 0, i18n("Close") );

    QHBoxLayout* hb = new QHBoxLayout;
    g->addLayout( hb, 0, 1 );
    hb->addWidget( button[0] );
    hb->addWidget( button[1] );
//    hb->addWidget( button[2] );

    int fh = fontMetrics().lineSpacing();

    titlebar = new QSpacerItem(10, fh, QSizePolicy::Expanding,
			       QSizePolicy::Minimum );
    hb->addItem( titlebar );

    button[6] = 0;
    if ( providesContextHelp() ) {
	button[6] = new KWinToolButton( this, 0, i18n("Help") );
	hb->addWidget( button[6] ); // help  button
	hb->addItem( new QSpacerItem( 5, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );
	button[6]->setIconSet( isActive() ? *question_mark_pix : *dis_question_mark_pix);
	connect( button[6], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }

    hb->addWidget( button[3] );
    hb->addWidget( button[4] );
    hb->addWidget( button[5] );

    for ( int i = 0; i < 7; i++) {
	if ( !button[i] )
	    continue;
	button[i]->setBackgroundMode( PaletteBackground );
	button[i]->setMouseTracking( FALSE );
	button[i]->setAutoRaise( TRUE );
	button[i]->setFocusPolicy( NoFocus );
	button[i]->setFixedSize( 20, 20 );
    }
    if(miniIcon().isNull())
        button[0]->setIconSet(isActive() ? *menu_pix : *dis_menu_pix);
    else
        button[0]->setIconSet( miniIcon() );

    connect( button[0], SIGNAL( pressed() ), this, SLOT( menuButtonPressed() ) );
    button[0]->setPopupDelay( 0 );
    button[0]->setPopup( workspace()->clientPopup( this ) );

    button[1]->setIconSet(isSticky() ? isActive() ? *pindown_pix : *dis_pindown_pix :
                          isActive() ? *pinup_pix : *dis_pinup_pix );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
//    button[2]->hide();

    button[3]->setIconSet(isActive() ? *minimize_pix : *dis_minimize_pix);
    connect( button[3], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    button[4]->setIconSet(isActive() ? *maximize_pix : *dis_maximize_pix);
    connect( button[4], SIGNAL( clicked(int) ), this, ( SLOT( maxButtonClicked(int) ) ) );
    button[5]->setIconSet(isActive() ? *close_pix : *dis_close_pix);
    connect( button[5], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );

    if ( button[6] ) {
    }


    if ( isTransient() ) {
	// lighter decoration for transient windows
	button[1]->hide();
//	button[2]->hide();
	button[3]->hide();
	button[4]->hide();
    }
}

void StdClient::activeChange(bool on)
{
    if(miniIcon().isNull())
        button[0]->setIconSet(on ? *menu_pix : *dis_menu_pix);
    button[1]->setIconSet(isSticky() ? on ? *pindown_pix : *dis_pindown_pix :
                          on ? *pinup_pix : *dis_pinup_pix );
    button[3]->setIconSet(on ? *minimize_pix : *dis_minimize_pix);
    button[4]->setIconSet(on ? *maximize_pix : *dis_maximize_pix);
    button[5]->setIconSet(on ? *close_pix : *dis_close_pix);
    if (button[6])
	button[6]->setIconSet(on ? *question_mark_pix : *dis_question_mark_pix);
    Client::activeChange(on);
}


StdClient::~StdClient()
{
}


void StdClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
    QRegion rr = rect();
    QRect t = titlebar->geometry();
//     t.setTop( 0 );
//     QRegion r = rr.subtract( QRect( t.x()+1, 0, t.width()-2, 1 ) );
//     setMask( r );

    if ( isVisibleToTLW() && !testWFlags( WNorthWestGravity )) {
	// manual clearing without the titlebar (we selected WResizeNoErase )
	QRect cr( 2, 2, width()-4, height()- 4 );
	erase( QRegion( cr ).subtract( t ) );
    }
}

/*!\reimp
 */
void StdClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), FALSE );
}


/*!\reimp
 */
void StdClient::maximizeChange( bool m )
{
    button[4]->setIconSet( m?*normalize_pix:*maximize_pix  );
    button[4]->setTipText( m ? i18n("Restore") : i18n("Maximize") );
}


/*!\reimp
 */
void StdClient::stickyChange( bool s)
{
    button[1]->setIconSet( s?*pindown_pix:*pinup_pix );
    button[1]->setTipText( s ? i18n("Un-Sticky") : i18n("Sticky") );
}

void StdClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect t = titlebar->geometry();
    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    qDrawWinPanel( &p, rect(), colorGroup() );
//     t.setTop( 1 );
    p.setClipRegion( t );
//     t.setTop( 0 );
    drawGradient(p, t, options->color(Options::TitleBar, isActive()), options->color(Options::TitleBlend, isActive()));
//     p.setPen( options->color(Options::TitleBar, isActive()).light() );
//     p.drawLine(t.left(), t.top()+1,  t.right(), t.top()+1);
    if ( isActive() )
	qDrawShadePanel( &p, t.x(), t.y(), t.width(), t.height(),
			 colorGroup(), true, 1 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );
    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    p.drawText( t, AlignLeft|AlignVCenter|SingleLine, caption() );
}


void StdClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if ( titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}


void StdClient::init()
{
    Client::init();
    button[0]->setIconSet( miniIcon() );
 }

void StdClient::iconChange()
{
    if(miniIcon().isNull())
        button[0]->setIconSet(isActive() ? *menu_pix : *dis_menu_pix);
    else
        button[0]->setIconSet( miniIcon() );
    button[0]->repaint( FALSE );
}


/*!
  Indicates that the menu button has been clicked. One press shows
  the window operation menu, a double click closes the window.
 */
void StdClient::menuButtonPressed()
{
    static QTime* t = 0;
    static StdClient* tc = 0;
    if ( !t )
	t = new QTime;

    if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() )
	button[0]->setPopup( workspace()->clientPopup( this ) );
    else {
	button[0]->setPopup( 0 );
	closeWindow();
    }
    t->start();
    tc = this;
}


void StdClient::maxButtonClicked( int button )
{
    switch ( button  ){
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


StdToolClient::StdToolClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name )
{
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
    setFont(options->font(isActive(), true ));

    QGridLayout* g = new QGridLayout( this, 0, 0, 2 );
    g->setRowStretch( 1, 10 );
    g->addWidget( windowWrapper(), 1, 1 );
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, 1);
    g->addColSpacing(2, 1);
    g->addRowSpacing(2, 1);

    closeBtn = new KWinToolButton( this, 0, i18n("Close") );
    connect( closeBtn, SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    closeBtn->setFixedSize( 13, 13);
    slotReset();

    QHBoxLayout* hb = new QHBoxLayout;
    g->addLayout( hb, 0, 1 );

    int fh = fontMetrics().lineSpacing()+2;

    titlebar = new QSpacerItem(10, fh, QSizePolicy::Expanding,
			       QSizePolicy::Minimum );
    hb->addItem( titlebar );
    hb->addWidget( closeBtn );
}

StdToolClient::~StdToolClient()
{
}

void StdToolClient::resizeEvent( QResizeEvent* e )
{
    Client::resizeEvent( e );
//     QRegion r = rect();
//     QRect t = titlebar->geometry();
//     t.setTop( 0 );
//     r = r.subtract( QRect(0, 0, width(), 1) );
//     r = r.subtract (QRect( 0, 0, 1, t.height() ) );
//     r = r.subtract (QRect( width()-1, 0, 1, t.height() ) );
//     setMask( r );
}

void StdToolClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect t = titlebar->geometry();
    QRect r = rect();
    qDrawWinPanel( &p, r, colorGroup() );
    r.setTop( t.bottom()+1 );
    qDrawWinPanel( &p, r, colorGroup() );
    p.fillRect( QRect( QPoint(t.topLeft() ), QPoint( width() - t.left(), t.bottom() ) ),
		options->color(Options::TitleBar, isActive()));
    p.setPen( options->color(Options::TitleBar, isActive()).light() );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );
    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive(), true));
    p.drawText( t, AlignLeft|AlignVCenter|SingleLine, caption() );
}


void StdToolClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if ( titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
    workspace()->requestFocus( this );
}

void StdToolClient::init()
{
}

void StdToolClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), FALSE );
}

void StdToolClient::activeChange( bool on )
{
    Client::activeChange(on);
}


void StdToolClient::slotReset()
{
    QImage img = close_pix->convertToImage();
    img = img.smoothScale( 12, 12 );
    QPixmap pm;
    pm.convertFromImage( img );
    closeBtn->setPixmap( pm );
    setFont(options->font(isActive(), true ));
}

extern "C"
{
  Client * allocate(Workspace * workSpace, WId winId, int tool )
  {
      if ( tool )
	  return new StdToolClient( workSpace, winId );
      return new StdClient(workSpace, winId);
  }
  void init()
  {
     create_pixmaps();
  }
  void reset()
  {
     delete_pixmaps();
     create_pixmaps();
     // Ensure change in tooltip state gets applied
     Workspace::self()->slotResetAllClientsDelayed();
  }
  void deinit()
  {
     delete_pixmaps();
  }
}


#include "kde1client.moc"
