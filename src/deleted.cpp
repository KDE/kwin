/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deleted.h"

#include "workspace.h"
#include "abstract_client.h"
#include "group.h"
#include "netinfo.h"
#include "shadow.h"
#include "virtualdesktops.h"

#include <QDebug>

namespace KWin
{

Deleted::Deleted()
    : Toplevel()
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
    if (delete_refcount != 0)
        qCCritical(KWIN_CORE) << "Deleted client has non-zero reference count (" << delete_refcount << ")";
    Q_ASSERT(delete_refcount == 0);
    if (workspace()) {
        workspace()->removeDeleted(this);
    }
    deleteEffectWindow();
    deleteShadow();
}

Deleted* Deleted::create(Toplevel* c)
{
    Deleted* d = new Deleted();
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

void Deleted::copyToDeleted(Toplevel* c)
{
    Q_ASSERT(dynamic_cast< Deleted* >(c) == nullptr);
    Toplevel::copyToDeleted(c);
    m_frameMargins = c->frameMargins();
    desk = c->desktop();
    m_desktops = c->desktops();
    activityList = c->activities();
    contentsRect = QRect(c->clientPos(), c->clientSize());
    m_layer = c->layer();
    m_frame = c->frameId();
    m_type = c->windowType();
    m_windowRole = c->windowRole();
    m_shade = c->isShade();
    if (WinInfo* cinfo = dynamic_cast< WinInfo* >(info))
        cinfo->disable();
    if (AbstractClient *client = dynamic_cast<AbstractClient*>(c)) {
        if (client->isDecorated()) {
            client->layoutDecorationRects(decoration_left,
                                          decoration_top,
                                          decoration_right,
                                          decoration_bottom);
        }
        m_wasClient = true;
        m_minimized = client->isMinimized();
        m_modal = client->isModal();
        m_mainClients = client->mainClients();
        for (AbstractClient *c : qAsConst(m_mainClients)) {
            connect(c, &AbstractClient::windowClosed, this, &Deleted::mainClientClosed);
        }
        m_fullscreen = client->isFullScreen();
        m_keepAbove = client->keepAbove();
        m_keepBelow = client->keepBelow();
        m_caption = client->caption();
    }

    for (auto vd : qAsConst(m_desktops)) {
        connect(vd, &QObject::destroyed, this, [=] {
            m_desktops.removeOne(vd);
        });
    }

    m_wasPopupWindow = c->isPopupWindow();
    m_wasOutline = c->isOutline();
    m_wasLockScreen = c->isLockScreen();
}

void Deleted::unrefWindow()
{
    if (--delete_refcount > 0)
        return;
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

QPoint Deleted::clientPos() const
{
    return contentsRect.topLeft();
}

void Deleted::layoutDecorationRects(QRect& left, QRect& top, QRect& right, QRect& bottom) const
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
    Q_UNUSED(direct)
    Q_UNUSED(supportedTypes)
    return m_type;
}

void Deleted::mainClientClosed(Toplevel *client)
{
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(client))
        m_mainClients.removeAll(c);
}

xcb_window_t Deleted::frameId() const
{
    return m_frame;
}

QByteArray Deleted::windowRole() const
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
        [] (const VirtualDesktop *vd) {
            return vd->x11DesktopNumber();
        }
    );
    return x11Ids;
}

} // namespace

