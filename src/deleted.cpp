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
#include "decorations/decoratedclient.h"
#include "decorations/decorationrenderer.h"

#include <QDebug>

namespace KWin
{

Deleted::Deleted()
    : AbstractClient()
    , delete_refcount(1)
    , m_frame(XCB_WINDOW_NONE)
    , m_wasClient(false)
    , m_decorationRenderer(nullptr)
    , m_fullscreen(false)
    , m_wasPopupWindow(false)
    , m_wasOutline(false)
    , m_wasDecorated(false)
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
    m_bufferScale = c->bufferScale();
    desk = c->desktop();
    m_desktops = c->desktops();
    activityList = c->activities();
    contentsRect = QRect(c->clientPos(), c->clientSize());
    transparent_rect = c->transparentRect();
    m_layer = c->layer();
    m_frame = c->frameId();
    m_type = c->windowType();
    m_windowRole = c->windowRole();
    if (WinInfo* cinfo = dynamic_cast< WinInfo* >(info))
        cinfo->disable();
    m_wasDecorated = c->isDecorated();
    if (m_wasDecorated) {
        c->layoutDecorationRects(decoration_left,
                                 decoration_top,
                                 decoration_right,
                                 decoration_bottom);
        if (c->isDecorated()) {
            if (Decoration::Renderer *renderer = c->decoratedClient()->renderer()) {
                m_decorationRenderer = renderer;
                m_decorationRenderer->reparent(this);
            }
        }
    }
    m_wasClient = true;
    m_mainClients = c->mainClients();
    foreach (AbstractClient *c, m_mainClients) {
        connect(c, &AbstractClient::windowClosed, this, &Deleted::mainClientClosed);
    }
    m_fullscreen = c->isFullScreen();
    m_caption = c->caption();

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

qreal Deleted::bufferScale() const
{
    return m_bufferScale;
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

bool Deleted::wasDecorated() const
{
    return m_wasDecorated;
}

void Deleted::layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const
{
    left = decoration_left;
    top = decoration_top;
    right = decoration_right;
    bottom = decoration_bottom;
}

QRect Deleted::transparentRect() const
{
    return transparent_rect;
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
    m_mainClients.removeAll(client);
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
