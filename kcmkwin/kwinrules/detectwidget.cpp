/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "detectwidget.h"

#include <kapplication.h>
#include <klocale.h>
#include <kdebug.h>
#include <kxerrorhandler.h>
#include <kwin.h>
#include <qlabel.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

namespace KWinInternal
{

DetectWidget::DetectWidget( QWidget* parent, const char* name )
: DetectWidgetBase( parent, name )
    {
    }

DetectDialog::DetectDialog( QWidget* parent, const char* name )
: KDialogBase( parent, name, true, "", Ok /*| Cancel*/ )
, grabber( NULL )
    {
    widget = new DetectWidget( this );
    setMainWidget( widget );
    }

void DetectDialog::detect( WId window )
    {
    if( window == 0 )
        selectWindow();
    else
        readWindow( window );
    }

static QCString getStringProperty(WId w, Atom prop, char separator=0)
    {
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    QCString result = "";
    status = XGetWindowProperty( qt_xdisplay(), w, prop, 0, 10000,
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

void DetectDialog::readWindow( WId w )
    {
    if( w == 0 )
        {
        emit detectionDone( false );
        return;
        }
    KXErrorHandler err;
    XClassHint hint;
    XGetClassHint( qt_xdisplay(), w, &hint );
    Atom wm_role = XInternAtom( qt_xdisplay(), "WM_WINDOW_ROLE", False );
    KWin::WindowInfo info = KWin::windowInfo( w );
    if( !info.valid())
        {
        emit detectionDone( false );
        return;
        }
    wmclass_class = hint.res_class;
    wmclass_name = hint.res_name;
    role = getStringProperty( w, wm_role );
    type = info.windowType( NET::NormalMask | NET::DesktopMask | NET::DockMask
        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask | NET::OverrideMask | NET::TopMenuMask
        | NET::UtilityMask | NET::SplashMask );
    title = info.name();
    extrarole = ""; // TODO
    machine = getStringProperty( w, XA_WM_CLIENT_MACHINE );
    if( err.error( true ))
        {
        emit detectionDone( false );
        return;
        }
    executeDialog();
    }

void DetectDialog::executeDialog()
    {
    static const char* const types[] =
        {
        I18N_NOOP( "Normal Window" ),
        I18N_NOOP( "Desktop" ),
        I18N_NOOP( "Dock (panel)" ),
        I18N_NOOP( "Toolbar" ),
        I18N_NOOP( "Torn Off Menu" ),
        I18N_NOOP( "Dialog Window" ),
        I18N_NOOP( "Override Type" ),
        I18N_NOOP( "Standalone Menubar" ),
        I18N_NOOP( "Utility Window" ),
        I18N_NOOP( "Splashscreen" )
        };
    widget->class_label->setText( wmclass_class + " (" + wmclass_name + ' ' + wmclass_class + ")" );
    widget->role_label->setText( role );
    if( type == NET::Unknown )
        widget->type_label->setText( i18n( "Unknown - will be treated as Normal Window" ));
    else
        widget->type_label->setText( i18n( types[ type ] ));
    widget->title_label->setText( title );
    widget->extrarole_label->setText( extrarole );
    widget->machine_label->setText( machine );
    emit detectionDone( exec() == QDialog::Accepted );
    }

void DetectDialog::selectWindow()
    {
    // use a dialog, so that all user input is blocked
    // use WX11BypassWM and moving away so that it's not actually visible
    // grab only mouse, so that keyboard can be used e.g. for switching windows
    grabber = new QDialog( NULL, NULL, true, WX11BypassWM );
    grabber->move( -1000, -1000 );
    grabber->show();
    grabber->grabMouse( crossCursor );
    grabber->installEventFilter( this );
    }

bool DetectDialog::eventFilter( QObject* o, QEvent* e )
    {
    if( o != grabber )
        return false;
    if( e->type() != QEvent::MouseButtonRelease )
        return false;
    delete grabber;
    grabber = NULL;
    readWindow( findWindow());
    return true;
    }

WId DetectDialog::findWindow()
    {
    Window root;
    Window child;
    uint mask;
    int rootX, rootY, x, y;
    Window parent = qt_xrootwin();
    Atom wm_state = XInternAtom( qt_xdisplay(), "WM_STATE", False );
    for( int i = 0;
         i < 10;
         ++i )
        {
        XQueryPointer( qt_xdisplay(), parent, &root, &child,
            &rootX, &rootY, &x, &y, &mask );
        if( child == None )
            return 0;
        Atom type;
        int format;
        unsigned long nitems, after;
        unsigned char* prop;
        if( XGetWindowProperty( qt_xdisplay(), child, wm_state, 0, 0, False, AnyPropertyType,
	    &type, &format, &nitems, &after, &prop ) == Success )
            {
	    if( prop != NULL )
	        XFree( prop );
	    if( type != None )
	        return child;
            }
        parent = child;
        }
    return 0;
    }

} // namespace

#include "detectwidget.moc"
