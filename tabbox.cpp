/*****************************************************************
kwin - the KDE window manager
								  
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#include "tabbox.h"
#include "workspace.h"
#include "client.h"
#include <qpainter.h>
#include <qlabel.h>
#include <qdrawutil.h>
#undef Bool // f**king X11
#include <kglobal.h>
#include <kconfig.h>

const bool options_traverse_all = FALSE; // TODO

TabBox::TabBox( Workspace *ws, const char *name )
    : QWidget( 0, name, WStyle_Customize | WStyle_NoBorder )
{
    wspace = ws;
    reset();
    connect(&delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
}

TabBox::~TabBox()
{
}


/*!
  Sets the current mode to \a mode, either DesktopMode or WindowsMode

  \sa mode()
 */
void TabBox::setMode( Mode mode )
{
    m = mode;
}


/*!
  Resets the tab box to display the active client in WindowsMode, or the
  current desktop in DesktopMode
 */
void TabBox::reset()
{
    QFont f = font();
    f.setBold( TRUE );
    f.setPointSize( 14 );
    setFont( f );

    wmax = 0;

    if ( mode() == WindowsMode ) {
	client = workspace()->activeClient();
	clients.clear();
	Client* c = workspace()->nextClient( client );
	Client* stop = c;
	QFontMetrics fm( fontMetrics() );
	int cw = 0;
	while ( c ) {
	    if ( (options_traverse_all ||c->isOnDesktop(workspace()->currentDesktop()))
		 && (!c->isIconified() || c->mainClient() == c ) ) {
		if ( client == c )
		    clients.prepend( c );
		else
		    clients += c;
		cw = fm.width( c->caption() ) + 40;
		if ( cw > wmax )
		    wmax = cw;
	    }
	    c = workspace()->nextClient( c );
	    if ( c == stop )
		break;
	}
	wmax = QMAX( wmax, int(clients.count())*20 );
    }
    else { // DesktopMode
	desk = workspace()->currentDesktop();
    }

    int w = QMAX( wmax + 20, qApp->desktop()->width()/3 );
    setGeometry( (qApp->desktop()->width()-w)/2,
		 qApp->desktop()->height()/2-fontMetrics().height()*2-10,
		 w, fontMetrics().height()*4 + 20 );
    wmax = QMIN( wmax, width() - 12 );
}


/*!
  Shows the next or previous item, depending on \a next
 */
void TabBox::nextPrev( bool next)
{
    if ( mode() == WindowsMode ) {
	Client* sign = client;
	do {
	    if (client != sign && !sign)
		sign = client;
	    if ( next )
		client = workspace()->nextClient(client);
	    else
		client = workspace()->previousClient(client);
	} while (client != sign && client &&
		 (( !options_traverse_all &&
                    !client->isOnDesktop(workspace()->currentDesktop()) ) ||
		  ( client->isIconified()  && client->mainClient() != client ))
		 );


	if (!options_traverse_all && client
	    && !client->isOnDesktop(workspace()->currentDesktop()))
	    client = 0;

    }
    else { // DesktopMode
	if ( next ) {
	    desk++;
	    if ( desk > workspace()->numberOfDesktops() )
		desk = 1;
	} else {
	    desk--;
	    if ( desk < 1 )
		desk = workspace()->numberOfDesktops();
	}
    }

    paintContents();
}



/*!
  Returns the currently displayed client ( only works in WindowsMode ).
  Returns 0 if no client is displayed.
 */
Client* TabBox::currentClient()
{
    if ( mode() != WindowsMode )
	return 0;
    return client;
}

/*!
  Returns the currently displayed virtual desktop ( only works in
  DesktopMode )
  Returns -1 if no desktop is displayed.
 */
int TabBox::currentDesktop()
{
    if ( mode() != DesktopMode )
	return -1;
    return desk;
}


/*!
  Reimplemented to raise the tab box as well
 */
void TabBox::showEvent( QShowEvent* )
{
    raise();
}


/*!
  hide the icon box if necessary
 */
void TabBox::hideEvent( QHideEvent* )
{
}


/*!
  Paints the tab box
 */
void TabBox::paintEvent( QPaintEvent* )
{
    {
	QPainter p( this );
	style().drawPanel( &p, 0, 0, width(), height(), colorGroup(), FALSE );
	style().drawPanel( &p, 4, 4, width()-8, height()-8, colorGroup(), TRUE );
    }
    paintContents();
}


/*!
  Paints the contents of the tab box. Used in paintEvent() and
  whenever the contents changes.
 */
void TabBox::paintContents()
{
    extern QPixmap* kwin_get_menu_pix_hack();
    QPixmap* menu_pix = kwin_get_menu_pix_hack();
    QPainter p( this );
    QRect r( 6, 6, width()-12, height()-32 );
    p.fillRect( r, colorGroup().brush( QColorGroup::Background ) );
    if ( mode () == WindowsMode ) {
	if ( currentClient() ) {
	    QString s;
	    if (!client->isOnDesktop(workspace()->currentDesktop())){
		s = KWM::desktopName(client->desktop());
		s.append(": ");
	    }

	    if (client->isIconified())
		s += QString("(")+client->caption()+")";
	    else
		s += client->caption();
	    int textw = fontMetrics().width( s );
	    r.setLeft( r.left() + (r.width() - textw)/2);

	    if ( !client->icon().isNull() ) {
		int py = r.center().y() - 16;
		r.setLeft( r.left() + 20 );
		p.drawPixmap( r.left()-42, py, client->icon() );
	    }

	    p.drawText( r, AlignVCenter, s );

	}
	else {
	    r.setBottom( r.bottom() + 20 );
	    p.drawText( r, AlignCenter, "*** No Tasks ***" );
	}

	int x = (width() - clients.count() * 20 )/2;
	int y = height() - 26;
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	    if ( workspace()->hasClient( *it ) ) { // safety
		p.save();
		if ( !(*it)->miniIcon().isNull() )
		    p.drawPixmap( x, y, (*it)->miniIcon() );
		else if ( menu_pix )
		    p.drawPixmap( x, y, *menu_pix );
		p.setPen( (*it)==currentClient()?
			   colorGroup().highlight():colorGroup().background() );
		p.drawRect( x-2, y-2, 20, 20 );
		p.setPen( colorGroup().foreground() );
		x += 20;
	    }
	}
    } else { // DesktopMode
	p.drawText( r, AlignCenter, QString::number( desk ) );
	int x = (width() - workspace()->numberOfDesktops() * 20 )/2;
	int y = height() - 26;
	QFont f( font() );
	f.setPointSize( 12 );
	f.setBold( FALSE );
	p.setFont(f );
	for ( int i = 1; i <= workspace()->numberOfDesktops(); i++ ) {
	    p.setPen( i == desk?
		      colorGroup().highlight():colorGroup().background() );
	    p.drawRect( x-2, y-2, 20, 20 );
	    qDrawWinPanel( &p, QRect( x, y, 16, 16), colorGroup(), FALSE,
			     &colorGroup().brush(QColorGroup::Base ) );
	    p.setPen( colorGroup().text() );
	    p.drawText( x, y, 16, 16, AlignCenter, QString::number(i) );
	    x += 20;
	}
    }
}

  void
TabBox::hide()
{
  delayedShowTimer.stop();
  QWidget::hide();
}

  void
TabBox::delayedShow()
{
  KConfig * c(KGlobal::config());
  c->setGroup("TabBox");
  bool delay = c->readNumEntry("ShowDelay", false);

  if (!delay) {
    show();
    return;
  }

  int delayTime = c->readNumEntry("DelayTime", 400);
  delayedShowTimer.start(delayTime, true);
}

