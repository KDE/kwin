/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screenedgeeffect.h"
// KWin
#include <kwinglutils.h>
#include <kwingltexture.h>
// KDE
#include <Plasma/Svg>
// Qt
#include <QTimer>
#include <QPainter>

namespace KWin {

KWIN_EFFECT(screenedge, ScreenEdgeEffect)

ScreenEdgeEffect::ScreenEdgeEffect()
    : Effect()
    , m_glow(new Plasma::Svg(this))
    , m_cleanupTimer(new QTimer(this))
{
    m_glow->setImagePath("widgets/glowbar");
    connect(effects, SIGNAL(screenEdgeApproaching(ElectricBorder,qreal,QRect)), SLOT(edgeApproaching(ElectricBorder,qreal,QRect)));
    m_cleanupTimer->setInterval(5000);
    m_cleanupTimer->setSingleShot(true);
    connect(m_cleanupTimer, SIGNAL(timeout()), SLOT(cleanup()));
}

ScreenEdgeEffect::~ScreenEdgeEffect()
{
    cleanup();
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

void ScreenEdgeEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
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
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            texture->bind();
            if (effects->compositingType() == OpenGL2Compositing) {
                ShaderBinder binder(ShaderManager::SimpleShader);
                const QVector4D constant(opacity, opacity, opacity, opacity);
                binder.shader()->setUniform(GLShader::ModulationConstant, constant);
                texture->render(infiniteRegion(), (*it)->geometry);
            } else if (effects->compositingType() == OpenGL1Compositing) {
#ifdef KWIN_HAVE_OPENGL_1
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glColor4f(1.0, 1.0, 1.0, opacity);
                texture->render(infiniteRegion(), (*it)->geometry);
#endif
            }
            texture->unbind();
            glDisable(GL_BLEND);
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
                (*it)->texture.reset(createEdgeGlow(border, geometry.size()));
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
    glow->strength = factor;
    glow->geometry = geometry;

    // render the glow image
    if (border == ElectricTopLeft || border == ElectricTopRight || border == ElectricBottomRight || border == ElectricBottomLeft) {
        glow->texture.reset(createCornerGlow(border));
    } else {
        glow->texture.reset(createEdgeGlow(border, geometry.size()));
    }
    if (glow->texture.isNull()) {
        delete glow;
        return NULL;
    }
    glow->texture->setWrapMode(GL_CLAMP_TO_EDGE);

    return glow;
}

GLTexture *ScreenEdgeEffect::createCornerGlow(ElectricBorder border)
{
    switch (border) {
    case ElectricTopLeft:
        return new GLTexture(m_glow->pixmap("bottomright"));
    case ElectricTopRight:
        return  new GLTexture(m_glow->pixmap("bottomleft"));
    case ElectricBottomRight:
        return new GLTexture(m_glow->pixmap("topleft"));
    case ElectricBottomLeft:
        return new GLTexture(m_glow->pixmap("topright"));
    default:
        return NULL;
    }
}

GLTexture *ScreenEdgeEffect::createEdgeGlow(ElectricBorder border, const QSize &size)
{
    QPoint pixmapPosition(0, 0);
    QPixmap l, r, c;
    switch (border) {
    case ElectricTop:
        l = m_glow->pixmap("bottomleft");
        r = m_glow->pixmap("bottomright");
        c = m_glow->pixmap("bottom");
        break;
    case ElectricBottom:
        l = m_glow->pixmap("topleft");
        r = m_glow->pixmap("topright");
        c = m_glow->pixmap("top");
        pixmapPosition = QPoint(0, size.height() - c.height());
        break;
    case ElectricLeft:
        l = m_glow->pixmap("topright");
        r = m_glow->pixmap("bottomright");
        c = m_glow->pixmap("right");
        break;
    case ElectricRight:
        l = m_glow->pixmap("topleft");
        r = m_glow->pixmap("bottomleft");
        c = m_glow->pixmap("left");
        pixmapPosition = QPoint(size.width() - c.width(), 0);
        break;
    default:
        return NULL;
    }
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    if (border == ElectricBottom || border == ElectricTop) {
        p.drawPixmap(pixmapPosition, l);
        p.drawTiledPixmap(QRect(l.width(), pixmapPosition.y(), size.width() - l.width() - r.width(), c.height()), c);
        p.drawPixmap(QPoint(size.width() - r.width(), pixmapPosition.y()), r);
    } else {
        p.drawPixmap(pixmapPosition, l);
        p.drawTiledPixmap(QRect(pixmapPosition.x(), l.height(), c.width(), size.height() - l.height() - r.height()), c);
        p.drawPixmap(QPoint(pixmapPosition.x(), size.height() - r.height()), r);
    }
    p.end();
    return new GLTexture(image);
}

bool ScreenEdgeEffect::isActive() const
{
    return !m_borders.isEmpty();
}

} // namespace
