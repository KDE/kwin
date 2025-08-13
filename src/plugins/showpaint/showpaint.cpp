/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showpaint.h"

#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"

#include <QAction>
#include <QPainter>

namespace KWin
{

static const qreal s_alpha = 0.2;
static const QList<QColor> s_colors{
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray};

ShowPaintEffect::ShowPaintEffect() = default;

void ShowPaintEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &deviceRegion, LogicalOutput *screen)
{
    m_painted = QRegion();
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    if (effects->isOpenGLCompositing()) {
        paintGL(renderTarget, viewport);
    } else if (effects->compositingType() == QPainterCompositing) {
        paintQPainter(viewport);
    }
    if (++m_colorIndex == s_colors.count()) {
        m_colorIndex = 0;
    }
}

void ShowPaintEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &deviceRegion, WindowPaintData &data)
{
    m_painted += deviceRegion;
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

void ShowPaintEffect::paintGL(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    binder.shader()->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    binder.shader()->setUniform(GLShader::ColorUniform::Color, color);
    QList<QVector2D> verts;
    verts.reserve(m_painted.rectCount() * 12);
    for (const QRect &deviceRect : m_painted) {
        const auto r = deviceRect.translated(viewport.scaledRenderRect().topLeft());
        verts.push_back(QVector2D(r.x() + r.width(), r.y()));
        verts.push_back(QVector2D(r.x(), r.y()));
        verts.push_back(QVector2D(r.x(), r.y() + r.height()));
        verts.push_back(QVector2D(r.x(), r.y() + r.height()));
        verts.push_back(QVector2D(r.x() + r.width(), r.y() + r.height()));
        verts.push_back(QVector2D(r.x() + r.width(), r.y()));
    }
    vbo->setVertices(verts);
    vbo->render(GL_TRIANGLES);
    glDisable(GL_BLEND);
}

void ShowPaintEffect::paintQPainter(const RenderViewport &viewport)
{
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    for (const QRect deviceRect : m_painted) {
        effects->scenePainter()->fillRect(viewport.mapFromDeviceCoordinates(deviceRect), color);
    }
}

} // namespace KWin

#include "moc_showpaint.cpp"
