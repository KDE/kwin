/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "unmanaged.h"

#include "deleted.h"
#include "effects.h"
#include "scene/surfaceitem_x11.h"
#include "scene/windowitem.h"
#include "utils/common.h"
#include "wayland/surface_interface.h"
#include "workspace.h"

#include <QDebug>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <xcb/shape.h>

using namespace KWaylandServer;

namespace KWin
{

// window types that are supported as unmanaged (mainly for compositing)
const NET::WindowTypes SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK = NET::NormalMask
    | NET::DesktopMask
    | NET::DockMask
    | NET::ToolbarMask
    | NET::MenuMask
    | NET::DialogMask
    /*| NET::OverrideMask*/
    | NET::TopMenuMask
    | NET::UtilityMask
    | NET::SplashMask
    | NET::DropdownMenuMask
    | NET::PopupMenuMask
    | NET::TooltipMask
    | NET::NotificationMask
    | NET::ComboBoxMask
    | NET::DNDIconMask
    | NET::OnScreenDisplayMask
    | NET::CriticalNotificationMask;

Unmanaged::Unmanaged()
    : Window()
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeXwayland:
        // The wayland surface is associated with the override-redirect window asynchronously.
        connect(this, &Window::surfaceChanged, this, &Unmanaged::associate);
        break;
    case Application::OperationModeX11:
        // We have no way knowing whether the override-redirect window can be painted. Mark it
        // as ready for painting after synthetic 50ms delay.
        QTimer::singleShot(50, this, &Unmanaged::initialize);
        break;
    case Application::OperationModeWaylandOnly:
        Q_UNREACHABLE();
    }
}

Unmanaged::~Unmanaged()
{
}

std::unique_ptr<WindowItem> Unmanaged::createItem(Scene *scene)
{
    return std::make_unique<WindowItemX11>(this, scene);
}

void Unmanaged::associate()
{
    if (surface()->isMapped()) {
        initialize();
    } else {
        // Queued connection because we want to mark the window ready for painting after
        // the associated surface item has processed the new surface state.
        connect(surface(), &SurfaceInterface::mapped, this, &Unmanaged::initialize, Qt::QueuedConnection);
    }
}

void Unmanaged::initialize()
{
    setReadyForPainting();
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

    setWindowHandles(w); // the window is also the frame
    Xcb::selectInput(w, attr->your_event_mask | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE);
    m_bufferGeometry = geo.rect();
    m_frameGeometry = geo.rect();
    m_clientGeometry = geo.rect();
    checkOutput();
    m_visual = attr->visual;
    bit_depth = geo->depth;
    info = new NETWinInfo(kwinApp()->x11Connection(), w, kwinApp()->x11RootWindow(),
                          NET::WMWindowType | NET::WMPid,
                          NET::WM2Opacity | NET::WM2WindowRole | NET::WM2WindowClass | NET::WM2OpaqueRegion);
    setOpacity(info->opacityF());
    getResourceClass();
    getWmClientLeader();
    getWmClientMachine();
    if (Xcb::Extensions::self()->isShapeAvailable()) {
        xcb_shape_select_input(kwinApp()->x11Connection(), w, true);
    }
    detectShape(w);
    getWmOpaqueRegion();
    getSkipCloseAnimation();
    setupCompositing();
    if (QWindow *internalWindow = findInternalWindow()) {
        m_outline = internalWindow->property("__kwin_outline").toBool();
    }
    if (effects) {
        static_cast<EffectsHandlerImpl *>(effects)->checkInputWindowStacking();
    }
    return true;
}

void Unmanaged::release(ReleaseReason releaseReason)
{
    Deleted *del = nullptr;
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        del = Deleted::create(this);
    }
    Q_EMIT windowClosed(this, del);
    finishCompositing(releaseReason);
    if (!QWidget::find(window()) && releaseReason != ReleaseReason::Destroyed) { // don't affect our own windows
        if (Xcb::Extensions::self()->isShapeAvailable()) {
            xcb_shape_select_input(kwinApp()->x11Connection(), window(), false);
        }
        Xcb::selectInput(window(), XCB_EVENT_MASK_NO_EVENT);
    }
    workspace()->removeUnmanaged(this);
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        disownDataPassedToDeleted();
        del->unrefWindow();
    }
    deleteUnmanaged(this);
}

void Unmanaged::deleteUnmanaged(Unmanaged *c)
{
    delete c;
}

bool Unmanaged::hasScheduledRelease() const
{
    return m_scheduledRelease;
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

QPointF Unmanaged::clientPos() const
{
    return QPoint(0, 0); // unmanaged windows don't have decorations
}

NET::WindowType Unmanaged::windowType(bool direct, int supportedTypes) const
{
    // for unmanaged windows the direct does not make any difference
    // as there are no rules to check and no hacks to apply
    if (supportedTypes == 0) {
        supportedTypes = SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK;
    }
    return info->windowType(NET::WindowTypes(supportedTypes));
}

bool Unmanaged::isOutline() const
{
    return m_outline;
}

bool Unmanaged::isUnmanaged() const
{
    return true;
}

QWindow *Unmanaged::findInternalWindow() const
{
    const QWindowList windows = kwinApp()->topLevelWindows();
    for (QWindow *w : windows) {
        if (w->handle() && w->winId() == window()) {
            return w;
        }
    }
    return nullptr;
}

void Unmanaged::checkOutput()
{
    setOutput(workspace()->outputAt(frameGeometry().center()));
}

void Unmanaged::damageNotifyEvent()
{
    Q_ASSERT(kwinApp()->operationMode() == Application::OperationModeX11);
    SurfaceItemX11 *item = static_cast<SurfaceItemX11 *>(surfaceItem());
    if (item) {
        item->processDamage();
    }
}

} // namespace
