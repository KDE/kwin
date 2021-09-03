/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "shadow.h"
// kwin
#include "atoms.h"
#include "abstract_client.h"
#include "composite.h"
#include "internal_client.h"
#include "scene.h"
#include "toplevel.h"
#include "wayland_server.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationShadow>

#include <KWaylandServer/shmclientbuffer.h>
#include <KWaylandServer/shadow_interface.h>
#include <KWaylandServer/surface_interface.h>

#include <QWindow>

Q_DECLARE_METATYPE(QMargins)

namespace KWin
{

Shadow::Shadow(Toplevel *toplevel)
    : m_topLevel(toplevel)
    , m_cachedSize(toplevel->size())
    , m_decorationShadow(nullptr)
{
    connect(m_topLevel, &Toplevel::frameGeometryChanged, this, &Shadow::geometryChanged);
}

Shadow::~Shadow()
{
}

Shadow *Shadow::createShadow(Toplevel *toplevel)
{
    Shadow *shadow = createShadowFromDecoration(toplevel);
    if (!shadow && waylandServer()) {
        shadow = createShadowFromWayland(toplevel);
    }
    if (!shadow && kwinApp()->x11Connection()) {
        shadow = createShadowFromX11(toplevel);
    }
    if (!shadow) {
        shadow = createShadowFromInternalWindow(toplevel);
    }
    return shadow;
}

Shadow *Shadow::createShadowFromX11(Toplevel *toplevel)
{
    auto data = Shadow::readX11ShadowProperty(toplevel->window());
    if (!data.isEmpty()) {
        Shadow *shadow = Compositor::self()->scene()->createShadow(toplevel);

        if (!shadow->init(data)) {
            delete shadow;
            return nullptr;
        }
        return shadow;
    } else {
        return nullptr;
    }
}

Shadow *Shadow::createShadowFromDecoration(Toplevel *toplevel)
{
    AbstractClient *c = qobject_cast<AbstractClient*>(toplevel);
    if (!c) {
        return nullptr;
    }
    if (!c->decoration()) {
        return nullptr;
    }
    Shadow *shadow = Compositor::self()->scene()->createShadow(toplevel);
    if (!shadow->init(c->decoration())) {
        delete shadow;
        return nullptr;
    }
    return shadow;
}

Shadow *Shadow::createShadowFromWayland(Toplevel *toplevel)
{
    auto surface = toplevel->surface();
    if (!surface) {
        return nullptr;
    }
    const auto s = surface->shadow();
    if (!s) {
        return nullptr;
    }
    Shadow *shadow = Compositor::self()->scene()->createShadow(toplevel);
    if (!shadow->init(s)) {
        delete shadow;
        return nullptr;
    }
    return shadow;
}

Shadow *Shadow::createShadowFromInternalWindow(Toplevel *toplevel)
{
    const InternalClient *client = qobject_cast<InternalClient *>(toplevel);
    if (!client) {
        return nullptr;
    }
    const QWindow *window = client->internalWindow();
    if (!window) {
        return nullptr;
    }
    Shadow *shadow = Compositor::self()->scene()->createShadow(toplevel);
    if (!shadow->init(window)) {
        delete shadow;
        return nullptr;
    }
    return shadow;
}

QVector< uint32_t > Shadow::readX11ShadowProperty(xcb_window_t id)
{
    QVector<uint32_t> ret;
    if (id != XCB_WINDOW_NONE) {
        Xcb::Property property(false, id, atoms->kde_net_wm_shadow, XCB_ATOM_CARDINAL, 0, 12);
        uint32_t *shadow = property.value<uint32_t*>();
        if (shadow) {
            ret.reserve(12);
            for (int i=0; i<12; ++i) {
                ret << shadow[i];
            }
        }
    }
    return ret;
}

bool Shadow::init(const QVector< uint32_t > &data)
{
    QVector<Xcb::WindowGeometry> pixmapGeometries(ShadowElementsCount);
    QVector<xcb_get_image_cookie_t> getImageCookies(ShadowElementsCount);
    auto *c = connection();
    for (int i = 0; i < ShadowElementsCount; ++i) {
        pixmapGeometries[i] = Xcb::WindowGeometry(data[i]);
    }
    auto discardReplies = [&getImageCookies](int start) {
        for (int i = start; i < getImageCookies.size(); ++i) {
            xcb_discard_reply(connection(), getImageCookies.at(i).sequence);
        }
    };
    for (int i = 0; i < ShadowElementsCount; ++i) {
        auto &geo = pixmapGeometries[i];
        if (geo.isNull()) {
            discardReplies(0);
            return false;
        }
        getImageCookies[i] = xcb_get_image_unchecked(c, XCB_IMAGE_FORMAT_Z_PIXMAP, data[i],
                                                     0, 0, geo->width, geo->height, ~0);
    }
    for (int i = 0; i < ShadowElementsCount; ++i) {
        auto *reply = xcb_get_image_reply(c, getImageCookies.at(i), nullptr);
        if (!reply) {
            discardReplies(i+1);
            return false;
        }
        auto &geo = pixmapGeometries[i];
        QImage image(xcb_get_image_data(reply), geo->width, geo->height, QImage::Format_ARGB32);
        m_shadowElements[i] = QPixmap::fromImage(image);
        free(reply);
    }
    m_offset = QMargins(data[ShadowElementsCount + 3],
                        data[ShadowElementsCount],
                        data[ShadowElementsCount + 1],
                        data[ShadowElementsCount + 2]);
    Q_EMIT offsetChanged();
    if (!prepareBackend()) {
        return false;
    }
    Q_EMIT textureChanged();
    return true;
}

bool Shadow::init(KDecoration2::Decoration *decoration)
{
    if (m_decorationShadow) {
        // disconnect previous connections
        disconnect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::innerShadowRectChanged, m_topLevel, &Toplevel::updateShadow);
        disconnect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::shadowChanged,        m_topLevel, &Toplevel::updateShadow);
        disconnect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::paddingChanged,       m_topLevel, &Toplevel::updateShadow);
    }
    m_decorationShadow = decoration->shadow();
    if (!m_decorationShadow) {
        return false;
    }
    // setup connections - all just mapped to recreate
    connect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::innerShadowRectChanged, m_topLevel, &Toplevel::updateShadow);
    connect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::shadowChanged,        m_topLevel, &Toplevel::updateShadow);
    connect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::paddingChanged,       m_topLevel, &Toplevel::updateShadow);

    m_offset = m_decorationShadow->padding();
    Q_EMIT offsetChanged();
    if (!prepareBackend()) {
        return false;
    }
    Q_EMIT textureChanged();
    return true;
}

static QPixmap shadowTileForBuffer(KWaylandServer::ClientBuffer *buffer)
{
    auto shmBuffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(buffer);
    if (shmBuffer) {
        return QPixmap::fromImage(shmBuffer->data().copy());
    }
    return QPixmap();
}

bool Shadow::init(const QPointer< KWaylandServer::ShadowInterface > &shadow)
{
    if (!shadow) {
        return false;
    }

    m_shadowElements[ShadowElementTop] = shadowTileForBuffer(shadow->top());
    m_shadowElements[ShadowElementTopRight] = shadowTileForBuffer(shadow->topRight());
    m_shadowElements[ShadowElementRight] = shadowTileForBuffer(shadow->right());
    m_shadowElements[ShadowElementBottomRight] = shadowTileForBuffer(shadow->bottomRight());
    m_shadowElements[ShadowElementBottom] = shadowTileForBuffer(shadow->bottom());
    m_shadowElements[ShadowElementBottomLeft] = shadowTileForBuffer(shadow->bottomLeft());
    m_shadowElements[ShadowElementLeft] = shadowTileForBuffer(shadow->left());
    m_shadowElements[ShadowElementTopLeft] = shadowTileForBuffer(shadow->topLeft());

    m_offset = shadow->offset().toMargins();
    Q_EMIT offsetChanged();
    if (!prepareBackend()) {
        return false;
    }
    Q_EMIT textureChanged();
    return true;
}

bool Shadow::init(const QWindow *window)
{
    const bool isEnabled = window->property("kwin_shadow_enabled").toBool();
    if (!isEnabled) {
        return false;
    }

    const QImage leftTile = window->property("kwin_shadow_left_tile").value<QImage>();
    const QImage topLeftTile = window->property("kwin_shadow_top_left_tile").value<QImage>();
    const QImage topTile = window->property("kwin_shadow_top_tile").value<QImage>();
    const QImage topRightTile = window->property("kwin_shadow_top_right_tile").value<QImage>();
    const QImage rightTile = window->property("kwin_shadow_right_tile").value<QImage>();
    const QImage bottomRightTile = window->property("kwin_shadow_bottom_right_tile").value<QImage>();
    const QImage bottomTile = window->property("kwin_shadow_bottom_tile").value<QImage>();
    const QImage bottomLeftTile = window->property("kwin_shadow_bottom_left_tile").value<QImage>();

    m_shadowElements[ShadowElementLeft] = QPixmap::fromImage(leftTile);
    m_shadowElements[ShadowElementTopLeft] = QPixmap::fromImage(topLeftTile);
    m_shadowElements[ShadowElementTop] = QPixmap::fromImage(topTile);
    m_shadowElements[ShadowElementTopRight] = QPixmap::fromImage(topRightTile);
    m_shadowElements[ShadowElementRight] = QPixmap::fromImage(rightTile);
    m_shadowElements[ShadowElementBottomRight] = QPixmap::fromImage(bottomRightTile);
    m_shadowElements[ShadowElementBottom] = QPixmap::fromImage(bottomTile);
    m_shadowElements[ShadowElementBottomLeft] = QPixmap::fromImage(bottomLeftTile);

    m_offset = window->property("kwin_shadow_padding").value<QMargins>();
    Q_EMIT offsetChanged();

    if (!prepareBackend()) {
        return false;
    }
    Q_EMIT textureChanged();
    return true;
}

bool Shadow::updateShadow()
{
    if (!m_topLevel) {
        return false;
    }

    if (m_decorationShadow) {
        if (AbstractClient *c = qobject_cast<AbstractClient*>(m_topLevel)) {
            if (c->decoration()) {
                if (init(c->decoration())) {
                    return true;
                }
            }
        }
        return false;
    }

    if (waylandServer()) {
        if (m_topLevel && m_topLevel->surface()) {
            if (const auto &s = m_topLevel->surface()->shadow()) {
                if (init(s)) {
                    return true;
                }
            }
        }
    }

    if (InternalClient *client = qobject_cast<InternalClient *>(m_topLevel)) {
        if (init(client->internalWindow())) {
            return true;
        }
    }

    auto data = Shadow::readX11ShadowProperty(m_topLevel->window());
    if (data.isEmpty()) {
        return false;
    }

    init(data);

    return true;
}

Toplevel *Shadow::toplevel() const
{
    return m_topLevel;
}

void Shadow::setToplevel(Toplevel *topLevel)
{
    m_topLevel = topLevel;
    connect(m_topLevel, &Toplevel::frameGeometryChanged, this, &Shadow::geometryChanged);
}
void Shadow::geometryChanged()
{
    if (m_cachedSize == m_topLevel->size()) {
        return;
    }
    m_cachedSize = m_topLevel->size();
    Q_EMIT rectChanged();
}

QImage Shadow::decorationShadowImage() const
{
    if (!m_decorationShadow) {
        return QImage();
    }
    return m_decorationShadow->shadow();
}

QSize Shadow::elementSize(Shadow::ShadowElements element) const
{
    if (m_decorationShadow) {
        switch (element) {
        case ShadowElementTop:
            return m_decorationShadow->topGeometry().size();
        case ShadowElementTopRight:
            return m_decorationShadow->topRightGeometry().size();
        case ShadowElementRight:
            return m_decorationShadow->rightGeometry().size();
        case ShadowElementBottomRight:
            return m_decorationShadow->bottomRightGeometry().size();
        case ShadowElementBottom:
            return m_decorationShadow->bottomGeometry().size();
        case ShadowElementBottomLeft:
            return m_decorationShadow->bottomLeftGeometry().size();
        case ShadowElementLeft:
            return m_decorationShadow->leftGeometry().size();
        case ShadowElementTopLeft:
            return m_decorationShadow->topLeftGeometry().size();
        default:
            return QSize();
        }
    } else {
        return m_shadowElements[element].size();
    }
}

void Shadow::setShadowElement(const QPixmap &shadow, Shadow::ShadowElements element)
{
    m_shadowElements[element] = shadow;
}

} // namespace
