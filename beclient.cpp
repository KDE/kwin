#include "beclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include "workspace.h"



static const char * size_xpm[] = {
/* width height num_colors chars_per_pixel */
"16 16 3 1",
/* colors */
" 	s None	c None",
".	c #707070",
"X	c white",
/* pixels */
"                ",
" .......        ",
" .XXXXXX        ",
" .X   .X        ",
" .X   .X....... ",
" .X   .XXXXXXXX ",
" .X   .X     .X ",
" .X....X     .X ",
" .XXXXXX     .X ",
"    .X       .X ",
"    .X       .X ",
"    .X       .X ",
"    .X........X ",
"    .XXXXXXXXXX ",
"                ",
"                "};

static QPixmap* size_pix = 0;
static bool pixmaps_created = FALSE;

static QColorGroup *aFrameGrp = 0;
static QColorGroup *iFrameGrp = 0;
static QColorGroup *aTitleGrp = 0;
static QColorGroup *iTitleGrp = 0;
static bool colors_created = FALSE;

static void create_pixmaps()
{
    if ( pixmaps_created )
        return;
    pixmaps_created = true;
    size_pix = new QPixmap( size_xpm );
}

static void create_colorgroups()
{
    if(colors_created)
        return;
    colors_created = true;
    aFrameGrp = BeClient::makeColorGroup(options->
                                         color(Options::Frame, true));
    iFrameGrp = BeClient::makeColorGroup(options->
                                         color(Options::Frame, false));
    aTitleGrp = BeClient::makeColorGroup(options->
                                         color(Options::TitleBar, true));
    iTitleGrp = BeClient::makeColorGroup(options->
                                         color(Options::TitleBar, false));
}

BeClient::BeClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name, WResizeNoErase  )
{
    create_pixmaps();
    create_colorgroups();
    QGridLayout* g = new QGridLayout( this, 0, 0, 2 );
    g->addRowSpacing(1, 2);
    g->setRowStretch( 2, 10 );
    g->addWidget( windowWrapper(), 2, 1 );
    g->addColSpacing(0, 2);
    g->addColSpacing(2, 2);
    g->addRowSpacing(3, 2);



    QHBoxLayout* hb = new QHBoxLayout;
    g->addLayout( hb, 0, 1 );
    int fh = QMAX( 16, fontMetrics().lineSpacing());
    titlebar = new QSpacerItem(40, fh, QSizePolicy::Preferred,
			       QSizePolicy::Minimum );
    hb->addItem( titlebar );

    hb->addStretch();
}


BeClient::~BeClient()
{
}


void BeClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );
    doShape();
    if ( isVisibleToTLW() ) {
	// manual clearing without the titlebar (we selected WResizeNoErase )
	QPainter p( this );
	QRect t = titlebar->geometry();
	t.setTop( 0 );
	t.setLeft( 0 );
	QRegion r = rect();
	r = r.subtract( t );
	p.setClipRegion( r );
	p.eraseRect( rect() );
    }
}

/*!\reimp
 */
void BeClient::captionChange( const QString& )
{
    doShape();
    repaint();
}

void BeClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect bar ( 0, 0, titlebar->geometry().right()+1,
                titlebar->geometry().bottom() );
    qDrawWinPanel( &p, 0, bar.bottom()+2, width(), height() - bar.bottom()-2,
                   isActive() ? *aFrameGrp : *iFrameGrp, FALSE );
    qDrawWinPanel( &p, 2, bar.bottom()+4, width()-4, height() - bar.bottom()-6,
                   isActive() ? *aFrameGrp : *iFrameGrp, TRUE );
    QRect t = titlebar->geometry();

    bar.setBottom( bar.bottom() + 3 );
    p.setClipRect( bar );
    bar.setBottom( bar.bottom() + 2 );
    qDrawWinPanel( &p, bar, isActive() ? *aTitleGrp : *iTitleGrp, FALSE,
                   isActive() ? &aTitleGrp->brush(QColorGroup::Background) :
                   &iTitleGrp->brush(QColorGroup::Background));
    p.setClipping( FALSE );

    p.drawPixmap( t.right() - 20, t.center().y()-8, *size_pix );
    p.drawPixmap( t.left() +4, t.center().y()-miniIcon().height()/2, miniIcon() );
    t.setLeft( t.left() + 20 +10);
    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    p.drawText( t, AlignLeft|AlignVCenter, caption() );
}


void BeClient::showEvent( QShowEvent* e)
{
    Client::showEvent( e );
    doShape();
    repaint();
}

void BeClient::doShape()
{
    QFontMetrics fm = fontMetrics();
    int cap = 20+20+10+10+fm.boundingRect(caption() ).width();
    titlebar->changeSize(  QMIN( width(), cap), QMAX( 16, fm.lineSpacing()),
	QSizePolicy::Preferred, QSizePolicy::Minimum );
    layout()->activate(); //#### this is broken!!!!! PAUL!!!!!

// //      // do it manually: #######remove this for Qt-2.01
    titlebar->setGeometry( QRect( titlebar->geometry().x(), titlebar->geometry().y(),
   				  titlebar->sizeHint().width(), titlebar->sizeHint().height() ) );
    QRegion r( rect() );
    r = r.subtract( QRect( QPoint( titlebar->geometry().right()+1, 0), QPoint( width(), titlebar->geometry().bottom()) ) );
    setMask( r );
}

/*!\reimp
 */
void BeClient::activeChange( bool /* act */ )
{
    repaint( 0, 0, width(), titlebar->geometry().bottom()+3, FALSE  );
}

/*!\reimp
 */
Client::MousePosition BeClient::mousePosition( const QPoint& p ) const
{
    const int range = 16;
    const int border = 4;

    int ly = titlebar->geometry().bottom();
    int lx = titlebar->geometry().right();
    if ( p.x() > titlebar->geometry().right() ) {

	if ( p.y() <= ly + range && p.x() >= width()-range)
	    return TopRight;
	else if ( p.y() <= ly + border )
	    return Top;
    } else if ( p.y() < ly ) {
	if ( p.y() > border && p.x() < lx - border )
	    return Client::mousePosition( p );
	if ( p.y() < range && p.x() > lx - range )
	    return TopRight;
	else if ( p.x() > lx-border )
	    return Right;
    }

    return Client::mousePosition( p );
}


void BeClient::mousePressEvent( QMouseEvent * e )
{
    
    Client::mousePressEvent( e );
}

void BeClient::mouseReleaseEvent( QMouseEvent * e )
{
    workspace()->makeFullScreen( this );
    Client::mouseReleaseEvent( e );
}


void BeClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if ( titlebar->geometry().contains( e->pos() ) )
	setShade( !isShade() );
    workspace()->requestFocus( this );
}

QColorGroup* BeClient::makeColorGroup(const QColor &bg, const QColor &fg)
{
    return(new QColorGroup( fg, bg, bg.light(150), bg.dark(),
                        bg.dark(120), fg,
                        QApplication::palette().normal().base()));
}            
