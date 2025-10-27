/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "touchpoints.h"

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"
#include <QAction>

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QPainter>

#include <cmath>

namespace KWin
{

TouchPointsEffect::TouchPointsEffect()
    : Effect()
{
}

TouchPointsEffect::~TouchPointsEffect() = default;

static const Qt::GlobalColor s_colors[] = {
    Qt::blue,
    Qt::red,
    Qt::green,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray,
    Qt::darkBlue,
    Qt::darkRed,
    Qt::darkGreen};

Qt::GlobalColor TouchPointsEffect::colorForId(quint32 id)
{
    auto it = m_colors.constFind(id);
    if (it != m_colors.constEnd()) {
        return it.value();
    }
    static int s_colorIndex = -1;
    s_colorIndex = (s_colorIndex + 1) % 10;
    m_colors.insert(id, s_colors[s_colorIndex]);
    return s_colors[s_colorIndex];
}

bool TouchPointsEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    TouchPoint point;
    point.pos = pos;
    point.press = true;
    point.color = colorForId(id);
    m_points << point;
    m_latestPositions.insert(id, pos);
    repaint();
    return false;
}

bool TouchPointsEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    TouchPoint point;
    point.pos = pos;
    point.press = true;
    point.color = colorForId(id);
    m_points << point;
    m_latestPositions.insert(id, pos);
    repaint();
    return false;
}

bool TouchPointsEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    auto it = m_latestPositions.constFind(id);
    if (it != m_latestPositions.constEnd()) {
        TouchPoint point;
        point.pos = it.value();
        point.press = false;
        point.color = colorForId(id);
        m_points << point;
    }
    return false;
}

void TouchPointsEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    int time = 0;
    if (m_lastPresentTime.count()) {
        time = (presentTime - m_lastPresentTime).count();
    }

    auto it = m_points.begin();
    while (it != m_points.end()) {
        it->time += time;
        if (it->time > m_ringLife) {
            it = m_points.erase(it);
        } else {
            it++;
        }
    }

    if (m_points.isEmpty()) {
        m_lastPresentTime = std::chrono::milliseconds::zero();
    } else {
        m_lastPresentTime = presentTime;
    }

    effects->prePaintScreen(data, presentTime);
}

void TouchPointsEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    if (effects->isOpenGLCompositing()) {
        paintScreenSetupGl(renderTarget, viewport.projectionMatrix());
    }
    for (auto it = m_points.constBegin(), end = m_points.constEnd(); it != end; ++it) {
        for (int i = 0; i < m_ringCount; ++i) {
            float alpha = computeAlpha(it->time, i);
            float size = computeRadius(it->time, it->press, i);
            if (size > 0 && alpha > 0) {
                QColor color = it->color;
                color.setAlphaF(alpha);
                drawCircle(viewport, color, it->pos.x(), it->pos.y(), size);
            }
        }
    }
    if (effects->isOpenGLCompositing()) {
        paintScreenFinishGl();
    }
}

void TouchPointsEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

float TouchPointsEffect::computeRadius(int time, bool press, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    if (press) {
        return ((time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
    }
    return ((m_ringLife - time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
}

float TouchPointsEffect::computeAlpha(int time, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    return (m_ringLife - (float)time - ringDistance * (ring)) / m_ringLife;
}

void TouchPointsEffect::repaint()
{
    if (!m_points.isEmpty()) {
        QRegion dirtyRegion;
        const int radius = m_ringMaxSize + m_lineWidth;
        for (auto it = m_points.constBegin(), end = m_points.constEnd(); it != end; ++it) {
            dirtyRegion += QRect(it->pos.x() - radius, it->pos.y() - radius, 2 * radius, 2 * radius);
        }
        effects->addRepaint(dirtyRegion);
    }
}

bool TouchPointsEffect::isActive() const
{
    return !m_points.isEmpty();
}

void TouchPointsEffect::drawCircle(const RenderViewport &viewport, const QColor &color, float cx, float cy, float r)
{
    if (effects->isOpenGLCompositing()) {
        drawCircleGl(viewport, color, cx, cy, r);
    } else if (effects->compositingType() == QPainterCompositing) {
        drawCircleQPainter(color, cx, cy, r);
    }
}

void TouchPointsEffect::drawCircleGl(const RenderViewport &viewport, const QColor &color, float cx, float cy, float r)
{
    static const int num_segments = 80;
    static const float theta = 2 * 3.1415926 / float(num_segments);
    static const float c = cosf(theta); // precalculate the sine and cosine
    static const float s = sinf(theta);
    const auto scale = viewport.scale();
    float t;

    float x = r; // we start at angle = 0
    float y = 0;

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    ShaderManager::instance()->getBoundShader()->setUniform(GLShader::ColorUniform::Color, color);
    QList<QVector2D> verts;
    verts.reserve(num_segments);

    for (int ii = 0; ii < num_segments; ++ii) {
        verts.push_back(QVector2D((x + cx) * scale, (y + cy) * scale));
        // apply the rotation matrix
        t = x;
        x = c * x - s * y;
        y = s * t + c * y;
    }
    vbo->setVertices(verts);
    vbo->render(GL_LINE_LOOP);
}

void TouchPointsEffect::drawCircleQPainter(const QColor &color, float cx, float cy, float r)
{
    QPainter *painter = effects->scenePainter();
    painter->save();
    painter->setPen(color);
    painter->drawArc(cx - r, cy - r, r * 2, r * 2, 0, 5760);
    painter->restore();
}

void TouchPointsEffect::paintScreenSetupGl(const RenderTarget &renderTarget, const QMatrix4x4 &projectionMatrix)
{
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projectionMatrix);
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

    glLineWidth(m_lineWidth);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TouchPointsEffect::paintScreenFinishGl()
{
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

} // namespace

#include "moc_touchpoints.cpp"
