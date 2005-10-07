/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils.h"

#include <unistd.h>

#ifndef KCMRULES

#include <kxerrorhandler.h>
#include <assert.h>
#include <kdebug.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <QX11Info>
#include <stdio.h>

#include "atoms.h"
#include "notifications.h"

#endif

namespace KWinInternal
{

#ifndef KCMRULES

// used to store the return values of
// XShapeQueryExtension.
// Necessary since shaped window are an extension to X
int Shape::kwin_has_shape = 0;
int Shape::kwin_shape_event = 0;

// does the window w  need a shape combine mask around it?
bool Shape::hasShape( WId w)
    {
    int xws, yws, xbs, ybs;
    unsigned int wws, hws, wbs, hbs;
    int boundingShaped = 0, clipShaped = 0;
    if (!kwin_has_shape)
        return FALSE;
    XShapeQueryExtents(QX11Info::display(), w,
                       &boundingShaped, &xws, &yws, &wws, &hws,
                       &clipShaped, &xbs, &ybs, &wbs, &hbs);
    return boundingShaped != 0;
    }

int Shape::shapeEvent()
    {
    return kwin_shape_event;
    }

void Shape::init()
    {
    int dummy;
    kwin_has_shape =
      XShapeQueryExtension(QX11Info::display(), &kwin_shape_event, &dummy);
    }

void Motif::readFlags( WId w, bool& noborder, bool& resize, bool& move,
    bool& minimize, bool& maximize, bool& close )
    {
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char* data;
    MwmHints* hints = 0;
    if ( XGetWindowProperty( QX11Info::display(), w, atoms->motif_wm_hints, 0, 5,
                             FALSE, atoms->motif_wm_hints, &type, &format,
                             &length, &after, &data ) == Success ) 
        {
        if ( data )
            hints = (MwmHints*) data;
        }
    noborder = false;
    resize = true;
    move = true;
    minimize = true;
    maximize = true;
    close = true;
    if ( hints ) 
        {
    // To quote from Metacity 'We support those MWM hints deemed non-stupid'
        if ( hints->flags & MWM_HINTS_FUNCTIONS ) 
            {
            // if MWM_FUNC_ALL is set, other flags say what to turn _off_
            bool set_value = (( hints->functions & MWM_FUNC_ALL ) == 0 );
            resize = move = minimize = maximize = close = !set_value;
            if( hints->functions & MWM_FUNC_RESIZE )
                resize = set_value;
            if( hints->functions & MWM_FUNC_MOVE )
                move = set_value;
            if( hints->functions & MWM_FUNC_MINIMIZE )
                minimize = set_value;
            if( hints->functions & MWM_FUNC_MAXIMIZE )
                maximize = set_value;
            if( hints->functions & MWM_FUNC_CLOSE )
                close = set_value;
            }
        if ( hints->flags & MWM_HINTS_DECORATIONS ) 
            {
            if ( hints->decorations == 0 )
                noborder = true;
            }
        XFree( data );
        }
    }

//************************************
// KWinSelectionOwner
//************************************

KWinSelectionOwner::KWinSelectionOwner( int screen_P )
    : KSelectionOwner( make_selection_atom( screen_P ), screen_P )
    {
    }

Atom KWinSelectionOwner::make_selection_atom( int screen_P )
    {
    if( screen_P < 0 )
        screen_P = DefaultScreen( QX11Info::display());
    char tmp[ 30 ];
    sprintf( tmp, "WM_S%d", screen_P );
    return XInternAtom( QX11Info::display(), tmp, False );
    }

void KWinSelectionOwner::getAtoms()
    {
    KSelectionOwner::getAtoms();
    if( xa_version == None )
        {
        Atom atoms[ 1 ];
        const char* const names[] =
            { "VERSION" };
        XInternAtoms( QX11Info::display(), const_cast< char** >( names ), 1, False, atoms );
        xa_version = atoms[ 0 ];
        }
    }

void KWinSelectionOwner::replyTargets( Atom property_P, Window requestor_P )
    {
    KSelectionOwner::replyTargets( property_P, requestor_P );
    Atom atoms[ 1 ] = { xa_version };
    // PropModeAppend !
    XChangeProperty( QX11Info::display(), requestor_P, property_P, XA_ATOM, 32, PropModeAppend,
        reinterpret_cast< unsigned char* >( atoms ), 1 );
    }

bool KWinSelectionOwner::genericReply( Atom target_P, Atom property_P, Window requestor_P )
    {
    if( target_P == xa_version )
        {
        long version[] = { 2, 0 };
        XChangeProperty( QX11Info::display(), requestor_P, property_P, XA_INTEGER, 32,
            PropModeReplace, reinterpret_cast< unsigned char* >( &version ), 2 );
        }
    else
        return KSelectionOwner::genericReply( target_P, property_P, requestor_P );
    return true;    
    }

Atom KWinSelectionOwner::xa_version = None;


QByteArray getStringProperty(WId w, Atom prop, char separator)
    {
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    QByteArray result = "";
    KXErrorHandler handler; // ignore errors
    status = XGetWindowProperty( QX11Info::display(), w, prop, 0, 10000,
                                 FALSE, XA_STRING, &type, &format,
                                 &nitems, &extra, &data );
    if ( status == Success) 
        {
        if (data && separator) 
            {
            for (int i=0; i<(int)nitems; i++)
                if (!data[i] && i+1<(int)nitems)
                    data[i] = separator;
            }
        if (data)
            result = (const char*) data;
        XFree(data);
        }
    return result;
    }

static Time next_x_time;
static Bool update_x_time_predicate( Display*, XEvent* event, XPointer )
{
    if( next_x_time != CurrentTime )
        return False;
    // from qapplication_x11.cpp
    switch ( event->type ) {
    case ButtonPress:
	// fallthrough intended
    case ButtonRelease:
	next_x_time = event->xbutton.time;
	break;
    case MotionNotify:
	next_x_time = event->xmotion.time;
	break;
    case KeyPress:
	// fallthrough intended
    case KeyRelease:
	next_x_time = event->xkey.time;
	break;
    case PropertyNotify:
	next_x_time = event->xproperty.time;
	break;
    case EnterNotify:
    case LeaveNotify:
	next_x_time = event->xcrossing.time;
	break;
    case SelectionClear:
	next_x_time = event->xselectionclear.time;
	break;
    default:
	break;
    }
    return False;
}

/*
 Updates QX11Info::appTime(). This used to simply fetch current timestamp from the server,
 but that can cause QX11Info::appTime() to be newer than timestamp of events that are
 still in our events queue, thus e.g. making XSetInputFocus() caused by such
 event to be ignored. Therefore events queue is searched for first
 event with timestamp, and extra PropertyNotify is generated in order to make
 sure such event is found.
*/
void updateXTime()
    {
    static QWidget* w = 0;
    if ( !w )
        w = new QWidget;
    long data = 1;
    XChangeProperty(QX11Info::display(), w->winId(), atoms->kwin_running, atoms->kwin_running, 32,
                    PropModeAppend, (unsigned char*) &data, 1);
    next_x_time = CurrentTime;
    XEvent dummy;
    XCheckIfEvent( QX11Info::display(), &dummy, update_x_time_predicate, NULL );
    if( next_x_time == CurrentTime )
        {
        XSync( QX11Info::display(), False );
        XCheckIfEvent( QX11Info::display(), &dummy, update_x_time_predicate, NULL );
        }
    assert( next_x_time != CurrentTime );
    QX11Info::setAppTime( next_x_time );
    XEvent ev; // remove the PropertyNotify event from the events queue
    XWindowEvent( QX11Info::display(), w->winId(), PropertyChangeMask, &ev );
    }

static int server_grab_count = 0;

void grabXServer()
    {
    if( ++server_grab_count == 1 )
        XGrabServer( QX11Info::display());
    }

void ungrabXServer()
    {
    assert( server_grab_count > 0 );
    if( --server_grab_count == 0 )
        {
        XUngrabServer( QX11Info::display());
        XFlush( QX11Info::display());
        Notify::sendPendingEvents();
        }
    }

bool grabbedXServer()
    {
    return server_grab_count > 0;
    }

#endif

bool isLocalMachine( const QByteArray& host )
    {
#ifdef HOST_NAME_MAX
    char hostnamebuf[HOST_NAME_MAX];
#else
    char hostnamebuf[256];
#endif
    if (gethostname (hostnamebuf, sizeof hostnamebuf) >= 0) 
        {
        hostnamebuf[sizeof(hostnamebuf)-1] = 0;
        if (host == hostnamebuf)
            return true;
        char *dot = strchr(hostnamebuf, '.');
        if (dot && !(*dot = 0) && host == hostnamebuf)
            return true;
        }
    return false;
    }

#ifndef KCMRULES
ShortcutDialog::ShortcutDialog( const KShortcut& cut )
    : KShortcutDialog( cut, false /*TODO???*/ )
    {
    // make it a popup, so that it has the grab
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XChangeWindowAttributes( QX11Info::display(), winId(), CWOverrideRedirect, &attrs );
    setWindowFlags( Qt::Popup );
    }

void ShortcutDialog::accept()
    {
    for( int i = 0;
         ;
         ++i )
        {
        KKeySequence seq = shortcut().seq( i );
        if( seq.isNull())
            break;
        if( seq.key( 0 ) == Qt::Key_Escape )
            {
            reject();
            return;
            }
        if( seq.key( 0 ) == Qt::Key_Space )
            { // clear
            setShortcut( KShortcut());
            KShortcutDialog::accept();
            return;
            }
        if( seq.key( 0 ).modFlags() == 0 )
            { // no shortcuts without modifiers
            KShortcut cut = shortcut();
            cut.setSeq( i, KKeySequence());
            setShortcut( cut );
            return;
            }
        }
    KShortcutDialog::accept();
    }
#endif


} // namespace

#ifndef KCMRULES
#include "utils.moc"
#endif
