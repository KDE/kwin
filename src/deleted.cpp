/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deleted.h"

#include "group.h"
#include "netinfo.h"
#include "shadow.h"
#include "virtualdesktops.h"
#include "workspace.h"

#include <QDebug>

namespace KWin
{

Deleted::Deleted()
    : Window()
    , delete_refcount(1)
    , m_frame(XCB_WINDOW_NONE)
    , m_layer(UnknownLayer)
    , m_shade(false)
    , m_minimized(false)
    , m_modal(false)
    , m_wasClient(false)
    , m_fullscreen(false)
    , m_keepAbove(false)
    , m_keepBelow(false)
    , m_wasPopupWindow(false)
    , m_wasOutline(false)
    , m_wasLockScreen(false)
{
}

Deleted::~Deleted()
{
    if (delete_refcount != 0) {
        qCCritical(KWIN_CORE) << "Deleted client has non-zero reference count (" << delete_refcount << ")";
    }
    Q_ASSERT(delete_refcount == 0);
    if (workspace()) {
        workspace()->removeDeleted(this);
    }
}

std::unique_ptr<WindowItem> Deleted::createItem(Scene *scene)
{
    Q_UNREACHABLE();
}

Deleted *Deleted::create(Window *c)
{
    Deleted *d = new Deleted();
    d->copyToDeleted(c);
    workspace()->addDeleted(d, c);
    return d;
}

// to be used only from Workspace::finishCompositing()
void Deleted::discard()
{
    delete_refcount = 0;
    delete this;
}

void Deleted::copyToDeleted(Window *window)
{
    Q_ASSERT(!window->isDeleted());
    Window::copyToDeleted(window);
    m_frameMargins = window->frameMargins();
    desk = window->desktop();
    m_desktops = window->desktops();
    activityList = window->activities();
    contentsRect = QRectF(window->clientPos(), window->clientSize());
    m_layer = window->layer();
    m_frame = window->frameId();
    m_type = window->windowType();
    m_windowRole = window->windowRole();
    m_shade = window->isShade();
    if (WinInfo *cinfo = dynamic_cast<WinInfo *>(info)) {
        cinfo->disable();
    }
    if (window->isDecorated()) {
        window->layoutDecorationRects(decoration_left,
                                      decoration_top,
                                      decoration_right,
                                      decoration_bottom);
    }
    m_wasClient = true;
    m_minimized = window->isMinimized();
    m_modal = window->isModal();
    m_mainWindows = window->mainWindows();
    for (Window *w : std::as_const(m_mainWindows)) {
        connect(w, &Window::windowClosed, this, &Deleted::mainWindowClosed);
    }
    m_fullscreen = window->isFullScreen();
    m_keepAbove = window->keepAbove();
    m_keepBelow = window->keepBelow();
    m_caption = window->caption();

    for (auto vd : std::as_const(m_desktops)) {
        connect(vd, &QObject::destroyed, this, [=, this] {
            m_desktops.removeOne(vd);
        });
    }

    m_wasPopupWindow = window->isPopupWindow();
    m_wasOutline = window->isOutline();
    m_wasLockScreen = window->isLockScreen();
}

void Deleted::unrefWindow()
{
    if (--delete_refcount > 0) {
        return;
    }
    // needs to be delayed
    // a) when calling from effects, otherwise it'd be rather complicated to handle the case of the
    // window going away during a painting pass
    // b) to prevent dangeling pointers in the stacking order, see bug #317765
    deleteLater();
}

QMargins Deleted::frameMargins() const
{
    return m_frameMargins;
}

int Deleted::desktop() const
{
    return desk;
}

QStringList Deleted::activities() const
{
    return activityList;
}

QVector<VirtualDesktop *> Deleted::desktops() const
{
    return m_desktops;
}

QPointF Deleted::clientPos() const
{
    return contentsRect.topLeft();
}

void Deleted::layoutDecorationRects(QRectF &left, QRectF &top, QRectF &right, QRectF &bottom) const
{
    left = decoration_left;
    top = decoration_top;
    right = decoration_right;
    bottom = decoration_bottom;
}

bool Deleted::isDeleted() const
{
    return true;
}

NET::WindowType Deleted::windowType(bool direct, int supportedTypes) const
{
    return m_type;
}

void Deleted::mainWindowClosed(Window *window)
{
    m_mainWindows.removeAll(window);
}

xcb_window_t Deleted::frameId() const
{
    return m_frame;
}

QString Deleted::windowRole() const
{
    return m_windowRole;
}

QVector<uint> Deleted::x11DesktopIds() const
{
    const auto desks = desktops();
    QVector<uint> x11Ids;
    x11Ids.reserve(desks.count());
    std::transform(desks.constBegin(), desks.constEnd(),
                   std::back_inserter(x11Ids),
                   [](const VirtualDesktop *vd) {
                       return vd->x11DesktopNumber();
                   });
    return x11Ids;
}

} // namespace
