/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "unmanaged.h"

#include "workspace.h"
#include "effects.h"
#include "deleted.h"
#include "xcbutils.h"

#include <QTimer>
#include <QDebug>

#include <xcb/shape.h>
#include <xcb/xtest.h>

namespace KWin
{

Unmanaged::Unmanaged()
    : Toplevel()
{
    ready_for_painting = false;
    connect(this, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SIGNAL(geometryChanged()));
    QTimer::singleShot(50, this, SLOT(setReadyForPainting()));
}

Unmanaged::~Unmanaged()
{
}

bool Unmanaged::track(Window w)
{
    XWindowAttributes attr;
    grabXServer();
    if (!XGetWindowAttributes(display(), w, &attr) || attr.map_state != IsViewable) {
        ungrabXServer();
        return false;
    }
    if (attr.c_class == InputOnly) {
        ungrabXServer();
        return false;
    }
    setWindowHandles(w);   // the window is also the frame
    Xcb::selectInput(w, attr.your_event_mask | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE);
    geom = QRect(attr.x, attr.y, attr.width, attr.height);
    checkScreen();
    vis = attr.visual;
    bit_depth = attr.depth;
    info = new NETWinInfo(connection(), w, rootWindow(),
                          NET::WMWindowType | NET::WMPid, NET::WM2Opacity);
    getResourceClass();
    getWindowRole();
    getWmClientLeader();
    getWmClientMachine();
    if (Xcb::Extensions::self()->isShapeAvailable())
        xcb_shape_select_input(connection(), w, true);
    detectShape(w);
    getWmOpaqueRegion();
    getSkipCloseAnimation();
    setupCompositing();
    ungrabXServer();
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking();
    return true;
}

void Unmanaged::release(bool on_shutdown)
{
    Deleted* del = NULL;
    if (!on_shutdown) {
        del = Deleted::create(this);
    }
    emit windowClosed(this, del);
    finishCompositing();
    if (!QWidget::find(window())) { // don't affect our own windows
        if (Xcb::Extensions::self()->isShapeAvailable())
            xcb_shape_select_input(connection(), window(), false);
        Xcb::selectInput(window(), XCB_EVENT_MASK_NO_EVENT);
    }
    if (!on_shutdown) {
        workspace()->removeUnmanaged(this);
        addWorkspaceRepaint(del->visibleRect());
        disownDataPassedToDeleted();
        del->unrefWindow();
    }
    deleteUnmanaged(this);
}

void Unmanaged::deleteUnmanaged(Unmanaged* c)
{
    delete c;
}

int Unmanaged::desktop() const
{
    return NET::OnAllDesktops; // TODO for some window types should be the current desktop?
}

QStringList Unmanaged::activities() const
{
    return QStringList();
}

QPoint Unmanaged::clientPos() const
{
    return QPoint(0, 0);   // unmanaged windows don't have decorations
}

QSize Unmanaged::clientSize() const
{
    return size();
}

QRect Unmanaged::transparentRect() const
{
    return QRect(clientPos(), clientSize());
}

void Unmanaged::debug(QDebug& stream) const
{
    stream << "\'ID:" << window() << "\'";
}

NET::WindowType Unmanaged::windowType(bool direct, int supportedTypes) const
{
    // for unmanaged windows the direct does not make any difference
    // as there are no rules to check and no hacks to apply
    Q_UNUSED(direct)
    if (supportedTypes == 0) {
        supportedTypes = SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK;
    }
    return info->windowType(NET::WindowTypes(supportedTypes));
}

void Unmanaged::sendPointerButtonEvent(uint32_t button, InputRedirection::PointerButtonState state)
{
    // TODO: don't use xtest
    uint8_t type = XCB_BUTTON_PRESS;
    if (state == KWin::InputRedirection::PointerButtonReleased) {
        type = XCB_BUTTON_RELEASE;
    }
    xcb_test_fake_input(connection(), type, InputRedirection::toXPointerButton(button), XCB_TIME_CURRENT_TIME, window(), 0, 0, 0);
}

void Unmanaged::sendPointerAxisEvent(InputRedirection::PointerAxis axis, qreal delta)
{
    // TODO: don't use xtest
    const int val = qRound(delta);
    if (val == 0) {
        return;
    }
    const uint8_t button = InputRedirection::toXPointerButton(axis, delta);
    for (int i = 0; i < qAbs(val); ++i) {
        xcb_test_fake_input(connection(), XCB_BUTTON_PRESS, button, XCB_TIME_CURRENT_TIME, window(), 0, 0, 0);
        xcb_test_fake_input(connection(), XCB_BUTTON_RELEASE, button, XCB_TIME_CURRENT_TIME, window(), 0, 0, 0);
    }
}

} // namespace

#include "unmanaged.moc"
