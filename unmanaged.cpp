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

#include <X11/extensions/shape.h>

namespace KWin
{

Unmanaged::Unmanaged(Workspace* ws)
    : Toplevel(ws)
{
    connect(this, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SIGNAL(geometryChanged()));
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
    setWindowHandles(w, w);   // the window is also the frame
    XSelectInput(display(), w, attr.your_event_mask | StructureNotifyMask | PropertyChangeMask);
    geom = QRect(attr.x, attr.y, attr.width, attr.height);
    vis = attr.visual;
    bit_depth = attr.depth;
    unsigned long properties[ 2 ];
    properties[ NETWinInfo::PROTOCOLS ] =
        NET::WMWindowType |
        NET::WMPid |
        0;
    properties[ NETWinInfo::PROTOCOLS2 ] =
        NET::WM2Opacity |
        0;
    info = new NETWinInfo2(display(), w, rootWindow(), properties, 2);
    getResourceClass();
    getWindowRole();
    getWmClientLeader();
    getWmClientMachine();
    if (Extensions::shapeAvailable())
        XShapeSelectInput(display(), w, ShapeNotifyMask);
    detectShape(w);
    getWmOpaqueRegion();
    setupCompositing();
    ungrabXServer();
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking();
    return true;
}

void Unmanaged::release()
{
    Deleted* del = Deleted::create(this);
    emit windowClosed(this, del);
    finishCompositing();
    workspace()->removeUnmanaged(this, Allowed);
    if (!QWidget::find(window())) { // don't affect our own windows
        if (Extensions::shapeAvailable())
            XShapeSelectInput(display(), window(), NoEventMask);
        XSelectInput(display(), window(), NoEventMask);
    }
    addWorkspaceRepaint(del->visibleRect());
    disownDataPassedToDeleted();
    del->unrefWindow();
    deleteUnmanaged(this, Allowed);
}

void Unmanaged::deleteUnmanaged(Unmanaged* c, allowed_t)
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

} // namespace

#include "unmanaged.moc"
