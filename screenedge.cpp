/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

Since the functionality provided in this class has been moved from
class Workspace, it is not clear who exactly has written the code.
The list below contains the copyright holders of the class Workspace.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "screenedge.h"

// KWin
#include "atoms.h"
#include "effects.h"
#include "options.h"
#include "utils.h"
#include "workspace.h"

// Qt
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusInterface>

// KDE-Workspace
#include <kephal/screens.h>

namespace KWin {

ScreenEdge::ScreenEdge()
    : QObject(NULL)
{
    for (int i = 0; i < ELECTRIC_COUNT; ++i) {
        electric_reserved[i] = 0;
        electric_windows[i] = None;
    }
}

ScreenEdge::~ScreenEdge()
{
}

void ScreenEdge::updateElectricBorders()
{
    electric_time_first = xTime();
    electric_time_last = xTime();
    electric_time_last_trigger = xTime();
    electric_current_border = ElectricNone;
    QRect r = Kephal::ScreenUtils::desktopGeometry();
    electricTop = r.top();
    electricBottom = r.bottom();
    electricLeft = r.left();
    electricRight = r.right();

    for (int pos = 0; pos < ELECTRIC_COUNT; ++pos) {
        if (electric_reserved[pos] == 0) {
            if (electric_windows[pos] != None)
                XDestroyWindow(display(), electric_windows[pos]);
            electric_windows[pos] = None;
            continue;
        }
        if (electric_windows[pos] != None)
            continue;
        XSetWindowAttributes attributes;
        attributes.override_redirect = True;
        attributes.event_mask = EnterWindowMask | LeaveWindowMask;
        unsigned long valuemask = CWOverrideRedirect | CWEventMask;
        int xywh[ELECTRIC_COUNT][4] = {
            { r.left() + 1, r.top(), r.width() - 2, 1 },   // Top
            { r.right(), r.top(), 1, 1 },                  // Top-right
            { r.right(), r.top() + 1, 1, r.height() - 2 }, // Etc.
            { r.right(), r.bottom(), 1, 1 },
            { r.left() + 1, r.bottom(), r.width() - 2, 1 },
            { r.left(), r.bottom(), 1, 1 },
            { r.left(), r.top() + 1, 1, r.height() - 2 },
            { r.left(), r.top(), 1, 1 }
        };
        electric_windows[pos] = XCreateWindow(display(), rootWindow(),
                                              xywh[pos][0], xywh[pos][1], xywh[pos][2], xywh[pos][3],
                                              0, CopyFromParent, InputOnly, CopyFromParent, valuemask, &attributes);
        XMapWindow(display(), electric_windows[pos]);

        // Set XdndAware on the windows, so that DND enter events are received (#86998)
        Atom version = 4; // XDND version
        XChangeProperty(display(), electric_windows[pos], atoms->xdnd_aware, XA_ATOM,
                        32, PropModeReplace, (unsigned char*)(&version), 1);
    }
}

void ScreenEdge::destroyElectricBorders()
{
    for (int pos = 0; pos < ELECTRIC_COUNT; ++pos) {
        if (electric_windows[pos] != None)
            XDestroyWindow(display(), electric_windows[pos]);
        electric_windows[pos] = None;
    }
}

void ScreenEdge::restoreElectricBorderSize(ElectricBorder border)
{
    if (electric_windows[border] == None)
        return;
    QRect r = Kephal::ScreenUtils::desktopGeometry();
    int xywh[ELECTRIC_COUNT][4] = {
        { r.left() + 1, r.top(), r.width() - 2, 1 },   // Top
        { r.right(), r.top(), 1, 1 },                  // Top-right
        { r.right(), r.top() + 1, 1, r.height() - 2 }, // Etc.
        { r.right(), r.bottom(), 1, 1 },
        { r.left() + 1, r.bottom(), r.width() - 2, 1 },
        { r.left(), r.bottom(), 1, 1 },
        { r.left(), r.top() + 1, 1, r.height() - 2 },
        { r.left(), r.top(), 1, 1 }
    };
    XMoveResizeWindow(display(), electric_windows[border],
                      xywh[border][0], xywh[border][1], xywh[border][2], xywh[border][3]);
}

void ScreenEdge::reserveElectricBorderActions(bool reserve)
{
    for (int pos = 0; pos < ELECTRIC_COUNT; ++pos)
        if (options->electricBorderAction(static_cast<ElectricBorder>(pos))) {
            if (reserve)
                reserveElectricBorder(static_cast<ElectricBorder>(pos));
            else
                unreserveElectricBorder(static_cast<ElectricBorder>(pos));
        }
}

void ScreenEdge::reserveElectricBorderSwitching(bool reserve)
{
    for (int pos = 0; pos < ELECTRIC_COUNT; ++pos)
        if (reserve)
            reserveElectricBorder(static_cast<ElectricBorder>(pos));
        else
            unreserveElectricBorder(static_cast<ElectricBorder>(pos));
}

void ScreenEdge::reserveElectricBorder(ElectricBorder border)
{
    if (border == ElectricNone)
        return;
    if (electric_reserved[border]++ == 0)
        QTimer::singleShot(0, this, SLOT(updateElectricBorders()));
}

void ScreenEdge::unreserveElectricBorder(ElectricBorder border)
{
    if (border == ElectricNone)
        return;
    assert(electric_reserved[border] > 0);
    if (--electric_reserved[border] == 0)
        QTimer::singleShot(0, this, SLOT(updateElectricBorders()));
}

void ScreenEdge::checkElectricBorder(const QPoint& pos, Time now)
{
    if ((pos.x() != electricLeft) &&
            (pos.x() != electricRight) &&
            (pos.y() != electricTop) &&
            (pos.y() != electricBottom))
        return;

    bool have_borders = false;
    for (int i = 0; i < ELECTRIC_COUNT; ++i)
        if (electric_windows[i] != None)
            have_borders = true;
    if (!have_borders)
        return;

    Time treshold_set = options->electricBorderDelay(); // Set timeout
    Time treshold_reset = 250; // Reset timeout
    Time treshold_trigger = options->electricBorderCooldown(); // Minimum time between triggers
    int distance_reset = 30; // Mouse should not move more than this many pixels
    int pushback_pixels = options->electricBorderPushbackPixels();

    ElectricBorder border;
    if (pos.x() == electricLeft && pos.y() == electricTop)
        border = ElectricTopLeft;
    else if (pos.x() == electricRight && pos.y() == electricTop)
        border = ElectricTopRight;
    else if (pos.x() == electricLeft && pos.y() == electricBottom)
        border = ElectricBottomLeft;
    else if (pos.x() == electricRight && pos.y() == electricBottom)
        border = ElectricBottomRight;
    else if (pos.x() == electricLeft)
        border = ElectricLeft;
    else if (pos.x() == electricRight)
        border = ElectricRight;
    else if (pos.y() == electricTop)
        border = ElectricTop;
    else if (pos.y() == electricBottom)
        border = ElectricBottom;
    else
        abort();

    if (electric_windows[border] == None)
        return;

    if (pushback_pixels == 0) {
        // no pushback so we have to activate at once
        electric_time_last = now;
    }
    if ((electric_current_border == border) &&
            (timestampDiff(electric_time_last, now) < treshold_reset) &&
            (timestampDiff(electric_time_last_trigger, now) > treshold_trigger) &&
            ((pos - electric_push_point).manhattanLength() < distance_reset)) {
        electric_time_last = now;

        if (timestampDiff(electric_time_first, now) > treshold_set) {
            electric_current_border = ElectricNone;
            electric_time_last_trigger = now;
            if (Workspace::self()->getMovingClient()) {
                // If moving a client or have force doing the desktop switch
                if (options->electricBorders() != Options::ElectricDisabled)
                    electricBorderSwitchDesktop(border, pos);
                return; // Don't reset cursor position
            } else {
                if (options->electricBorders() == Options::ElectricAlways &&
                        (border == ElectricTop || border == ElectricRight ||
                         border == ElectricBottom || border == ElectricLeft)) {
                    // If desktop switching is always enabled don't apply it to the corners if
                    // an effect is applied to it (We will check that later).
                    electricBorderSwitchDesktop(border, pos);
                    return; // Don't reset cursor position
                }
                switch(options->electricBorderAction(border)) {
                case ElectricActionDashboard: { // Display Plasma dashboard
                    QDBusInterface plasmaApp("org.kde.plasma-desktop", "/App");
                    plasmaApp.call("toggleDashboard");
                }
                break;
                case ElectricActionShowDesktop: {
                    Workspace::self()->setShowingDesktop(!Workspace::self()->showingDesktop());
                    break;
                }
                case ElectricActionLockScreen: { // Lock the screen
                    QDBusInterface screenSaver("org.kde.screensaver", "/ScreenSaver");
                    screenSaver.call("Lock");
                }
                break;
                case ElectricActionPreventScreenLocking: {
                    break;
                }
                case ElectricActionNone: // Either desktop switching or an effect
                default: {
                    if (effects && static_cast<EffectsHandlerImpl*>(effects)->borderActivated(border))
                        {} // Handled by effects
                    else {
                        electricBorderSwitchDesktop(border, pos);
                        return; // Don't reset cursor position
                    }
                }
                }
            }
        }
    } else {
        electric_current_border = border;
        electric_time_first = now;
        electric_time_last = now;
        electric_push_point = pos;
    }

    // Reset the pointer to find out whether the user is really pushing
    // (the direction back from which it came, starting from top clockwise)
    const int xdiff[ELECTRIC_COUNT] = { 0,
                                        -pushback_pixels,
                                        -pushback_pixels,
                                        -pushback_pixels,
                                        0,
                                        pushback_pixels,
                                        pushback_pixels,
                                        pushback_pixels
                                      };
    const int ydiff[ELECTRIC_COUNT] = { pushback_pixels,
                                        pushback_pixels,
                                        0,
                                        -pushback_pixels,
                                        -pushback_pixels,
                                        -pushback_pixels,
                                        0,
                                        pushback_pixels
                                      };
    QCursor::setPos(pos.x() + xdiff[border], pos.y() + ydiff[border]);
}

void ScreenEdge::electricBorderSwitchDesktop(ElectricBorder border, const QPoint& _pos)
{
    QPoint pos = _pos;
    int desk = Workspace::self()->currentDesktop();
    const int OFFSET = 2;
    if (border == ElectricLeft || border == ElectricTopLeft || border == ElectricBottomLeft) {
        desk = Workspace::self()->desktopToLeft(desk, options->rollOverDesktops);
        pos.setX(displayWidth() - 1 - OFFSET);
    }
    if (border == ElectricRight || border == ElectricTopRight || border == ElectricBottomRight) {
        desk = Workspace::self()->desktopToRight(desk, options->rollOverDesktops);
        pos.setX(OFFSET);
    }
    if (border == ElectricTop || border == ElectricTopLeft || border == ElectricTopRight) {
        desk = Workspace::self()->desktopAbove(desk, options->rollOverDesktops);
        pos.setY(displayHeight() - 1 - OFFSET);
    }
    if (border == ElectricBottom || border == ElectricBottomLeft || border == ElectricBottomRight) {
        desk = Workspace::self()->desktopBelow(desk, options->rollOverDesktops);
        pos.setY(OFFSET);
    }
    int desk_before = Workspace::self()->currentDesktop();
    Workspace::self()->setCurrentDesktop(desk);
    if (Workspace::self()->currentDesktop() != desk_before)
        QCursor::setPos(pos);
}

/**
 * Called when the user entered an electric border with the mouse.
 * It may switch to another virtual desktop.
 */
bool ScreenEdge::electricBorderEvent(XEvent* e)
{
    if (e->type == EnterNotify) {
        for (int i = 0; i < ELECTRIC_COUNT; ++i)
            if (electric_windows[i] != None && e->xcrossing.window == electric_windows[i]) {
                // The user entered an electric border
                checkElectricBorder(QPoint(e->xcrossing.x_root, e->xcrossing.y_root), e->xcrossing.time);
                return true;
            }
    }
    if (e->type == ClientMessage) {
        if (e->xclient.message_type == atoms->xdnd_position) {
            for (int i = 0; i < ELECTRIC_COUNT; ++i)
                if (electric_windows[i] != None && e->xclient.window == electric_windows[i]) {
                    updateXTime();
                    checkElectricBorder(QPoint(e->xclient.data.l[2] >> 16, e->xclient.data.l[2] & 0xffff), xTime());
                    return true;
                }
        }
    }
    return false;
}

/**
 * Raise electric border windows to the real top of the screen. We only need
 * to do this if an effect input window is active.
 */
void ScreenEdge::raiseElectricBorderWindows()
{
    Window* windows = new Window[ 8 ]; // There are up to 8 borders
    int pos = 0;
    for (int i = 0; i < ELECTRIC_COUNT; ++i)
        if (electric_windows[ i ] != None)
            windows[ pos++ ] = electric_windows[ i ];
    if (!pos) {
        delete [] windows;
        return; // No borders at all
    }
    XRaiseWindow(display(), windows[ 0 ]);
    XRestackWindows(display(), windows, pos);
    delete [] windows;
}
} //namespace
