/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
//#define QT_CLEAN_NAMESPACE
#include "popupinfo.h"
#include "workspace.h"
#include "client.h"
#include <qpainter.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qstyle.h>
#undef Bool // f**king X11
#include <kglobal.h>
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qcursor.h>
#include <kstringhandler.h>

// specify externals before namespace

extern QPixmap* kwin_get_menu_pix_hack();

using namespace KWinInternal;

PopupInfo::PopupInfo( Workspace *ws, const char *name )
    : QWidget( 0, name, WStyle_Customize | WStyle_NoBorder )
{
    no_tasks = i18n("*** No Tasks ***");
    m = DesktopMode; // init variables
    wspace = ws;
    reconfigure();
    reset();
    connect(&delayedHideTimer, SIGNAL(timeout()), this, SLOT(hide()));
}

PopupInfo::~PopupInfo()
{
}


/*!
  Sets the current mode to \a mode, either DesktopListMode or WindowsMode

  \sa mode()
 */
void PopupInfo::setMode( Mode mode )
{
    m = mode;
}


/*!
  Resets the tab box to display the active client in WindowsMode, or the
  current desktop in DesktopListMode
 */
void PopupInfo::reset()
{
    QFont f = font();
    f.setBold( TRUE );
    f.setPointSize( 14 );
    setFont( f );

    wmax = 0;

    desk = workspace()->currentDesktop();

    QDesktopWidget* desktop = qApp->desktop();
    int screen =  desktop->screenNumber( QCursor::pos() );
    QRect r = desktop->screenGeometry(screen);

    int w = QMIN( QMAX( wmax + 20, r.width()/3 ), r.width() );
    setGeometry( (r.width()-w)/2 + r.x(),
		 r.height()/2-fontMetrics().height()*2-10 + r.y(),
		 w, fontMetrics().height()*2 + 20 );

    wmax = QMIN( wmax, width() - 12 );
}


/*!
  Returns the currently displayed client ( only works in WindowsMode ).
  Returns 0 if no client is displayed.
 */
Client* PopupInfo::currentClient()
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
int PopupInfo::currentDesktop()
{
    if ( mode() == DesktopListMode || mode() == DesktopMode )
	return desk;
    else
	return -1;
}


/*!
  Reimplemented to raise the tab box as well
 */
void PopupInfo::showEvent( QShowEvent* )
{
}


/*!
  hide the icon box if necessary
 */
void PopupInfo::hideEvent( QHideEvent* )
{
}


/*!
  Paints the tab box
 */
void PopupInfo::paintEvent( QPaintEvent* )
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
    paintContents();
}


/*!
  Paints the contents of the tab box. Used in paintEvent() and
  whenever the contents changes.
 */
void PopupInfo::paintContents()
{
    QPixmap* menu_pix = kwin_get_menu_pix_hack();
    QPainter p( this );
    QRect r( 6, 6, width()-12, height()-32 );
    p.fillRect( r, colorGroup().brush( QColorGroup::Background ) );

    p.drawText( r, AlignCenter, workspace()->desktopName(desk) );
    int x = (width() - workspace()->numberOfDesktops() * 20 )/2;
    int y = height() - 16;
    QFont f( font() );
    f.setPointSize( 12 );
    f.setBold( FALSE );
    p.setFont(f );
}

void PopupInfo::hide()
{
    delayedHideTimer.stop();
    QWidget::hide();
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent (qt_xdisplay(), EnterNotify, &otherEvent ) )
	;
}


void PopupInfo::reconfigure()
{
    KConfig * c(KGlobal::config());
    c->setGroup("PopupInfo");
    m_show = c->readNumEntry("Show", false);
    m_delayTime = c->readNumEntry("DelayTime", 250);
}

void PopupInfo::showInfo()
{
    if (m_show) {
       reset();
       show();
       raise();
       delayedHideTimer.start(m_delayTime, true);
    }
}

#include "popupinfo.moc"
