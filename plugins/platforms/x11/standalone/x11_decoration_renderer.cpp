/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_decoration_renderer.h"
#include "decorations/decoratedclient.h"
#include "x11client.h"
#include "deleted.h"

#include <kwinglobals.h>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <QTimer>

namespace KWin
{
namespace Decoration
{

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
        if (!geo.isValid()) {
            return;
        }
        QImage image = renderToImage(geo);
        xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, client()->client()->frameId(), m_gc,
                      image.width(), image.height(), geo.x(), geo.y(), 0, client()->client()->depth(),
                      image.sizeInBytes(), image.constBits());
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
