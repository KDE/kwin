#include "tabbox.h"
#include "workspace.h"
#include "client.h"
#include <qpainter.h>

const bool options_traverse_all = FALSE; // TODO

TabBox::TabBox( Workspace *ws, const char *name=0 )
    : QWidget( 0, name, WStyle_Customize | WStyle_NoBorder )
{
    wspace = ws;
    reset();
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


    // TODO icons etc.
    setGeometry( qApp->desktop()->width()/4,
		 qApp->desktop()->height()/2-fontMetrics().height()*2,
		 qApp->desktop()->width()/2, fontMetrics().height()*4 );

    if ( mode() == WindowsMode ) {
	client = workspace()->activeClient();
	// todo build window list, consider options_traverse_all
    }
    else { // DesktopMode
	desk = wspace->currentDesktop();
    }

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
		 !options_traverse_all &&
		 !client->isOnDesktop(workspace()->currentDesktop()));

	if (!options_traverse_all && client
	    && !client->isOnDesktop(workspace()->currentDesktop()))
	    client = 0;
    }
    else { // DesktopMode
	if ( next ) {
	    desk++;
	    if ( desk > wspace->numberOfDesktops() )
		desk = 1;
	} else {
	    desk--;
	    if ( desk < 1 )
		desk = wspace->numberOfDesktops();
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
    QPainter p( this );
    QRect r(6, 6, width()-12, height()-12 );
    p.fillRect( r, colorGroup().brush( QColorGroup::Background ) );
    if ( mode () == WindowsMode ) {
	if ( currentClient() ) {
	    QString s;
	    if (!client->isOnDesktop(workspace()->currentDesktop())){
		//TODO s = KWM::getDesktopName(client->desktop);
		s.append(": ");
	    }
		
	    if (client->isIconified())
		s += QString("(")+client->caption()+")";
	    else
		s += client->caption();
	    if ( p.fontMetrics().width( s ) > r.width() )
		p.drawText( r, AlignLeft, s );
	    else
		p.drawText( r, AlignCenter, s );
		
	}
	else {
		    p.drawText( r, AlignCenter, "*** No Tasks ***" );
	}
    } else { // DesktopMode
	QString s;
	s.setNum( desk );
	p.drawText( r, AlignCenter, s );
    }
}
