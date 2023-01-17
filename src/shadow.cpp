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
#include "composite.h"
#include "internalwindow.h"
#include "scene/workspacescene.h"
#include "wayland/shadow_interface.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "window.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationShadow>

#include <QWindow>

Q_DECLARE_METATYPE(QMargins)

namespace KWin
{

Shadow::Shadow(Window *window)
    : m_window(window)
    , m_cachedSize(window->size())
    , m_decorationShadow(nullptr)
{
    connect(m_window, &Window::frameGeometryChanged, this, &Shadow::geometryChanged);
}

Shadow::~Shadow()
{
}

std::unique_ptr<Shadow> Shadow::createShadow(Window *window)
{
    auto shadow = createShadowFromDecoration(window);
    if (!shadow && waylandServer()) {
        shadow = createShadowFromWayland(window);
    }
    if (!shadow && kwinApp()->x11Connection()) {
        shadow = createShadowFromX11(window);
    }
    if (!shadow) {
        shadow = createShadowFromInternalWindow(window);
    }
    return shadow;
}

std::unique_ptr<Shadow> Shadow::createShadowFromX11(Window *window)
{
    auto data = Shadow::readX11ShadowProperty(window->window());
    if (!data.isEmpty()) {
        auto shadow = Compositor::self()->scene()->createShadow(window);
        if (!shadow->init(data)) {
            return nullptr;
        }
        return shadow;
    } else {
        return nullptr;
    }
}

std::unique_ptr<Shadow> Shadow::createShadowFromDecoration(Window *window)
{
    if (!window->decoration()) {
        return nullptr;
    }
    auto shadow = Compositor::self()->scene()->createShadow(window);
    if (!shadow->init(window->decoration())) {
        return nullptr;
    }
    return shadow;
}

std::unique_ptr<Shadow> Shadow::createShadowFromWayland(Window *window)
{
    auto surface = window->surface();
    if (!surface) {
        return nullptr;
    }
    const auto s = surface->shadow();
    if (!s) {
        return nullptr;
    }
    auto shadow = Compositor::self()->scene()->createShadow(window);
    if (!shadow->init(s)) {
        return nullptr;
    }
    return shadow;
}

std::unique_ptr<Shadow> Shadow::createShadowFromInternalWindow(Window *window)
{
    const InternalWindow *internalWindow = qobject_cast<InternalWindow *>(window);
    if (!internalWindow) {
        return nullptr;
    }
    const QWindow *handle = internalWindow->handle();
    if (!handle) {
        return nullptr;
    }
    auto shadow = Compositor::self()->scene()->createShadow(window);
    if (!shadow->init(handle)) {
        return nullptr;
    }
    return shadow;
}

QVector<uint32_t> Shadow::readX11ShadowProperty(xcb_window_t id)
{
    QVector<uint32_t> ret;
    if (id != XCB_WINDOW_NONE) {
        Xcb::Property property(false, id, atoms->kde_net_wm_shadow, XCB_ATOM_CARDINAL, 0, 12);
        uint32_t *shadow = property.value<uint32_t *>();
        if (shadow) {
            ret.reserve(12);
            for (int i = 0; i < 12; ++i) {
                ret << shadow[i];
            }
        }
    }
    return ret;
}

bool Shadow::init(const QVector<uint32_t> &data)
{
    QVector<Xcb::WindowGeometry> pixmapGeometries(ShadowElementsCount);
    QVector<xcb_get_image_cookie_t> getImageCookies(ShadowElementsCount);
    auto *c = kwinApp()->x11Connection();
    for (int i = 0; i < ShadowElementsCount; ++i) {
        pixmapGeometries[i] = Xcb::WindowGeometry(data[i]);
    }
    auto discardReplies = [&getImageCookies](int start) {
        for (int i = start; i < getImageCookies.size(); ++i) {
            xcb_discard_reply(kwinApp()->x11Connection(), getImageCookies.at(i).sequence);
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
            discardReplies(i + 1);
            return false;
        }
        auto &geo = pixmapGeometries[i];
        QImage image(xcb_get_image_data(reply), geo->width, geo->height, QImage::Format_ARGB32);
        m_shadowElements[i] = image.copy();
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
        disconnect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::innerShadowRectChanged, m_window, &Window::updateShadow);
        disconnect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::shadowChanged, m_window, &Window::updateShadow);
        disconnect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::paddingChanged, m_window, &Window::updateShadow);
    }
    m_decorationShadow = decoration->shadow();
    if (!m_decorationShadow) {
        return false;
    }
    // setup connections - all just mapped to recreate
    connect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::innerShadowRectChanged, m_window, &Window::updateShadow);
    connect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::shadowChanged, m_window, &Window::updateShadow);
    connect(m_decorationShadow.data(), &KDecoration2::DecorationShadow::paddingChanged, m_window, &Window::updateShadow);

    m_offset = m_decorationShadow->padding();
    Q_EMIT offsetChanged();
    if (!prepareBackend()) {
        return false;
    }
    Q_EMIT textureChanged();
    return true;
}

static QImage shadowTileForBuffer(KWaylandServer::ClientBuffer *buffer)
{
    auto shmBuffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(buffer);
    if (shmBuffer) {
        return shmBuffer->data().copy();
    }
    return QImage();
}

bool Shadow::init(const QPointer<KWaylandServer::ShadowInterface> &shadow)
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

    m_shadowElements[ShadowElementLeft] = leftTile;
    m_shadowElements[ShadowElementTopLeft] = topLeftTile;
    m_shadowElements[ShadowElementTop] = topTile;
    m_shadowElements[ShadowElementTopRight] = topRightTile;
    m_shadowElements[ShadowElementRight] = rightTile;
    m_shadowElements[ShadowElementBottomRight] = bottomRightTile;
    m_shadowElements[ShadowElementBottom] = bottomTile;
    m_shadowElements[ShadowElementBottomLeft] = bottomLeftTile;

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
    if (!m_window) {
        return false;
    }

    if (m_decorationShadow) {
        if (m_window) {
            if (m_window->decoration()) {
                if (init(m_window->decoration())) {
                    return true;
                }
            }
        }
        return false;
    }

    if (waylandServer()) {
        if (m_window && m_window->surface()) {
            if (const auto &s = m_window->surface()->shadow()) {
                if (init(s)) {
                    return true;
                }
            }
        }
    }

    if (InternalWindow *window = qobject_cast<InternalWindow *>(m_window)) {
        if (init(window->handle())) {
            return true;
        }
    }

    auto data = Shadow::readX11ShadowProperty(m_window->window());
    if (data.isEmpty()) {
        return false;
    }

    init(data);

    return true;
}

Window *Shadow::window() const
{
    return m_window;
}

void Shadow::setWindow(Window *window)
{
    m_window = window;
    connect(m_window, &Window::frameGeometryChanged, this, &Shadow::geometryChanged);
}
void Shadow::geometryChanged()
{
    if (m_cachedSize == m_window->size()) {
        return;
    }
    m_cachedSize = m_window->size();
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

} // namespace
