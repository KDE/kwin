/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "unmanaged.h"

#include "workspace.h"
#include "effects.h"
#include "deleted.h"
#include "utils.h"
#include "xcbutils.h"

#include <QTimer>
#include <QDebug>
#include <QWindow>

#include <xcb/shape.h>

namespace KWin
{

// window types that are supported as unmanaged (mainly for compositing)
const NET::WindowTypes SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
        | NET::UtilityMask | NET::SplashMask | NET::DropdownMenuMask | NET::PopupMenuMask
        | NET::TooltipMask | NET::NotificationMask | NET::ComboBoxMask | NET::DNDIconMask | NET::OnScreenDisplayMask
        | NET::CriticalNotificationMask;

Unmanaged::Unmanaged()
    : Toplevel()
{
    QTimer::singleShot(50, this, SLOT(setReadyForPainting()));
}

Unmanaged::~Unmanaged()
{
}

bool Unmanaged::track(xcb_window_t w)
{
    XServerGrabber xserverGrabber;
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
    m_frameGeometry = geo.rect();
    m_clientGeometry = geo.rect();
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
    if (QWindow *internalWindow = findInternalWindow()) {
        m_outline = internalWindow->property("__kwin_outline").toBool();
    }
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking();
    return true;
}

void Unmanaged::release(ReleaseReason releaseReason)
{
    addWorkspaceRepaint(visibleRect());
    Deleted* del = nullptr;
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
    workspace()->removeUnmanaged(this);
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        disownDataPassedToDeleted();
        del->unrefWindow();
    }
    deleteUnmanaged(this);
}

void Unmanaged::deleteUnmanaged(Unmanaged* c)
{
    delete c;
}

bool Unmanaged::hasScheduledRelease() const
{
    return m_scheduledRelease;
}

QRect Unmanaged::bufferGeometry() const
{
    return m_frameGeometry;
}

int Unmanaged::desktop() const
{
    return NET::OnAllDesktops; // TODO for some window types should be the current desktop?
}

QStringList Unmanaged::activities() const
{
    return QStringList();
}

QVector<VirtualDesktop *> Unmanaged::desktops() const
{
    return QVector<VirtualDesktop *>();
}

QPoint Unmanaged::clientPos() const
{
    return QPoint(0, 0);   // unmanaged windows don't have decorations
}

QRect Unmanaged::transparentRect() const
{
    return QRect(clientPos(), clientSize());
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

bool Unmanaged::isOutline() const
{
    return m_outline;
}

void Unmanaged::addDamage(const QRegion &damage)
{
    repaints_region += damage;
    Toplevel::addDamage(damage);
}

QWindow *Unmanaged::findInternalWindow() const
{
    const QWindowList windows = kwinApp()->topLevelWindows();
    for (QWindow *w : windows) {
        if (w->winId() == window()) {
            return w;
        }
    }
    return nullptr;
}

bool Unmanaged::setupCompositing()
{
    if (!Toplevel::setupCompositing()) {
        return false;
    }

    // With unmanaged windows there is a race condition between the client painting the window
    // and us setting up damage tracking.  If the client wins we won't get a damage event even
    // though the window has been painted.  To avoid this we mark the whole window as damaged
    // and schedule a repaint immediately after creating the damage object.
    addDamageFull();

    return true;
}

} // namespace

