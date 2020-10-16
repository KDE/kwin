/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenedgeeffect.h"
// KWin
#include <kwinglutils.h>
#include <kwingltexture.h>
#include <kwinxrenderutils.h>
// KDE
#include <Plasma/Svg>
// Qt
#include <QTimer>
#include <QPainter>
#include <QVector4D>
// xcb
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <xcb/render.h>
#endif

namespace KWin {

ScreenEdgeEffect::ScreenEdgeEffect()
    : Effect()
    , m_cleanupTimer(new QTimer(this))
{
    connect(effects, &EffectsHandler::screenEdgeApproaching, this, &ScreenEdgeEffect::edgeApproaching);
    m_cleanupTimer->setInterval(5000);
    m_cleanupTimer->setSingleShot(true);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ScreenEdgeEffect::cleanup);
    connect(effects, &EffectsHandler::screenLockingChanged, this,
        [this] (bool locked) {
            if (locked) {
                cleanup();
            }
        }
    );
}

ScreenEdgeEffect::~ScreenEdgeEffect()
{
    cleanup();
}

void ScreenEdgeEffect::ensureGlowSvg()
{
    if (!m_glow) {
        m_glow = new Plasma::Svg(this);
        m_glow->setImagePath(QStringLiteral("widgets/glowbar"));
    }
}

void ScreenEdgeEffect::cleanup()
{
    for (QHash<ElectricBorder, Glow*>::iterator it = m_borders.begin();
            it != m_borders.end();
            ++it) {
        effects->addRepaint((*it)->geometry);
    }
    qDeleteAll(m_borders);
    m_borders.clear();
}

void ScreenEdgeEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    effects->prePaintScreen(data, time);
    for (QHash<ElectricBorder, Glow*>::iterator it = m_borders.begin();
            it != m_borders.end();
            ++it) {
        if ((*it)->strength == 0.0) {
            continue;
        }
        data.paint += (*it)->geometry;
    }
}

void ScreenEdgeEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
    for (QHash<ElectricBorder, Glow*>::iterator it = m_borders.begin();
            it != m_borders.end();
            ++it) {
        const qreal opacity = (*it)->strength;
        if (opacity == 0.0) {
            continue;
        }
        if (effects->isOpenGLCompositing()) {
            GLTexture *texture = (*it)->texture.data();
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            texture->bind();
            ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate);
            const QVector4D constant(opacity, opacity, opacity, opacity);
            binder.shader()->setUniform(GLShader::ModulationConstant, constant);
            QMatrix4x4 mvp = data.projectionMatrix();
            mvp.translate((*it)->geometry.x(), (*it)->geometry.y());
            binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
            texture->render(infiniteRegion(), (*it)->geometry);
            texture->unbind();
            glDisable(GL_BLEND);
        } else if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            const QRect &rect = (*it)->geometry;
            const QSize &size = (*it)->pictureSize;
            int x = rect.x();
            int y = rect.y();
            int width = rect.width();
            int height = rect.height();
            switch ((*it)->border) {
                case ElectricTopRight:
                    x = rect.x() + rect.width() - size.width();
                    break;
                case ElectricBottomRight:
                    x = rect.x() + rect.width() - size.width();
                    y = rect.y() + rect.height() - size.height();
                    break;
                case ElectricBottomLeft:
                    y = rect.y() + rect.height() - size.height();
                    break;
                default:
                    // nothing
                    break;
            }
            xcb_render_composite(xcbConnection(), XCB_RENDER_PICT_OP_OVER, *(*it)->picture.data(),
                                 xRenderBlendPicture(opacity), effects->xrenderBufferPicture(),
                                 0, 0, 0, 0, x, y, width, height);
#endif
        } else if (effects->compositingType() == QPainterCompositing) {
            QImage tmp((*it)->image->size(), QImage::Format_ARGB32_Premultiplied);
            tmp.fill(Qt::transparent);
            QPainter p(&tmp);
            p.drawImage(0, 0, *(*it)->image.data());
            QColor color(Qt::transparent);
            color.setAlphaF(opacity);
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.fillRect(QRect(QPoint(0, 0), tmp.size()), color);
            p.end();

            QPainter *painter = effects->scenePainter();
            const QRect &rect = (*it)->geometry;
            const QSize &size = (*it)->pictureSize;
            int x = rect.x();
            int y = rect.y();
            switch ((*it)->border) {
                case ElectricTopRight:
                    x = rect.x() + rect.width() - size.width();
                    break;
                case ElectricBottomRight:
                    x = rect.x() + rect.width() - size.width();
                    y = rect.y() + rect.height() - size.height();
                    break;
                case ElectricBottomLeft:
                    y = rect.y() + rect.height() - size.height();
                    break;
                default:
                    // nothing
                    break;
            }
            painter->drawImage(QPoint(x, y), tmp);
        }
    }
}

void ScreenEdgeEffect::edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry)
{
    QHash<ElectricBorder, Glow*>::iterator it = m_borders.find(border);
    if (it != m_borders.end()) {
        // need to update
        effects->addRepaint((*it)->geometry);
        (*it)->strength = factor;
        if ((*it)->geometry != geometry) {
            (*it)->geometry = geometry;
            effects->addRepaint((*it)->geometry);
            if (border == ElectricLeft || border == ElectricRight || border == ElectricTop || border == ElectricBottom) {
                if (effects->isOpenGLCompositing()) {
                    (*it)->texture.reset(createEdgeGlow<GLTexture>(border, geometry.size()));
                } else if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
                    (*it)->picture.reset(createEdgeGlow<XRenderPicture>(border, geometry.size()));
#endif
                } else if (effects->compositingType() == QPainterCompositing) {
                    (*it)->image.reset(createEdgeGlow<QImage>(border, geometry.size()));
                }
            }
        }
        if (factor == 0.0) {
            m_cleanupTimer->start();
        } else {
            m_cleanupTimer->stop();
        }
    } else if (factor != 0.0) {
        // need to generate new Glow
        Glow *glow = createGlow(border, factor, geometry);
        if (glow) {
            m_borders.insert(border, glow);
            effects->addRepaint(glow->geometry);
        }
    }
}

Glow *ScreenEdgeEffect::createGlow(ElectricBorder border, qreal factor, const QRect &geometry)
{
    Glow *glow = new Glow();
    glow->border = border;
    glow->strength = factor;
    glow->geometry = geometry;

    // render the glow image
    if (effects->isOpenGLCompositing()) {
        effects->makeOpenGLContextCurrent();
        if (border == ElectricTopLeft || border == ElectricTopRight || border == ElectricBottomRight || border == ElectricBottomLeft) {
            glow->texture.reset(createCornerGlow<GLTexture>(border));
        } else {
            glow->texture.reset(createEdgeGlow<GLTexture>(border, geometry.size()));
        }
        if (!glow->texture.isNull()) {
            glow->texture->setWrapMode(GL_CLAMP_TO_EDGE);
        }
        if (glow->texture.isNull()) {
            delete glow;
            return nullptr;
        }
    } else if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if (border == ElectricTopLeft || border == ElectricTopRight || border == ElectricBottomRight || border == ElectricBottomLeft) {
            glow->pictureSize = cornerGlowSize(border);
            glow->picture.reset(createCornerGlow<XRenderPicture>(border));
        } else {
            glow->pictureSize = geometry.size();
            glow->picture.reset(createEdgeGlow<XRenderPicture>(border, geometry.size()));
        }
        if (glow->picture.isNull()) {
            delete glow;
            return nullptr;
        }
#endif
    } else if (effects->compositingType() == QPainterCompositing) {
        if (border == ElectricTopLeft || border == ElectricTopRight || border == ElectricBottomRight || border == ElectricBottomLeft) {
            glow->image.reset(createCornerGlow<QImage>(border));
            glow->pictureSize = cornerGlowSize(border);
        } else {
            glow->image.reset(createEdgeGlow<QImage>(border, geometry.size()));
            glow->pictureSize = geometry.size();
        }
        if (glow->image.isNull()) {
            delete glow;
            return nullptr;
        }
    }

    return glow;
}

template <typename T>
T *ScreenEdgeEffect::createCornerGlow(ElectricBorder border)
{
    ensureGlowSvg();

    switch (border) {
    case ElectricTopLeft:
        return new T(m_glow->pixmap(QStringLiteral("bottomright")).toImage());
    case ElectricTopRight:
        return  new T(m_glow->pixmap(QStringLiteral("bottomleft")).toImage());
    case ElectricBottomRight:
        return new T(m_glow->pixmap(QStringLiteral("topleft")).toImage());
    case ElectricBottomLeft:
        return new T(m_glow->pixmap(QStringLiteral("topright")).toImage());
    default:
        return nullptr;
    }
}

QSize ScreenEdgeEffect::cornerGlowSize(ElectricBorder border)
{
    ensureGlowSvg();

    switch (border) {
    case ElectricTopLeft:
        return m_glow->elementSize(QStringLiteral("bottomright"));
    case ElectricTopRight:
        return  m_glow->elementSize(QStringLiteral("bottomleft"));
    case ElectricBottomRight:
        return m_glow->elementSize(QStringLiteral("topleft"));
    case ElectricBottomLeft:
        return m_glow->elementSize(QStringLiteral("topright"));
    default:
        return QSize();
    }
}

template <typename T>
T *ScreenEdgeEffect::createEdgeGlow(ElectricBorder border, const QSize &size)
{
    ensureGlowSvg();

    const bool stretchBorder = m_glow->hasElement(QStringLiteral("hint-stretch-borders"));

    QPoint pixmapPosition(0, 0);
    QPixmap l, r, c;
    switch (border) {
    case ElectricTop:
        l = m_glow->pixmap(QStringLiteral("bottomleft"));
        r = m_glow->pixmap(QStringLiteral("bottomright"));
        c = m_glow->pixmap(QStringLiteral("bottom"));
        break;
    case ElectricBottom:
        l = m_glow->pixmap(QStringLiteral("topleft"));
        r = m_glow->pixmap(QStringLiteral("topright"));
        c = m_glow->pixmap(QStringLiteral("top"));
        pixmapPosition = QPoint(0, size.height() - c.height());
        break;
    case ElectricLeft:
        l = m_glow->pixmap(QStringLiteral("topright"));
        r = m_glow->pixmap(QStringLiteral("bottomright"));
        c = m_glow->pixmap(QStringLiteral("right"));
        break;
    case ElectricRight:
        l = m_glow->pixmap(QStringLiteral("topleft"));
        r = m_glow->pixmap(QStringLiteral("bottomleft"));
        c = m_glow->pixmap(QStringLiteral("left"));
        pixmapPosition = QPoint(size.width() - c.width(), 0);
        break;
    default:
        return nullptr;
    }
    QPixmap image(size);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    if (border == ElectricBottom || border == ElectricTop) {
        p.drawPixmap(pixmapPosition, l);
        const QRect cRect(l.width(), pixmapPosition.y(), size.width() - l.width() - r.width(), c.height());
        if (stretchBorder) {
            p.drawPixmap(cRect, c);
        } else {
            p.drawTiledPixmap(cRect, c);
        }
        p.drawPixmap(QPoint(size.width() - r.width(), pixmapPosition.y()), r);
    } else {
        p.drawPixmap(pixmapPosition, l);
        const QRect cRect(pixmapPosition.x(), l.height(), c.width(), size.height() - l.height() - r.height());
        if (stretchBorder) {
            p.drawPixmap(cRect, c);
        } else {
            p.drawTiledPixmap(cRect, c);
        }
        p.drawPixmap(QPoint(pixmapPosition.x(), size.height() - r.height()), r);
    }
    p.end();
    return new T(image.toImage());
}

bool ScreenEdgeEffect::isActive() const
{
    return !m_borders.isEmpty() && !effects->isScreenLocked();
}

} // namespace
