/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

//#ifndef QT_CLEAN_NAMESPACE
//#define QT_CLEAN_NAMESPACE
//#endif
#include "killwindow.h"
#include <qcursor.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>

namespace KWinInternal
{

KillWindow::KillWindow( Workspace* ws )
    : workspace( ws )
    {
    }

KillWindow::~KillWindow()
    {
    }

void KillWindow::start() 
    {
    static Cursor kill_cursor = 0;
    if (!kill_cursor)
        kill_cursor = XCreateFontCursor(qt_xdisplay(), XC_pirate);

    if (XGrabPointer(qt_xdisplay(), qt_xrootwin(), False,
                     ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask |
                     EnterWindowMask | LeaveWindowMask,
                     GrabModeAsync, GrabModeAsync, None,
                     kill_cursor, CurrentTime) == GrabSuccess) 
        {
        XGrabKeyboard(qt_xdisplay(), qt_xrootwin(), False,
                      GrabModeAsync, GrabModeAsync, CurrentTime);

        XEvent ev;
        int return_pressed  = 0;
        int escape_pressed  = 0;
        int button_released = 0;

        grabXServer();

        while (!return_pressed && !escape_pressed && !button_released) 
            {
            XMaskEvent(qt_xdisplay(), KeyPressMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask, &ev);

            if (ev.type == KeyPress)    
                {
                int kc = XKeycodeToKeysym(qt_xdisplay(), ev.xkey.keycode, 0);
                int mx = 0;
                int my = 0;
                return_pressed = (kc == XK_Return) || (kc == XK_space);
                escape_pressed = (kc == XK_Escape);
                if (kc == XK_Left)  mx = -10;
                if (kc == XK_Right) mx = 10;
                if (kc == XK_Up)    my = -10;
                if (kc == XK_Down)  my = 10;
                if (ev.xkey.state & ControlMask) 
                    {
                    mx /= 10;
                    my /= 10;
                    }
                QCursor::setPos(QCursor::pos()+QPoint(mx, my));
                }

            if (ev.type == ButtonRelease) 
                {
                button_released = (ev.xbutton.button == Button1);
                if ( ev.xbutton.button == Button3 ) 
                    {
                    escape_pressed = TRUE;
                    break;
                    }
                workspace->killWindowId(ev.xbutton.subwindow);
                }
            continue;
            }
        if (return_pressed) 
            {
            Window root, child;
            int dummy1, dummy2, dummy3, dummy4;
            unsigned int dummy5;
            if( XQueryPointer( qt_xdisplay(), qt_xrootwin(), &root, &child,
                &dummy1, &dummy2, &dummy3, &dummy4, &dummy5 ) == true
                && child != None )
                workspace->killWindowId( child );
            }

        ungrabXServer();

        XUngrabKeyboard(qt_xdisplay(), CurrentTime);
        XUngrabPointer(qt_xdisplay(), CurrentTime);
        }
    }

} // namespace
