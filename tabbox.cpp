/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
//#define QT_CLEAN_NAMESPACE
#include "tabbox.h"
#include "workspace.h"
#include "client.h"
#include <qpainter.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qstyle.h>
#undef Bool // f**king X11
#include <kglobal.h>
#include <kconfig.h>
#include <klocale.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qcursor.h>

// specify externals before namespace

extern QPixmap* kwin_get_menu_pix_hack();

using namespace KWinInternal;

TabBox::TabBox( Workspace *ws, const char *name )
    : QWidget( 0, name, WStyle_Customize | WStyle_NoBorder )
{
    no_tasks = i18n("*** No Tasks ***");
    wspace = ws;
    reconfigure();
    reset();
    connect(&delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
}

TabBox::~TabBox()
{
}


/*!
  Sets the current mode to \a mode, either DesktopListMode or WindowsMode

  \sa mode()
 */
void TabBox::setMode( Mode mode )
{
    m = mode;
}


/*!
  Resets the tab box to display the active client in WindowsMode, or the
  current desktop in DesktopListMode
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
	int cw = fm.width(no_tasks)+20;
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
    else { // DesktopListMode
	desk = workspace()->currentDesktop();
    }


    QDesktopWidget* desktop = qApp->desktop();
    int screen =  desktop->screenNumber( QCursor::pos() );
    QRect r = desktop->screenGeometry(screen);


    int w = QMAX( wmax + 20, r.width()/3 );
    setGeometry( (r.width()-w)/2 + r.x(),
		 r.height()/2-fontMetrics().height()*2-10 + r.y(),
		 w, fontMetrics().height()*4 + 20 );

    wmax = QMIN( wmax, width() - 12 );
}


/*!
  Shows the next or previous item, depending on \a next
 */
void TabBox::nextPrev( bool next)
{
    if ( mode() == WindowsMode ) {
	Client* firstClient = 0;
	do {
	    if ( next )
		client = workspace()->nextClient(client);
	    else
		client = workspace()->previousClient(client);
	    if (!firstClient) {
		// When we see our first client for the second time,
		// it's time to stop.
	        firstClient = client;
	    }
	    else if (client == firstClient) {
		// No candidates found.
		client = 0;
	        break;
            }
	} while (client &&
		 (( !options_traverse_all &&
                    !client->isOnDesktop(workspace()->currentDesktop()) ) ||
		  ( client->isIconified()  && client->mainClient() != client ))
		 );


	if (!options_traverse_all && client
	    && !client->isOnDesktop(workspace()->currentDesktop()))
	    client = 0;
    }
    else if( mode() == DesktopMode ) {
	if ( next )
	    desk = workspace()->nextDesktop( desk );
	else
	    desk = workspace()->previousDesktop( desk );
    }
    else { // DesktopListMode
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
    if (!workspace()->hasClient( client ))
        return 0;
    return client;
}

/*!
  Returns the currently displayed virtual desktop ( only works in
  DesktopListMode )
  Returns -1 if no desktop is displayed.
 */
int TabBox::currentDesktop()
{
    if ( mode() == DesktopListMode || mode() == DesktopMode )
	return desk;
    else
	return -1;
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
#if QT_VERSION < 300
	style().drawPanel( &p, 0, 0, width(), height(), colorGroup(), FALSE );
	style().drawPanel( &p, 4, 4, width()-8, height()-8, colorGroup(), TRUE );
#else
	style().drawPrimitive( QStyle::PE_Panel, &p, QRect( 0, 0, width(), height() ),
                               colorGroup(), QStyle::Style_Default );
	style().drawPrimitive( QStyle::PE_Panel, &p, QRect( 4, 4, width()-8, height()-8 ),
                               colorGroup(), QStyle::Style_Sunken );
#endif
    }
    paintContents();
}


/*!
  Paints the contents of the tab box. Used in paintEvent() and
  whenever the contents changes.
 */
void TabBox::paintContents()
{
    QPixmap* menu_pix = kwin_get_menu_pix_hack();
    QPainter p( this );
    QRect r( 6, 6, width()-12, height()-32 );
    p.fillRect( r, colorGroup().brush( QColorGroup::Background ) );
    if ( mode () == WindowsMode ) {
	if ( currentClient() ) {
	    QString s;
	    if (!client->isOnDesktop(workspace()->currentDesktop())){
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
	    p.drawText( r, AlignCenter, no_tasks);
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
    } else { // DesktopMode || DesktopListMode
	p.drawText( r, AlignCenter, workspace()->desktopName(desk) );
	int x = (width() - workspace()->numberOfDesktops() * 20 )/2;
	int y = height() - 26;
	QFont f( font() );
	f.setPointSize( 12 );
	f.setBold( FALSE );
	p.setFont(f );

	// In DesktopMode, start at the current desktop
	// In DesktopListMode, start at desktop #1
	int iDesktop = (mode() == DesktopMode) ? workspace()->currentDesktop() : 1;
	for ( int i = 1; i <= workspace()->numberOfDesktops(); i++ ) {
	    p.setPen( iDesktop == desk?
		      colorGroup().highlight():colorGroup().background() );
	    p.drawRect( x-2, y-2, 20, 20 );
	    qDrawWinPanel( &p, QRect( x, y, 16, 16), colorGroup(), FALSE,
			    &colorGroup().brush(QColorGroup::Base ) );
	    p.setPen( colorGroup().text() );
	    p.drawText( x, y, 16, 16, AlignCenter, QString::number(iDesktop) );
	    x += 20;
	    
	    if( mode() == DesktopMode )
	        iDesktop = workspace()->nextDesktop( iDesktop );
	    else
	        iDesktop++;
	}
    }
}

void TabBox::hide()
{
    delayedShowTimer.stop();
    QWidget::hide();
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent (qt_xdisplay(), EnterNotify, &otherEvent ) )
	;
}


void TabBox::reconfigure()
{
    KConfig * c(KGlobal::config());
    c->setGroup("TabBox");
    options_traverse_all = c->readNumEntry("TraverseAll", false );
}

/*!
  Rikkus: please document!   (Matthias)

  Ok, here's the docs :)

  You call delayedShow() instead of show() directly.

  If the 'ShowDelay' setting is false, show() is simply called.

  Otherwise, we start a timer for the delay given in the settings and only
  do a show() when it times out.

  This means that you can alt-tab between windows and you don't see the
  tab box immediately. Not only does this make alt-tabbing faster, it gives
  less 'flicker' to the eyes. You don't need to see the tab box if you're
  just quickly switching between 2 or 3 windows. It seems to work quite
  nicely.
 */
void TabBox::delayedShow()
{
    KConfig * c(KGlobal::config());
    c->setGroup("TabBox");
    bool delay = c->readNumEntry("ShowDelay", true);

    if (!delay) {
	show();
	return;
    }

    int delayTime = c->readNumEntry("DelayTime", 90);
    delayedShowTimer.start(delayTime, true);
}

#include "tabbox.moc"
