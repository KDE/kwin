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

#include "deleted.h"

#include "workspace.h"
#include "client.h"
#include "netinfo.h"
#include "shadow.h"
#include "decorations/decoratedclient.h"
#include "decorations/decorationrenderer.h"

#include <QDebug>

namespace KWin
{

Deleted::Deleted()
    : Toplevel()
    , m_frame(XCB_WINDOW_NONE)
    , no_border(true)
    , m_layer(UnknownLayer)
    , m_minimized(false)
    , m_modal(false)
    , m_wasClient(false)
    , m_wasCurrentTab(true)
    , m_decorationRenderer(nullptr)
    , m_fullscreen(false)
    , m_keepAbove(false)
    , m_keepBelow(false)
{
}

Deleted::~Deleted()
{
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
    delete this;
}

void Deleted::copyToDeleted(Toplevel* c)
{
    assert(dynamic_cast< Deleted* >(c) == NULL);
    Toplevel::copyToDeleted(c);
    desk = c->desktop();
    activityList = c->activities();
    contentsRect = QRect(c->clientPos(), c->clientSize());
    m_contentPos = c->clientContentPos();
    transparent_rect = c->transparentRect();
    m_layer = c->layer();
    m_frame = c->frameId();
    m_opacity = c->opacity();
    m_type = c->windowType(true);
    m_windowRole = c->windowRole();
    if (WinInfo* cinfo = dynamic_cast< WinInfo* >(info))
        cinfo->disable();
    if (AbstractClient *client = dynamic_cast<AbstractClient*>(c)) {
        no_border = client->noBorder();
        if (!no_border) {
            client->layoutDecorationRects(decoration_left,
                                          decoration_top,
                                          decoration_right,
                                          decoration_bottom);
            if (client->isDecorated()) {
                if (Decoration::Renderer *renderer = client->decoratedClient()->renderer()) {
                    m_decorationRenderer = renderer;
                    m_decorationRenderer->reparent(this);
                }
            }
        }
        m_wasClient = true;
        m_minimized = client->isMinimized();
        m_modal = client->isModal();
        m_mainClients = client->mainClients();
        foreach (AbstractClient *c, m_mainClients) {
            connect(c, &AbstractClient::windowClosed, this, &Deleted::mainClientClosed);
        }
        m_fullscreen = client->isFullScreen();
        m_wasCurrentTab = client->isCurrentTab();
        m_keepAbove = client->keepAbove();
        m_keepBelow = client->keepBelow();
        m_caption = client->caption();
    }
}

void Deleted::windowReleased()
{
    // needs to be delayed
    // a) when calling from effects, otherwise it'd be rather complicated to handle the case of the
    // window going away during a painting pass
    // b) to prevent dangeling pointers in the stacking order, see bug #317765
    deleteLater();
}

int Deleted::desktop() const
{
    return desk;
}

QStringList Deleted::activities() const
{
    return activityList;
}

QPoint Deleted::clientPos() const
{
    return contentsRect.topLeft();
}

QSize Deleted::clientSize() const
{
    return contentsRect.size();
}

void Deleted::debug(QDebug& stream) const
{
    stream << "\'ID:" << window() << "\' (deleted)";
}

void Deleted::layoutDecorationRects(QRect& left, QRect& top, QRect& right, QRect& bottom) const
{
    left = decoration_left;
    top = decoration_top;
    right = decoration_right;
    bottom = decoration_bottom;
}

QRect Deleted::decorationRect() const
{
    return rect();
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
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(client))
        m_mainClients.removeAll(c);
}

xcb_window_t Deleted::frameId() const
{
    return m_frame;
}

double Deleted::opacity() const
{
    return m_opacity;
}

QByteArray Deleted::windowRole() const
{
    return m_windowRole;
}

} // namespace

