#include "stdclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qbitmap.h>
#include <kdrawutil.h>
#include "workspace.h"
#include "options.h"

#include "stdclient_bitmaps.h"

static QPixmap* close_pix = 0;
static QPixmap* maximize_pix = 0;
static QPixmap* minimize_pix = 0;
static QPixmap* normalize_pix = 0;
static QPixmap* pinup_pix = 0;
static QPixmap* pindown_pix = 0;
static QPixmap* menu_pix = 0;

static QPixmap* dis_close_pix = 0;
static QPixmap* dis_maximize_pix = 0;
static QPixmap* dis_minimize_pix = 0;
static QPixmap* dis_normalize_pix = 0;
static QPixmap* dis_pinup_pix = 0;
static QPixmap* dis_pindown_pix = 0;
static QPixmap* dis_menu_pix = 0;

static QPixmap* question_mark_pix = 0;

/* XPM */
static const char *question_mark[] = {
/* width height ncolors chars_per_pixel */
"16 16 2 1",
/* colors */
"  c #000000",
"X c None",
/* pixels */
"XXXXXXXXXXXXXXXX",
"XXXXX        XXX",
"XXXX  XXXXX   XX",
"XXX  XXXXXXX  XX",
"XXX  XXXXXXX  XX",
"XXXXXXXXXXX   XX",
"XXXXXXXXXX   XXX",
"XXXXXXXXX   XXXX",
"XXXXXXXX  XXXXXX",
"XXXXXXX  XXXXXXX",
"XXXXXXX  XXXXXXX",
"XXXXXXXXXXXXXXXX",
"XXXXXXXXXXXXXXXX",
"XXXXXXX  XXXXXXX",
"XXXXXXX  XXXXXXX",
"XXXXXXXXXXXXXXXX"
};


static bool pixmaps_created = FALSE;

static void create_pixmaps();
QPixmap* kwin_get_menu_pix_hack()
{
    create_pixmaps();
    return menu_pix;
}

static void create_pixmaps()
{
    if ( pixmaps_created )
        return;
    pixmaps_created = true;
    QColorGroup aGrp = options->colorGroup(Options::ButtonFg, true);
    QColorGroup iGrp = options->colorGroup(Options::ButtonFg, false);

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

    question_mark_pix = new QPixmap(question_mark );

}


StdClient::StdClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    create_pixmaps();

    QGridLayout* g = new QGridLayout( this, 0, 0, 2 );
    g->setRowStretch( 1, 10 );
    g->addWidget( windowWrapper(), 1, 1 );
    g->addItem( new QSpacerItem( 0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );

    g->addColSpacing(0, 1);
    g->addColSpacing(2, 1);
    g->addRowSpacing(2, 2);


    button[0] = new QToolButton( this );
    button[1] = new QToolButton( this );
    button[2] = new QToolButton( this );
    button[3] = new QToolButton( this );
    button[4] = new ThreeButtonButton( this );
    button[5] = new QToolButton( this );

    QHBoxLayout* hb = new QHBoxLayout;
    g->addLayout( hb, 0, 1 );
    hb->addWidget( button[0] );
    hb->addWidget( button[1] );
    hb->addWidget( button[2] );

    int fh = fontMetrics().lineSpacing();

    titlebar = new QSpacerItem(10, fh, QSizePolicy::Expanding,
			       QSizePolicy::Minimum );
    hb->addItem( titlebar );

    button[6] = 0;
    if ( providesContextHelp() ) {
	button[6] = new QToolButton( this );
	hb->addWidget( button[6] ); // help  button
	hb->addItem( new QSpacerItem( 5, 0, QSizePolicy::Fixed, QSizePolicy::Expanding ) );
	button[6]->setIconSet( *question_mark_pix );
	connect( button[6], SIGNAL( clicked() ), this, ( SLOT( contextHelp() ) ) );
    }

    hb->addWidget( button[3] );
    hb->addWidget( button[4] );
    hb->addWidget( button[5] );

    for ( int i = 0; i < 7; i++) {
	if ( !button[i] )
	    continue;
	button[i]->setBackgroundMode( PaletteBackground );
	button[i]->setMouseTracking( TRUE );
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
    button[2]->hide();

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
	button[2]->hide();
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
    t.setTop( 0 );
    QRegion r = rr.subtract( QRect( t.x()+1, 0, t.width()-2, 1 ) );
    setMask( r );

    if ( isVisibleToTLW() && !testWFlags( WNorthWestGravity )) {
	// manual clearing without the titlebar (we selected WResizeNoErase )
	QPainter p( this );
	r = rr.subtract( t );
	p.setClipRegion( r );
	p.fillRect( rect(), colorGroup().brush( QColorGroup::Background ) );
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
}


/*!\reimp
 */
void StdClient::stickyChange( bool s)
{
    button[1]->setIconSet( s?*pindown_pix:*pinup_pix );
}

void StdClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect t = titlebar->geometry();
    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    qDrawWinPanel( &p, rect(), colorGroup() );
    t.setTop( 1 );
    p.setClipRegion( t );
    t.setTop( 0 );
    p.fillRect( t, options->color(Options::TitleBar, isActive()));
    p.setPen( options->color(Options::TitleBar, isActive()).light() );
    p.drawLine(t.left(), t.top()+1,  t.right(), t.top()+1);
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

   // ### TODO transient etc.
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
  Indicates that the menu button has been clicked
 */
void StdClient::menuButtonPressed()
{
    (void ) workspace()->clientPopup( this ); //trigger the popup menu
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
