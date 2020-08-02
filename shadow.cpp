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
#include "effects.h"
#include "internal_client.h"
#include "toplevel.h"
#include "wayland_server.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationShadow>

#include <KWaylandServer/buffer_interface.h>
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
    if (!effects) {
        return nullptr;
    }
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
    if (!shadow) {
        return nullptr;
    }
    if (toplevel->effectWindow() && toplevel->effectWindow()->sceneWindow()) {
        toplevel->effectWindow()->sceneWindow()->updateShadow(shadow);
        emit toplevel->shadowChanged();
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
    m_topOffset = data[ShadowElementsCount];
    m_rightOffset = data[ShadowElementsCount+1];
    m_bottomOffset = data[ShadowElementsCount+2];
    m_leftOffset = data[ShadowElementsCount+3];
    updateShadowRegion();
    if (!prepareBackend()) {
        return false;
    }
    buildQuads();
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

    const QMargins &p = m_decorationShadow->padding();
    m_topOffset    = p.top();
    m_rightOffset  = p.right();
    m_bottomOffset = p.bottom();
    m_leftOffset   = p.left();
    updateShadowRegion();
    if (!prepareBackend()) {
        return false;
    }
    buildQuads();
    return true;
}

bool Shadow::init(const QPointer< KWaylandServer::ShadowInterface > &shadow)
{
    if (!shadow) {
        return false;
    }

    m_shadowElements[ShadowElementTop] = shadow->top() ? QPixmap::fromImage(shadow->top()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementTopRight] = shadow->topRight() ? QPixmap::fromImage(shadow->topRight()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementRight] = shadow->right() ? QPixmap::fromImage(shadow->right()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementBottomRight] = shadow->bottomRight() ? QPixmap::fromImage(shadow->bottomRight()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementBottom] = shadow->bottom() ? QPixmap::fromImage(shadow->bottom()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementBottomLeft] = shadow->bottomLeft() ? QPixmap::fromImage(shadow->bottomLeft()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementLeft] = shadow->left() ? QPixmap::fromImage(shadow->left()->data().copy()) : QPixmap();
    m_shadowElements[ShadowElementTopLeft] = shadow->topLeft() ? QPixmap::fromImage(shadow->topLeft()->data().copy()) : QPixmap();

    const QMarginsF &p = shadow->offset();
    m_topOffset    = p.top();
    m_rightOffset  = p.right();
    m_bottomOffset = p.bottom();
    m_leftOffset   = p.left();
    updateShadowRegion();
    if (!prepareBackend()) {
        return false;
    }
    buildQuads();
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

    const QMargins padding = window->property("kwin_shadow_padding").value<QMargins>();

    m_leftOffset = padding.left();
    m_topOffset = padding.top();
    m_rightOffset = padding.right();
    m_bottomOffset = padding.bottom();

    updateShadowRegion();

    if (!prepareBackend()) {
        return false;
    }

    buildQuads();

    return true;
}

void Shadow::updateShadowRegion()
{
    const QRect top(0, - m_topOffset, m_topLevel->width(), m_topOffset);
    const QRect right(m_topLevel->width(), - m_topOffset, m_rightOffset, m_topLevel->height() + m_topOffset + m_bottomOffset);
    const QRect bottom(0, m_topLevel->height(), m_topLevel->width(), m_bottomOffset);
    const QRect left(- m_leftOffset, - m_topOffset, m_leftOffset, m_topLevel->height() + m_topOffset + m_bottomOffset);
    m_shadowRegion = QRegion(top).united(right).united(bottom).united(left);
}

void Shadow::buildQuads()
{
    // prepare window quads
    m_shadowQuads.clear();
    const QSize top(m_shadowElements[ShadowElementTop].size());
    const QSize topRight(m_shadowElements[ShadowElementTopRight].size());
    const QSize right(m_shadowElements[ShadowElementRight].size());
    const QSize bottomRight(m_shadowElements[ShadowElementBottomRight].size());
    const QSize bottom(m_shadowElements[ShadowElementBottom].size());
    const QSize bottomLeft(m_shadowElements[ShadowElementBottomLeft].size());
    const QSize left(m_shadowElements[ShadowElementLeft].size());
    const QSize topLeft(m_shadowElements[ShadowElementTopLeft].size());
    if ((left.width() - m_leftOffset > m_topLevel->width()) ||
        (right.width() - m_rightOffset > m_topLevel->width()) ||
        (top.height() - m_topOffset > m_topLevel->height()) ||
        (bottom.height() - m_bottomOffset > m_topLevel->height())) {
        // if our shadow is bigger than the window, we don't render the shadow
        m_shadowRegion = QRegion();
        return;
    }

    const QRect outerRect(QPoint(-m_leftOffset, -m_topOffset), QPoint(m_topLevel->width() + m_rightOffset, m_topLevel->height() + m_bottomOffset));

    WindowQuad topLeftQuad(WindowQuadShadowTopLeft);
    topLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                      outerRect.y(), 0.0, 0.0);
    topLeftQuad[ 1 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y(), 1.0, 0.0);
    topLeftQuad[ 2 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y() + topLeft.height(), 1.0, 1.0);
    topLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                      outerRect.y() + topLeft.height(), 0.0, 1.0);
    m_shadowQuads.append(topLeftQuad);

    WindowQuad topQuad(WindowQuadShadowTop);
    topQuad[ 0 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y(), 0.0, 0.0);
    topQuad[ 1 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y(), 1.0, 0.0);
    topQuad[ 2 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y() + top.height(), 1.0, 1.0);
    topQuad[ 3 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y() + top.height(), 0.0, 1.0);
    m_shadowQuads.append(topQuad);

    WindowQuad topRightQuad(WindowQuadShadowTopRight);
    topRightQuad[ 0 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y(), 0.0, 0.0);
    topRightQuad[ 1 ] = WindowVertex(outerRect.right(),                     outerRect.y(), 1.0, 0.0);
    topRightQuad[ 2 ] = WindowVertex(outerRect.right(),                     outerRect.y() + topRight.height(), 1.0, 1.0);
    topRightQuad[ 3 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y() + topRight.height(), 0.0, 1.0);
    m_shadowQuads.append(topRightQuad);

    WindowQuad rightQuad(WindowQuadShadowRight);
    rightQuad[ 0 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.y() + topRight.height(), 0.0, 0.0);
    rightQuad[ 1 ] = WindowVertex(outerRect.right(),                    outerRect.y() + topRight.height(), 1.0, 0.0);
    rightQuad[ 2 ] = WindowVertex(outerRect.right(),                    outerRect.bottom() - bottomRight.height(), 1.0, 1.0);
    rightQuad[ 3 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.bottom() - bottomRight.height(), 0.0, 1.0);
    m_shadowQuads.append(rightQuad);

    WindowQuad bottomRightQuad(WindowQuadShadowBottomRight);
    bottomRightQuad[ 0 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom() - bottomRight.height(), 0.0, 0.0);
    bottomRightQuad[ 1 ] = WindowVertex(outerRect.right(),                          outerRect.bottom() - bottomRight.height(), 1.0, 0.0);
    bottomRightQuad[ 2 ] = WindowVertex(outerRect.right(),                          outerRect.bottom(), 1.0, 1.0);
    bottomRightQuad[ 3 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom(), 0.0, 1.0);
    m_shadowQuads.append(bottomRightQuad);

    WindowQuad bottomQuad(WindowQuadShadowBottom);
    bottomQuad[ 0 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom() - bottom.height(), 0.0, 0.0);
    bottomQuad[ 1 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom() - bottom.height(), 1.0, 0.0);
    bottomQuad[ 2 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom(), 1.0, 1.0);
    bottomQuad[ 3 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom(), 0.0, 1.0);
    m_shadowQuads.append(bottomQuad);

    WindowQuad bottomLeftQuad(WindowQuadShadowBottomLeft);
    bottomLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                       outerRect.bottom() - bottomLeft.height(), 0.0, 0.0);
    bottomLeftQuad[ 1 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom() - bottomLeft.height(), 1.0, 0.0);
    bottomLeftQuad[ 2 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom(), 1.0, 1.0);
    bottomLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                       outerRect.bottom(), 0.0, 1.0);
    m_shadowQuads.append(bottomLeftQuad);

    WindowQuad leftQuad(WindowQuadShadowLeft);
    leftQuad[ 0 ] = WindowVertex(outerRect.x(),                 outerRect.y() + topLeft.height(), 0.0, 0.0);
    leftQuad[ 1 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.y() + topLeft.height(), 1.0, 0.0);
    leftQuad[ 2 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.bottom() - bottomLeft.height(), 1.0, 1.0);
    leftQuad[ 3 ] = WindowVertex(outerRect.x(),                 outerRect.bottom() - bottomLeft.height(), 0.0, 1.0);
    m_shadowQuads.append(leftQuad);
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
    updateShadowRegion();
    buildQuads();
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
