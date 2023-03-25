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
    , m_frame(XCB_WINDOW_NONE)
    , m_layer(UnknownLayer)
    , m_shade(false)
    , m_fullscreen(false)
    , m_wasPopupWindow(false)
    , m_wasOutline(false)
    , m_wasLockScreen(false)
{
}

Deleted::~Deleted()
{
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

void Deleted::copyToDeleted(Window *window)
{
    Q_ASSERT(!window->isDeleted());
    Window::copyToDeleted(window);
    desk = window->desktop();
    m_desktops = window->desktops();
    activityList = window->activities();
    contentsRect = QRectF(window->clientPos(), window->clientSize());
    m_layer = window->layer();
    m_frame = window->frameId();
    m_type = window->windowType();
    m_windowRole = window->windowRole();
    m_shade = window->isShade();
    m_mainWindows = window->mainWindows();
    for (Window *w : std::as_const(m_mainWindows)) {
        connect(w, &Window::closed, this, &Deleted::mainWindowClosed);
    }
    m_fullscreen = window->isFullScreen();
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

} // namespace
