#include "stdclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qbitmap.h>
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


static bool pixmaps_created = FALSE;


/**
 * Pixmap creation routine that creates full pixmaps out of bitmaps
 * for each shade and the user defined titlebutton foreground colors. There
 * is a large amount of QBitmap constructors/copies here since loadFromData
 * with type XBM doesn't seem to work with QBitmaps, the only way I could get
 * a load from data is via the constructor :( Matthias, do you know about
 * this? <mosfet@kde.org>
 */
static void create_pixmaps()
{
    if ( pixmaps_created )
        return;
    pixmaps_created = true;

    QPainter pact, pdis;
    QBitmap bitmap;
    QColor actHigh = options->color(Options::ButtonFg, true).light(150);
    QColor actMed = options->color(Options::ButtonFg, true);
    QColor actLow = options->color(Options::ButtonFg, true).dark(120);
    QColor disHigh = options->color(Options::ButtonFg, false).light(150);
    QColor disMed = options->color(Options::ButtonFg, false);
    QColor disLow = options->color(Options::ButtonFg, false).dark(120);

    close_pix = new QPixmap(16, 16);
    dis_close_pix = new QPixmap(16, 16);
    pact.begin(close_pix); pdis.begin(dis_close_pix);
    bitmap = QBitmap(16, 16, close_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, close_dgray_bits, true);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, close_mask_bits, true);
    close_pix->setMask(bitmap); dis_close_pix->setMask(bitmap);

    minimize_pix = new QPixmap(16, 16);
    dis_minimize_pix = new QPixmap(16, 16);
    pact.begin(minimize_pix); pdis.begin(dis_minimize_pix);
    bitmap = QBitmap(16, 16, iconify_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, iconify_dgray_bits, true);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, iconify_mask_bits, true);
    minimize_pix->setMask(bitmap); dis_minimize_pix->setMask(bitmap);

    maximize_pix = new QPixmap(16, 16);
    dis_maximize_pix = new QPixmap(16, 16);
    pact.begin(maximize_pix); pdis.begin(dis_maximize_pix);
    bitmap = QBitmap(16, 16, maximize_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, maximize_dgray_bits, true);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, maximize_mask_bits, true);
    maximize_pix->setMask(bitmap); dis_maximize_pix->setMask(bitmap);

    normalize_pix = new QPixmap(16, 16);
    dis_normalize_pix = new QPixmap(16, 16);
    pact.begin(normalize_pix); pdis.begin(dis_normalize_pix);
    bitmap = QBitmap(16, 16, maximizedown_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, maximizedown_dgray_bits, true);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, maximizedown_mask_bits, true);
    normalize_pix->setMask(bitmap); dis_normalize_pix->setMask(bitmap);

    menu_pix = new QPixmap(16, 16);
    dis_menu_pix = new QPixmap(16, 16);
    pact.begin(menu_pix); pdis.begin(dis_menu_pix);
    bitmap = QBitmap(16, 16, menu_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, menu_dgray_bits, true);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, menu_mask_bits, true);
    menu_pix->setMask(bitmap); dis_menu_pix->setMask(bitmap);

    pinup_pix = new QPixmap(16, 16);
    dis_pinup_pix = new QPixmap(16, 16);
    pact.begin(pinup_pix); pdis.begin(dis_pinup_pix);
    bitmap = QBitmap(16, 16, pinup_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, pinup_gray_bits, true);
    pact.setPen(actMed); pdis.setPen(disMed);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, pinup_dgray_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, pinup_mask_bits, true);
    pinup_pix->setMask(bitmap); dis_pinup_pix->setMask(bitmap);

    pindown_pix = new QPixmap(16, 16);
    dis_pindown_pix = new QPixmap(16, 16);
    pact.begin(pindown_pix); pdis.begin(dis_pindown_pix);
    bitmap = QBitmap(16, 16, pindown_white_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actHigh); pdis.setPen(disHigh);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, pindown_gray_bits, true);
    pact.setPen(actMed); pdis.setPen(disMed);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    bitmap = QBitmap(16, 16, pindown_dgray_bits, true);
    bitmap.setMask(bitmap);
    pact.setPen(actLow); pdis.setPen(disLow);
    pact.drawPixmap(0, 0, bitmap);
    pdis.drawPixmap(0, 0, bitmap);
    pact.end(); pdis.end();
    bitmap = QBitmap(16, 16, pindown_mask_bits, true);
    pindown_pix->setMask(bitmap); dis_pindown_pix->setMask(bitmap);

}


StdClient::StdClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    create_pixmaps();

    QGridLayout* g = new QGridLayout( this, 0, 0, 2 );
    g->setRowStretch( 1, 10 );
    g->addWidget( windowWrapper(), 1, 1 );
    g->addColSpacing(0, 2);
    g->addColSpacing(2, 2);
    g->addRowSpacing(2, 2);


    button[0] = new QToolButton( this );
    button[1] = new QToolButton( this );
    button[2] = new QToolButton( this );
    button[3] = new QToolButton( this );
    button[4] = new QToolButton( this );
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

    hb->addWidget( button[3] );
    hb->addWidget( button[4] );
    hb->addWidget( button[5] );

    for ( int i = 0; i < 6; i++) {
	button[i]->setBackgroundMode( PaletteBackground );
	button[i]->setMouseTracking( TRUE );
	button[i]->setFixedSize( 20, 20 );
    }
    if(miniIcon().isNull())
        button[0]->setIconSet(isActive() ? *menu_pix : *dis_menu_pix);
    else
        button[0]->setIconSet( miniIcon() );
    button[1]->setIconSet(isSticky() ? isActive() ? *pindown_pix : *dis_pindown_pix :
                          isActive() ? *pinup_pix : *dis_pinup_pix );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( toggleSticky() ) ) );
    button[2]->hide();

    button[3]->setIconSet(isActive() ? *minimize_pix : *dis_minimize_pix);
    connect( button[3], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    button[4]->setIconSet(isActive() ? *maximize_pix : *dis_maximize_pix);
    connect( button[4], SIGNAL( clicked() ), this, ( SLOT( maximize() ) ) );
    button[5]->setIconSet(isActive() ? *close_pix : *dis_close_pix);
    connect( button[5], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );

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

    if ( isVisibleToTLW() ) {
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

void StdClient::paintEvent( QPaintEvent* e)
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
    qDrawShadePanel( &p, t.x(), t.y(), t.width(), t.height(),
                     colorGroup(), true, 1 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );
    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    p.drawText( t, AlignLeft|AlignVCenter, caption() );
}


void StdClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if ( titlebar->geometry().contains( e->pos() ) )
	setShade( !isShade() );
    workspace()->requestFocus( this );
}


void StdClient::init()
{
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

