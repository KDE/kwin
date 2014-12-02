/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "decorationrenderer.h"
#include "decoratedclient.h"
#include "client.h"
#include "deleted.h"

#include <kwinglobals.h>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <QDebug>
#include <QPainter>
#include <QTimer>

namespace KWin
{
namespace Decoration
{

Renderer::Renderer(DecoratedClientImpl *client)
    : QObject(client)
    , m_client(client)
    , m_imageSizesDirty(true)
{
    auto markImageSizesDirty = [this]{ m_imageSizesDirty = true; };
    connect(client->decoration(), &KDecoration2::Decoration::bordersChanged, this, markImageSizesDirty);
    connect(client->decoratedClient(), &KDecoration2::DecoratedClient::widthChanged, this, markImageSizesDirty);
    connect(client->decoratedClient(), &KDecoration2::DecoratedClient::heightChanged, this, markImageSizesDirty);
}

Renderer::~Renderer() = default;

void Renderer::schedule(const QRect &rect)
{
    m_scheduled = m_scheduled.united(rect);
    emit renderScheduled(rect);
}

QRegion Renderer::getScheduled()
{
    QRegion region = m_scheduled;
    m_scheduled = QRegion();
    return region;
}

QImage Renderer::renderToImage(const QRect &geo)
{
    Q_ASSERT(m_client);
    QImage image(geo.width(), geo.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setWindow(geo);
    p.setClipRect(geo);
    client()->decoration()->paint(&p, geo);
    return image;
}

void Renderer::reparent(Deleted *deleted)
{
    setParent(deleted);
    m_client = nullptr;
}

X11Renderer::X11Renderer(DecoratedClientImpl *client)
    : Renderer(client)
    , m_scheduleTimer(new QTimer(this))
    , m_gc(XCB_NONE)
{
    // delay any rendering to end of event cycle to catch multiple updates per cycle
    m_scheduleTimer->setSingleShot(true);
    m_scheduleTimer->setInterval(0);
    connect(m_scheduleTimer, &QTimer::timeout, this, &X11Renderer::render);
    connect(this, &Renderer::renderScheduled, m_scheduleTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
}

X11Renderer::~X11Renderer()
{
    if (m_gc != XCB_NONE) {
        xcb_free_gc(connection(), m_gc);
    }
}

void X11Renderer::reparent(Deleted *deleted)
{
    if (m_scheduleTimer->isActive()) {
        m_scheduleTimer->stop();
    }
    disconnect(m_scheduleTimer, &QTimer::timeout, this, &X11Renderer::render);
    disconnect(this, &Renderer::renderScheduled, m_scheduleTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    Renderer::reparent(deleted);
}

void X11Renderer::render()
{
    if (!client()) {
        return;
    }
    const QRegion scheduled = getScheduled();
    if (scheduled.isEmpty()) {
        return;
    }
    xcb_connection_t *c = connection();
    if (m_gc == XCB_NONE) {
        m_gc = xcb_generate_id(c);
        xcb_create_gc(c, m_gc, client()->client()->frameId(), 0, nullptr);
    }

    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    const QRect geometry = scheduled.boundingRect();
    left   = left.intersected(geometry);
    top    = top.intersected(geometry);
    right  = right.intersected(geometry);
    bottom = bottom.intersected(geometry);

    auto renderPart = [this, c](const QRect &geo) {
        if (geo.isNull()) {
            return;
        }
        QImage image = renderToImage(geo);
        xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, client()->client()->frameId(), m_gc,
                    image.width(), image.height(), geo.x(), geo.y(), 0, 24, image.byteCount(), image.constBits());
    };
    renderPart(left);
    renderPart(top);
    renderPart(right);
    renderPart(bottom);

    xcb_flush(c);
    resetImageSizesDirty();
}

}
}
