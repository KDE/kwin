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
#include "utils.h"
#include "xcbutils.h"

#include <QTimer>
#include <QDebug>

#include <xcb/shape.h>

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
    GRAB_SERVER_DURING_CONTEXT
    Xcb::WindowAttributes attr(w);
    Xcb::WindowGeometry geo(w);
    if (attr.isNull() || attr->map_state != XCB_MAP_STATE_VIEWABLE) {
        return false;
    }
    if (attr->_class == XCB_WINDOW_CLASS_INPUT_ONLY) {
        return false;
    }
    if (geo.isNull()) {
        return false;
    }
    setWindowHandles(w);   // the window is also the frame
    Xcb::selectInput(w, attr->your_event_mask | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE);
    geom = geo.rect();
    checkScreen();
    m_visual = attr->visual;
    bit_depth = geo->depth;
    info = new NETWinInfo(connection(), w, rootWindow(),
                          NET::WMWindowType | NET::WMPid,
                          NET::WM2Opacity |
                          NET::WM2WindowRole |
                          NET::WM2WindowClass |
                          NET::WM2OpaqueRegion);
    getResourceClass();
    getWmClientLeader();
    getWmClientMachine();
    if (Xcb::Extensions::self()->isShapeAvailable())
        xcb_shape_select_input(connection(), w, true);
    detectShape(w);
    getWmOpaqueRegion();
    getSkipCloseAnimation();
    setupCompositing();
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking();
    return true;
}

void Unmanaged::release(ReleaseReason releaseReason)
{
    Deleted* del = NULL;
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        del = Deleted::create(this);
    }
    emit windowClosed(this, del);
    finishCompositing(releaseReason);
    if (!QWidget::find(window()) && releaseReason != ReleaseReason::Destroyed) { // don't affect our own windows
        if (Xcb::Extensions::self()->isShapeAvailable())
            xcb_shape_select_input(connection(), window(), false);
        Xcb::selectInput(window(), XCB_EVENT_MASK_NO_EVENT);
    }
    if (releaseReason != ReleaseReason::KWinShutsDown) {
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

void Unmanaged::addDamage(const QRegion &damage)
{
    repaints_region += damage;
    Toplevel::addDamage(damage);
}

} // namespace

