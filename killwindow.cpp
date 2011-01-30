/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

//#ifndef QT_CLEAN_NAMESPACE
//#define QT_CLEAN_NAMESPACE
//#endif
#include "killwindow.h"
#include <QCursor>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>
#include <QX11Info>

namespace KWin
{

KillWindow::KillWindow(Workspace* ws)
    : workspace(ws)
{
}

KillWindow::~KillWindow()
{
}

void KillWindow::start()
{
    static Cursor kill_cursor = 0;
    if (!kill_cursor)
        kill_cursor = XCreateFontCursor(display(), XC_pirate);

    if (XGrabPointer(display(), rootWindow(), False,
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask |
                    EnterWindowMask | LeaveWindowMask,
                    GrabModeAsync, GrabModeAsync, None,
                    kill_cursor, CurrentTime) == GrabSuccess) {
        grabXKeyboard();

        XEvent ev;
        int return_pressed  = 0;
        int escape_pressed  = 0;
        int button_released = 0;

        grabXServer();

        while (!return_pressed && !escape_pressed && !button_released) {
            XMaskEvent(display(), KeyPressMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask, &ev);

            if (ev.type == KeyPress) {
                int kc = XKeycodeToKeysym(display(), ev.xkey.keycode, 0);
                int mx = 0;
                int my = 0;
                return_pressed = (kc == XK_Return) || (kc == XK_space);
                escape_pressed = (kc == XK_Escape);
                if (kc == XK_Left)  mx = -10;
                if (kc == XK_Right) mx = 10;
                if (kc == XK_Up)    my = -10;
                if (kc == XK_Down)  my = 10;
                if (ev.xkey.state & ControlMask) {
                    mx /= 10;
                    my /= 10;
                }
                QCursor::setPos(cursorPos() + QPoint(mx, my));
            }

            if (ev.type == ButtonRelease) {
                button_released = (ev.xbutton.button == Button1);
                if (ev.xbutton.button == Button3) {
                    escape_pressed = true;
                    break;
                }
                if (ev.xbutton.button == Button1 || ev.xbutton.button == Button2)
                    workspace->killWindowId(ev.xbutton.subwindow);
            }
            continue;
        }
        if (return_pressed) {
            Window root, child;
            int dummy1, dummy2, dummy3, dummy4;
            unsigned int dummy5;
            if (XQueryPointer(display(), rootWindow(), &root, &child,
                             &dummy1, &dummy2, &dummy3, &dummy4, &dummy5) == true
                    && child != None)
                workspace->killWindowId(child);
        }

        ungrabXServer();
        ungrabXKeyboard();
        XUngrabPointer(display(), CurrentTime);
    }
}

} // namespace
