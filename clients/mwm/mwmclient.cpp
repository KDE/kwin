#include <qdatetime.h>
#include <qlayout.h>
#include <qbutton.h>
#include <qdrawutil.h>
#include <klocale.h>
#include "../../workspace.h"
#include "../../options.h"

#include "mwmclient.h"

using namespace KWinInternal;

static const int s_frameWidth = 5;
static const int s_buttonSize = 19;

static void fixColorGroup(QColorGroup & colorGroup)
{
    QColor light = colorGroup.light();

    int hue, saturation, value;

    light.hsv(&hue, &saturation, &value);

    if (value < 128)
    {
      light.setHsv(hue, saturation, 128);
      colorGroup.setColor(QColorGroup::Light, light);
    }

    QColor dark = colorGroup.light();

    dark.hsv(&hue, &saturation, &value);

    if (value < 84)
    {
      dark.setHsv(hue, saturation, 84);
      colorGroup.setColor(QColorGroup::Dark, dark);
    }
}

MwmButton::MwmButton( MwmClient* parent, const char* name, int btnType, const QString& tip )
    : KWinButton( parent, name, tip ), m_btnType(btnType)
{
    setBackgroundMode( QWidget::NoBackground );
    setFixedSize( s_buttonSize, s_buttonSize );
    resize( s_buttonSize, s_buttonSize );
    m_parent = parent;
}

void MwmButton::reset()
{
    repaint( false );
}

void MwmButton::drawButton( QPainter* p )
{
    p->setBrush( options->color( Options::TitleBar, m_parent->isActive() ) );
    p->drawRect( 0, 0, s_buttonSize, s_buttonSize );

    QColorGroup colorGroup =
      options->colorGroup( Options::TitleBar, m_parent->isActive() );

    fixColorGroup(colorGroup);

    qDrawShadePanel( p, 0, 0, s_buttonSize, s_buttonSize,
	colorGroup, isDown() );

    switch ( m_btnType )
    {
	case (BtnMenu):
	    qDrawShadePanel( p, 4, 7, 11, 4, colorGroup );
	break;
	case (BtnIconify):
	    qDrawShadePanel( p, 7, 7, 5, 5, colorGroup );
	break;
	case (BtnMax):
	    qDrawShadePanel( p, 4, 4, 11, 11, colorGroup );
	break;
    }
}


MwmClient::MwmClient( Workspace* ws, WId w, QWidget* parent, const char* name )
    : Client( ws, w, parent, name, WNorthWestGravity | WResizeNoErase
	| WRepaintNoErase )
{
    setBackgroundMode( QWidget::NoBackground );

    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QHBoxLayout* windowLayout = new QHBoxLayout();

    mainLayout->addSpacing( s_frameWidth+1 );
    mainLayout->addLayout( titleLayout );
    mainLayout->addLayout( windowLayout, 1 );
    mainLayout->addSpacing( s_frameWidth+1 );

    windowLayout->addSpacing( s_frameWidth+1 );
    windowLayout->addWidget( windowWrapper(), 1 );
    windowLayout->addSpacing( s_frameWidth+1 );

    button[BtnMenu]    = new MwmButton( this, "menu", BtnMenu, i18n("Menu") );
    button[BtnIconify] = new MwmButton( this, "iconify", BtnIconify, i18n("Minimize") );
    button[BtnMax]     = new MwmButton( this, "maximize", BtnMax, i18n("Maximize") );

    connect( button[BtnMenu], SIGNAL( pressed() ),
	this, ( SLOT( menuButtonPressed() ) ) );
    connect( button[BtnIconify], SIGNAL( clicked() ),
	this, ( SLOT( iconify() ) ) );
    connect( button[BtnMax], SIGNAL( clicked() ),
	this, ( SLOT( slotMaximize() ) ) );
    connect( options, SIGNAL( resetClients() ),
	this, ( SLOT( slotReset() ) ) );

    titleLayout->addSpacing( s_frameWidth+1 );
    titleLayout->addWidget( button[BtnMenu] );

    titlebar = new QSpacerItem( 10, 16, QSizePolicy::Expanding,
	QSizePolicy::Minimum );

    titleLayout->addItem( titlebar );
    titleLayout->addWidget( button[BtnIconify] );
    titleLayout->addWidget( button[BtnMax] );
    titleLayout->addSpacing( s_frameWidth + 1 );
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
    maximize();
}

void MwmClient::maximizeChange(bool m)
{
    button[BtnMax]->setTipText(m ? i18n("Restore") : i18n("Maximize"));
}

void MwmClient::menuButtonPressed()
{
    static QTime* t = 0;
    static MwmClient* tc = 0;
    if ( !t )
	t = new QTime;

    if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() )
    {
	workspace()->clientPopup( this )->popup(
	    button[BtnMenu]->mapToGlobal(
		button[BtnMenu]->rect().bottomLeft() ) );
	button[BtnMenu]->animateClick();
    }
    else
	closeWindow();

    t->start();
    tc = this;
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
	    update( QRect( QPoint(4,4),
		titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
	    update( QRect( titlebar->geometry().topRight(),
	        QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
    	    // Titlebar needs no paint event
	    QApplication::postEvent( this,
	           new QPaintEvent( titlebar->geometry(), false ) );
	}
    } 
}

void MwmClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );

    QColorGroup colorGroup =
      options->colorGroup( Options::TitleBar, isActive() );

    fixColorGroup(colorGroup);

    QRect trect = titlebar->geometry();
    QRect wrect = windowWrapper()->geometry();
    QRect mrect = rect();
    
    // draw black frame:
    p.setPen( Qt::black );
    p.drawRect( mrect );
    p.setPen( Qt::NoPen );

    p.setBrush( colorGroup.background() );

    // draw frame-background:
    p.drawRect(
	1,
	1,
	mrect.width() - 2,
	s_frameWidth );
    p.drawRect(
	1,
	mrect.height() - s_frameWidth - 1,
	mrect.width() - 2,
	s_frameWidth );
    p.drawRect(
	1,
	s_frameWidth + 1,
	s_frameWidth,
	mrect.height() - 2*s_frameWidth - 2 );
    p.drawRect(
	mrect.width() - s_frameWidth - 1,
	s_frameWidth + 1,
	s_frameWidth,
	mrect.height() - 2*s_frameWidth - 2 );
    
    // draw left frame:
    qDrawShadePanel( &p,
	1,
	wrect.y(),
	s_frameWidth,
	wrect.height() - s_buttonSize,
			colorGroup );

    // draw right frame:
    qDrawShadePanel( &p,
	mrect.width() - s_frameWidth - 1,
	wrect.y(),
	s_frameWidth,
	wrect.height() - s_buttonSize,
			colorGroup );

    // draw top frame:
    qDrawShadePanel( &p,
	s_frameWidth + s_buttonSize + 1,
	1,
	wrect.width() - 2*s_buttonSize,
	s_frameWidth,
			colorGroup );

    // draw bottom frame:
    qDrawShadePanel( &p,
	s_frameWidth + s_buttonSize + 1,
	mrect.height() - s_frameWidth - 1,
	wrect.width() - 2*s_buttonSize,
	s_frameWidth,
			colorGroup );

    // draw light corner parts:
    p.setPen( colorGroup.light() );

    // tl corner:
    p.drawLine(
	1,
	1,
	s_frameWidth + s_buttonSize - 1,
	1 );
    p.drawLine(
	1,
	1,
	1,
	s_frameWidth + s_buttonSize - 1 );
    // tr corner:
    p.drawLine(
	mrect.width() - 3,
	1,
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	1 );
    p.drawLine(
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	1,
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	s_frameWidth - 1 );
    p.drawLine(
	mrect.width() - s_frameWidth - 1,
	s_frameWidth,
	mrect.width() - s_frameWidth - 1,
	s_frameWidth + s_buttonSize - 1 );
    // br corner:
    p.drawLine(
	mrect.width() - 3,
	mrect.height() - s_frameWidth - s_buttonSize - 1,
	mrect.width() - s_frameWidth - 1,
	mrect.height() - s_frameWidth - s_buttonSize - 1 );
    p.drawLine(
	mrect.width() - s_frameWidth - 1,
	mrect.height() - s_frameWidth - s_buttonSize,
	mrect.width() - s_frameWidth - 1,
	mrect.height() - s_frameWidth - 1 );
    p.drawLine(
	mrect.width() - s_frameWidth - 2,
	mrect.height() - s_frameWidth - 1,
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	mrect.height() - s_frameWidth - 1 );
    p.drawLine(
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	mrect.height() - s_frameWidth,
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	mrect.height() - 2 );
    // bl corner:
    p.drawLine(
	s_frameWidth-1,
	mrect.height() - s_frameWidth - s_buttonSize - 1,
	2,
	mrect.height() - s_frameWidth - s_buttonSize - 1 );
    p.drawLine(
	1,
	mrect.height() - s_frameWidth - s_buttonSize - 1,
	1,
	mrect.height() - 3 );
    p.drawLine(
	s_frameWidth + s_buttonSize - 1,
	mrect.height() - s_frameWidth - 1,
	s_frameWidth + 1,
	mrect.height() - s_frameWidth - 1 );

//    p.setPen( Qt::NoPen );

    // draw dark corner parts:
    p.setPen( colorGroup.dark() );
    // tl corner:
    p.drawLine(
	1,
	s_frameWidth + s_buttonSize,
	s_frameWidth,
	s_frameWidth + s_buttonSize );
    p.drawLine(
	s_frameWidth,
	s_frameWidth + s_buttonSize - 1,
	s_frameWidth,
	s_frameWidth );
    p.drawLine(
	s_frameWidth + 1,
	s_frameWidth,
	s_frameWidth + s_buttonSize,
	s_frameWidth );
    p.drawLine(
	s_frameWidth + s_buttonSize,
	s_frameWidth,
	s_frameWidth + s_buttonSize,
	1 );
    // tr corner:
    p.drawLine(
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	s_frameWidth,
	mrect.width() - s_frameWidth - 2,
	s_frameWidth );
    p.drawLine(
	mrect.width() - s_frameWidth - 1,
	s_frameWidth + s_buttonSize,
	mrect.width() - 2,
	s_frameWidth + s_buttonSize );
    p.drawLine(
	mrect.width() - 2,
	s_frameWidth + s_buttonSize,
	mrect.width() - 2,
	1 );
    // br corner:
    p.drawLine(
	mrect.width() - s_frameWidth - s_buttonSize - 1,
	mrect.height() - 2,
	mrect.width() - 3,
	mrect.height() - 2 );
    p.drawLine(
	mrect.width() - 2,
	mrect.height() - 2,
	mrect.width() - 2,
	mrect.height() - s_frameWidth - s_buttonSize - 2 );
    // bl corner:
    p.drawLine(
	1,
	mrect.height() - 2,
	s_frameWidth + s_buttonSize,
	mrect.height() - 2 );
    p.drawLine(
	s_frameWidth + s_buttonSize,
	mrect.height() - 3,
	s_frameWidth + s_buttonSize,
	mrect.height() - s_frameWidth - 1 );
    p.drawLine(
	s_frameWidth,
	mrect.height() - s_frameWidth - 1,
	s_frameWidth,
	mrect.height() - s_frameWidth - s_buttonSize - 1 );

    // Draw something behind wrapped window when shaded
    p.drawLine(
	wrect.x(),
	wrect.y(),
	wrect.x() + wrect.width() - 1,
	wrect.y() );


    p.setPen( Qt::NoPen );

    // draw titlebar:
    p.drawRect( trect );
    qDrawShadePanel( &p, trect,
	colorGroup );

    // draw caption:	
    p.setFont( options->font( true ) );
    p.setPen( options->color( Options::Font, isActive() ) );
    p.drawText(
	trect.x() + 3,
	trect.y(),
	trect.width() - 6,
	trect.height(),
	AlignHCenter | AlignVCenter, caption() );
}

Client::MousePosition MwmClient::mousePosition( const QPoint& p ) const
{
    const int range = s_frameWidth + s_buttonSize;
    const int border = s_frameWidth + 1;

    MousePosition m = Nowhere;

    if ( ( p.x() > border && p.x() < width() - border )
         && ( p.y() > border && p.y() < height() - border ) )
        return Center;

    if ( p.y() <= range && p.x() <= range)
        m = TopLeft;
    else if ( p.y() >= height()-range && p.x() >= width()-range)
        m = BottomRight;
    else if ( p.y() >= height()-range && p.x() <= range)
        m = BottomLeft;
    else if ( p.y() <= range && p.x() >= width()-range)
        m = TopRight;
    else if ( p.y() <= border )
        m = Top;
    else if ( p.y() >= height()-border )
        m = Bottom;
    else if ( p.x() <= border )
        m = Left;
    else if ( p.x() >= width()-border )
        m = Right;
    else
        m = Center;
    return m;
}

void MwmClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if ( titlebar->geometry().contains( e->pos() ) )
        workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );

    workspace()->requestFocus(this);
}

void MwmClient::init()
{
    Client::init();
}

extern "C"
{
    Client* allocate( Workspace* ws, WId w )
    {
        return( new MwmClient( ws, w ) );
    }
    void init()
    {
    }
    void reset()
    {
        // Ensure changes in tooltip state get applied
        Workspace::self()->slotResetAllClientsDelayed();
    }
    void deinit()
    {
    }
}

#include "mwmclient.moc"
