#include "mwmclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qdatetime.h>
#include <kpixmapeffect.h>
#include "../../workspace.h"
#include "../../options.h"

#include <kdebug.h>

using namespace KWinInternal;

const int s_frameWidth = 5;
const int s_buttonSize = 18;


MwmButton::MwmButton( MwmClient* parent, const char* name, int btnType )
    : QButton( parent, name , WStyle_Customize | WStyle_NoBorder | WRepaintNoErase
	| WResizeNoErase ), m_parent(parent), m_btnType(btnType)
{
    setBackgroundMode( QWidget::NoBackground );

    setFixedSize( s_buttonSize, s_buttonSize );
    resize( s_buttonSize, s_buttonSize );
}

void MwmButton::reset()
{
    repaint( false );
}

void MwmButton::drawButton( QPainter* p )
{
    p->setBrush( options->color( Options::TitleBar, m_parent->isActive() ) );
    p->drawRect( 0, 0, s_buttonSize, s_buttonSize );

    qDrawShadePanel( p, 0, 0, s_buttonSize, s_buttonSize,
	options->colorGroup( Options::TitleBar, m_parent->isActive() ), isDown() );

    switch ( m_btnType )
    {
	case (BtnMenu):
	    qDrawShadePanel( p, 4, 6, 10, 4, options->colorGroup( Options::TitleBar, m_parent->isActive() ) );
	break;
	case (BtnIconify):
	    qDrawShadePanel( p, 6, 7, 5, 5, options->colorGroup( Options::TitleBar, m_parent->isActive() ) );
	break;
	case (BtnMax):
	    qDrawShadePanel( p, 4, 4, 10, 10, options->colorGroup( Options::TitleBar, m_parent->isActive() ) );
	break;
    }
}


MwmClient::MwmClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WNorthWestGravity | WResizeNoErase | WRepaintNoErase )
{
    setBackgroundMode( QWidget::NoBackground );

    QVBoxLayout *mainLayout = new QVBoxLayout( this );
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QHBoxLayout *windowLayout = new QHBoxLayout();

    mainLayout->addSpacing( s_frameWidth+1 );
    mainLayout->addLayout( titleLayout );
    mainLayout->addLayout( windowLayout, 1 );
    mainLayout->addSpacing( s_frameWidth+1 );

    windowLayout->addSpacing( s_frameWidth+1 );
    windowLayout->addWidget( windowWrapper(), 1 );
    windowLayout->addSpacing( s_frameWidth+1 );

    button[BtnMenu]    = new MwmButton( this, "menu", BtnMenu );
    button[BtnIconify] = new MwmButton( this, "iconify", BtnIconify );
    button[BtnMax]     = new MwmButton( this, "maximize", BtnMax );

    connect( button[BtnMenu],    SIGNAL( pressed() ),      this, ( SLOT( menuButtonPressed() ) ) );
    connect( button[BtnIconify], SIGNAL( clicked() ),      this, ( SLOT( iconify() ) ) );
    connect( button[BtnMax],     SIGNAL( clicked() ),      this, ( SLOT( slotMaximize() ) ) );
    connect( options,            SIGNAL( resetClients() ), this, ( SLOT( slotReset() ) ) );

    titleLayout->addSpacing( s_frameWidth+1 );
    titleLayout->addWidget( button[BtnMenu] );
    titlebar = new QSpacerItem( 10, 16, QSizePolicy::Expanding,
                               QSizePolicy::Minimum );
    titleLayout->addItem( titlebar );
    titleLayout->addWidget( button[BtnIconify] );
    titleLayout->addWidget( button[BtnMax] );
    titleLayout->addSpacing( s_frameWidth+1 );
}

void MwmClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}

void MwmClient::activeChange( bool )
{
    button[BtnMenu]->reset();
    button[BtnIconify]->reset();
    button[BtnMax]->reset();

    repaint( false );
}

void MwmClient::slotReset()
{
    button[BtnMenu]->reset();
    button[BtnIconify]->reset();
    button[BtnMax]->reset();

    repaint( false );
}

void MwmClient::slotMaximize()
{
/*    if ( button[BtnMax]->last_button == MidButton )
	maximize( MaximizeVertical );
    else if ( button[BtnMax]->last_button == RightButton )
	maximize( MaximizeHorizontal );
    else
*/	maximize();
}

void MwmClient::menuButtonPressed()
{
    workspace()->clientPopup(this)->
    	popup(button[BtnMenu]->mapToGlobal(button[BtnMenu]->rect().bottomLeft()));
}

void MwmClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

    if ( isVisibleToTLW() ) 
    {
        update( rect() );
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
	       QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), false ) );
	    }
    } 
}

void MwmClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );

    QRect trect = titlebar->geometry();
    QRect wrect = windowWrapper()->geometry();
    QRect mrect = this->geometry();
    
    // draw black frame:
    p.setPen( Qt::black );
    p.drawRect( mrect );
    p.setPen( Qt::NoPen );

    p.setBrush( options->color( Options::TitleBar, isActive() ) );

    // draw frame-background:
    p.drawRect( 1, 1, mrect.width()-2, s_frameWidth );
    p.drawRect( 1, mrect.height()-s_frameWidth-1, mrect.width()-2, s_frameWidth );
    p.drawRect( 1, s_frameWidth+1, s_frameWidth, mrect.height()-2*s_frameWidth-2 );
    p.drawRect( mrect.width()-s_frameWidth-1, s_frameWidth+1, s_frameWidth, mrect.height()-2*s_frameWidth-2 );
    
    // draw left border:
    qDrawShadePanel( &p, 1, wrect.y(), s_frameWidth, wrect.height()-s_buttonSize,
	options->colorGroup( Options::TitleBar, isActive() ) );

    // draw right border:
    qDrawShadePanel( &p, mrect.width()-s_frameWidth-1, wrect.y(), s_frameWidth, wrect.height()-s_buttonSize,
	options->colorGroup( Options::TitleBar, isActive() ) );

    // draw top border:
    qDrawShadePanel( &p, s_frameWidth+s_buttonSize+1, 1, wrect.width()-2*s_buttonSize, s_frameWidth,
	options->colorGroup( Options::TitleBar, isActive() ) );

    // draw bottom border:
    qDrawShadePanel( &p, s_frameWidth+s_buttonSize+1, mrect.height()-s_frameWidth-1, wrect.width()-2*s_buttonSize, s_frameWidth,
	options->colorGroup( Options::TitleBar, isActive() ) );

    // draw light corner parts:
    p.setPen( options->colorGroup( Options::TitleBar, isActive() ).light() );
    // tl corner:
    p.drawLine( 1, 1, s_frameWidth+s_buttonSize-1, 1 );
    p.drawLine( 1, 1, 1, s_frameWidth+s_buttonSize-1 );
    // tr corner:
    p.drawLine( mrect.width()-3, 1, mrect.width()-s_frameWidth-s_buttonSize-1, 1 );
    p.drawLine( mrect.width()-s_frameWidth-s_buttonSize-1, 1, mrect.width()-s_frameWidth-s_buttonSize-1, s_frameWidth-1 );
    p.drawLine( mrect.width()-s_frameWidth-1, s_frameWidth, mrect.width()-s_frameWidth-1, s_frameWidth+s_buttonSize-1 );
    // br corner:
    p.drawLine( mrect.width()-3, mrect.height()-s_frameWidth-s_buttonSize-1, mrect.width()-s_frameWidth-1, mrect.height()-s_frameWidth-s_buttonSize-1 );
    p.drawLine( mrect.width()-s_frameWidth-1, mrect.height()-s_frameWidth-s_buttonSize, mrect.width()-s_frameWidth-1, mrect.height()-s_frameWidth-1 );
    p.drawLine( mrect.width()-s_frameWidth-2, mrect.height()-s_frameWidth-1, mrect.width()-s_frameWidth-s_buttonSize-1, mrect.height()-s_frameWidth-1 );
    p.drawLine( mrect.width()-s_frameWidth-s_buttonSize-1, mrect.height()-s_frameWidth, mrect.width()-s_frameWidth-s_buttonSize-1, mrect.height()-2 );
    // bl corner:
    p.drawLine( s_frameWidth-1, mrect.height()-s_frameWidth-s_buttonSize-1, 1, mrect.height()-s_frameWidth-s_buttonSize-1 );
    p.drawLine( 1, mrect.height()-s_frameWidth-s_buttonSize-2, 1, mrect.height()-3 );
    p.drawLine( s_frameWidth+s_buttonSize-1, mrect.height()-s_frameWidth-1, s_frameWidth+1, mrect.height()-s_frameWidth-1 );

    p.setPen( Qt::NoPen );

    // draw dark corner parts:
    p.setPen( options->colorGroup( Options::TitleBar, isActive() ).dark() );
    // tl corner:
    p.drawLine( 1, s_frameWidth+s_buttonSize, s_frameWidth, s_frameWidth+s_buttonSize );
    p.drawLine( s_frameWidth, s_frameWidth+s_buttonSize-1, s_frameWidth, s_frameWidth );
    p.drawLine( s_frameWidth+1, s_frameWidth, s_frameWidth+s_buttonSize, s_frameWidth );
    p.drawLine( s_frameWidth+s_buttonSize, s_frameWidth, s_frameWidth+s_buttonSize, 1 );
    // tr corner:
    p.drawLine( mrect.width()-s_frameWidth-s_buttonSize-1, s_frameWidth, mrect.width()-s_frameWidth-2, s_frameWidth );
    p.drawLine( mrect.width()-s_frameWidth-1, s_frameWidth+s_buttonSize, mrect.width()-2, s_frameWidth+s_buttonSize );
    p.drawLine( mrect.width()-2, s_frameWidth+s_buttonSize, mrect.width()-2, 1 );
    // br corner:
    p.drawLine( mrect.width()-s_frameWidth-s_buttonSize-1, mrect.height()-2, mrect.width()-3, mrect.height()-2 );
    p.drawLine( mrect.width()-2, mrect.height()-2, mrect.width()-2, mrect.height()-s_frameWidth-s_buttonSize-2 );
    // bl corner:
    p.drawLine( 1, mrect.height()-2, s_frameWidth+s_buttonSize, mrect.height()-2 );
    p.drawLine( s_frameWidth+s_buttonSize, mrect.height()-3, s_frameWidth+s_buttonSize, mrect.height()-s_frameWidth-1 );
    p.drawLine( s_frameWidth, mrect.height()-s_frameWidth-1, s_frameWidth, mrect.height()-s_frameWidth-s_buttonSize-1 );

    p.setPen( Qt::NoPen );

    // draw titlebar:
    p.drawRect( trect );
    qDrawShadePanel( &p, trect,
	options->colorGroup( Options::TitleBar, isActive() ) );

    // draw caption:	
    p.setFont( options->font(true) );
    p.setPen( options->color( Options::Font, isActive() ) );
    p.drawText( trect.x()+3, trect.y()-1, trect.width()-6, trect.height(),
	AlignHCenter | AlignVCenter, caption() );
}

void MwmClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if ( titlebar->geometry().contains( e->pos() ) )
        setShade( !isShade() );
    workspace()->requestFocus( this );
}

void MwmClient::init()
{
    Client::init();
}

extern "C"
{
    Client *allocate(Workspace *ws, WId w)
    {
        return(new MwmClient(ws, w));
    }
    void init()
    {
    }
    void reset()
    {
    }
    void deinit()
    {
    }
}

#include "mwmclient.moc"
