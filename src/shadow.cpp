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

QSharedPointer<GLTexture> Shadow::texture() const
{
    return nullptr;
}

static inline void distributeHorizontally(QRectF &leftRect, QRectF &rightRect)
{
    if (leftRect.right() > rightRect.left()) {
        const qreal boundedRight = std::min(leftRect.right(), rightRect.right());
        const qreal boundedLeft = std::max(leftRect.left(), rightRect.left());
        const qreal halfOverlap = (boundedRight - boundedLeft) / 2.0;
        leftRect.setRight(boundedRight - halfOverlap);
        rightRect.setLeft(boundedLeft + halfOverlap);
    }
}

static inline void distributeVertically(QRectF &topRect, QRectF &bottomRect)
{
    if (topRect.bottom() > bottomRect.top()) {
        const qreal boundedBottom = std::min(topRect.bottom(), bottomRect.bottom());
        const qreal boundedTop = std::max(topRect.top(), bottomRect.top());
        const qreal halfOverlap = (boundedBottom - boundedTop) / 2.0;
        topRect.setBottom(boundedBottom - halfOverlap);
        bottomRect.setTop(boundedTop + halfOverlap);
    }
}

WindowQuadList Shadow::geometry() const
{
    // Do not draw shadows if window width or window height is less than 5 px. 5 is an arbitrary choice.
    if (!m_topLevel->wantsShadowToBeRendered() || m_topLevel->width() < 5 || m_topLevel->height() < 5) {
        return WindowQuadList();
    }

    const QSizeF top(elementSize(Shadow::ShadowElementTop));
    const QSizeF topRight(elementSize(Shadow::ShadowElementTopRight));
    const QSizeF right(elementSize(Shadow::ShadowElementRight));
    const QSizeF bottomRight(elementSize(Shadow::ShadowElementBottomRight));
    const QSizeF bottom(elementSize(Shadow::ShadowElementBottom));
    const QSizeF bottomLeft(elementSize(Shadow::ShadowElementBottomLeft));
    const QSizeF left(elementSize(Shadow::ShadowElementLeft));
    const QSizeF topLeft(elementSize(Shadow::ShadowElementTopLeft));

    const QMarginsF shadowMargins(
            std::max({topLeft.width(), left.width(), bottomLeft.width()}),
            std::max({topLeft.height(), top.height(), topRight.height()}),
            std::max({topRight.width(), right.width(), bottomRight.width()}),
            std::max({bottomRight.height(), bottom.height(), bottomLeft.height()}));

    const QRect visibleRect = rect() + offset();
    const QRectF outerRect = QRect(0, 0, visibleRect.width(), visibleRect.height());

    const int width = shadowMargins.left() + std::max(top.width(), bottom.width()) + shadowMargins.right();
    const int height = shadowMargins.top() + std::max(left.height(), right.height()) + shadowMargins.bottom();

    QRectF topLeftRect;
    if (!topLeft.isEmpty()) {
        topLeftRect = QRectF(outerRect.topLeft(), topLeft);
    } else {
        topLeftRect = QRectF(outerRect.left() + shadowMargins.left(),
                             outerRect.top() + shadowMargins.top(), 0, 0);
    }

    QRectF topRightRect;
    if (!topRight.isEmpty()) {
        topRightRect = QRectF(outerRect.right() - topRight.width(), outerRect.top(),
                              topRight.width(), topRight.height());
    } else {
        topRightRect = QRectF(outerRect.right() - shadowMargins.right(),
                              outerRect.top() + shadowMargins.top(), 0, 0);
    }

    QRectF bottomRightRect;
    if (!bottomRight.isEmpty()) {
        bottomRightRect = QRectF(outerRect.right() - bottomRight.width(),
                                 outerRect.bottom() - bottomRight.height(),
                                 bottomRight.width(), bottomRight.height());
    } else {
        bottomRightRect = QRectF(outerRect.right() - shadowMargins.right(),
                                 outerRect.bottom() - shadowMargins.bottom(), 0, 0);
    }

    QRectF bottomLeftRect;
    if (!bottomLeft.isEmpty()) {
        bottomLeftRect = QRectF(outerRect.left(), outerRect.bottom() - bottomLeft.height(),
                                bottomLeft.width(), bottomLeft.height());
    } else {
        bottomLeftRect = QRectF(outerRect.left() + shadowMargins.left(),
                                outerRect.bottom() - shadowMargins.bottom(), 0, 0);
    }

    // Re-distribute the corner tiles so no one of them is overlapping with others.
    // By doing this, we assume that shadow's corner tiles are symmetric
    // and it is OK to not draw top/right/bottom/left tile between corners.
    // For example, let's say top-left and top-right tiles are overlapping.
    // In that case, the right side of the top-left tile will be shifted to left,
    // the left side of the top-right tile will shifted to right, and the top
    // tile won't be rendered.
    distributeHorizontally(topLeftRect, topRightRect);
    distributeHorizontally(bottomLeftRect, bottomRightRect);
    distributeVertically(topLeftRect, bottomLeftRect);
    distributeVertically(topRightRect, bottomRightRect);

    qreal tx1 = 0.0,
          tx2 = 0.0,
          ty1 = 0.0,
          ty2 = 0.0;

    WindowQuadList quads;
    quads.reserve(8);

    if (topLeftRect.isValid()) {
        tx1 = 0.0;
        ty1 = 0.0;
        tx2 = topLeftRect.width();
        ty2 = topLeftRect.height();
        WindowQuad topLeftQuad;
        topLeftQuad[0] = WindowVertex(topLeftRect.left(),  topLeftRect.top(),    tx1, ty1);
        topLeftQuad[1] = WindowVertex(topLeftRect.right(), topLeftRect.top(),    tx2, ty1);
        topLeftQuad[2] = WindowVertex(topLeftRect.right(), topLeftRect.bottom(), tx2, ty2);
        topLeftQuad[3] = WindowVertex(topLeftRect.left(),  topLeftRect.bottom(), tx1, ty2);
        quads.append(topLeftQuad);
    }

    if (topRightRect.isValid()) {
        tx1 = width - topRightRect.width();
        ty1 = 0.0;
        tx2 = width;
        ty2 = topRightRect.height();
        WindowQuad topRightQuad;
        topRightQuad[0] = WindowVertex(topRightRect.left(),  topRightRect.top(),    tx1, ty1);
        topRightQuad[1] = WindowVertex(topRightRect.right(), topRightRect.top(),    tx2, ty1);
        topRightQuad[2] = WindowVertex(topRightRect.right(), topRightRect.bottom(), tx2, ty2);
        topRightQuad[3] = WindowVertex(topRightRect.left(),  topRightRect.bottom(), tx1, ty2);
        quads.append(topRightQuad);
    }

    if (bottomRightRect.isValid()) {
        tx1 = width - bottomRightRect.width();
        tx2 = width;
        ty1 = height - bottomRightRect.height();
        ty2 = height;
        WindowQuad bottomRightQuad;
        bottomRightQuad[0] = WindowVertex(bottomRightRect.left(),  bottomRightRect.top(),    tx1, ty1);
        bottomRightQuad[1] = WindowVertex(bottomRightRect.right(), bottomRightRect.top(),    tx2, ty1);
        bottomRightQuad[2] = WindowVertex(bottomRightRect.right(), bottomRightRect.bottom(), tx2, ty2);
        bottomRightQuad[3] = WindowVertex(bottomRightRect.left(),  bottomRightRect.bottom(), tx1, ty2);
        quads.append(bottomRightQuad);
    }

    if (bottomLeftRect.isValid()) {
        tx1 = 0.0;
        tx2 = bottomLeftRect.width();
        ty1 = height - bottomLeftRect.height();
        ty2 = height;
        WindowQuad bottomLeftQuad;
        bottomLeftQuad[0] = WindowVertex(bottomLeftRect.left(),  bottomLeftRect.top(),    tx1, ty1);
        bottomLeftQuad[1] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.top(),    tx2, ty1);
        bottomLeftQuad[2] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.bottom(), tx2, ty2);
        bottomLeftQuad[3] = WindowVertex(bottomLeftRect.left(),  bottomLeftRect.bottom(), tx1, ty2);
        quads.append(bottomLeftQuad);
    }

    QRectF topRect(QPointF(topLeftRect.right(), outerRect.top()),
                   QPointF(topRightRect.left(), outerRect.top() + top.height()));

    QRectF rightRect(QPointF(outerRect.right() - right.width(), topRightRect.bottom()),
                     QPointF(outerRect.right(), bottomRightRect.top()));

    QRectF bottomRect(QPointF(bottomLeftRect.right(), outerRect.bottom() - bottom.height()),
                      QPointF(bottomRightRect.left(), outerRect.bottom()));

    QRectF leftRect(QPointF(outerRect.left(), topLeftRect.bottom()),
                    QPointF(outerRect.left() + left.width(), bottomLeftRect.top()));

    // Re-distribute left/right and top/bottom shadow tiles so they don't
    // overlap when the window is too small. Please notice that we don't
    // fix overlaps between left/top(left/bottom, right/top, and so on)
    // corner tiles because corresponding counter parts won't be valid when
    // the window is too small, which means they won't be rendered.
    distributeHorizontally(leftRect, rightRect);
    distributeVertically(topRect, bottomRect);

    if (topRect.isValid()) {
        tx1 = shadowMargins.left();
        ty1 = 0.0;
        tx2 = tx1 + top.width();
        ty2 = topRect.height();
        WindowQuad topQuad;
        topQuad[0] = WindowVertex(topRect.left(),  topRect.top(),    tx1, ty1);
        topQuad[1] = WindowVertex(topRect.right(), topRect.top(),    tx2, ty1);
        topQuad[2] = WindowVertex(topRect.right(), topRect.bottom(), tx2, ty2);
        topQuad[3] = WindowVertex(topRect.left(),  topRect.bottom(), tx1, ty2);
        quads.append(topQuad);
    }

    if (rightRect.isValid()) {
        tx1 = width - rightRect.width();
        ty1 = shadowMargins.top();
        tx2 = width;
        ty2 = ty1 + right.height();
        WindowQuad rightQuad;
        rightQuad[0] = WindowVertex(rightRect.left(),  rightRect.top(),    tx1, ty1);
        rightQuad[1] = WindowVertex(rightRect.right(), rightRect.top(),    tx2, ty1);
        rightQuad[2] = WindowVertex(rightRect.right(), rightRect.bottom(), tx2, ty2);
        rightQuad[3] = WindowVertex(rightRect.left(),  rightRect.bottom(), tx1, ty2);
        quads.append(rightQuad);
    }

    if (bottomRect.isValid()) {
        tx1 = shadowMargins.left();
        ty1 = height - bottomRect.height();
        tx2 = tx1 + bottom.width();
        ty2 = height;
        WindowQuad bottomQuad;
        bottomQuad[0] = WindowVertex(bottomRect.left(),  bottomRect.top(),    tx1, ty1);
        bottomQuad[1] = WindowVertex(bottomRect.right(), bottomRect.top(),    tx2, ty1);
        bottomQuad[2] = WindowVertex(bottomRect.right(), bottomRect.bottom(), tx2, ty2);
        bottomQuad[3] = WindowVertex(bottomRect.left(),  bottomRect.bottom(), tx1, ty2);
        quads.append(bottomQuad);
    }

    if (leftRect.isValid()) {
        tx1 = 0.0;
        ty1 = shadowMargins.top();
        tx2 = leftRect.width();
        ty2 = ty1 + left.height();
        WindowQuad leftQuad;
        leftQuad[0] = WindowVertex(leftRect.left(),  leftRect.top(),    tx1, ty1);
        leftQuad[1] = WindowVertex(leftRect.right(), leftRect.top(),    tx2, ty1);
        leftQuad[2] = WindowVertex(leftRect.right(), leftRect.bottom(), tx2, ty2);
        leftQuad[3] = WindowVertex(leftRect.left(),  leftRect.bottom(), tx1, ty2);
        quads.append(leftQuad);
    }

    return quads;
}

} // namespace
