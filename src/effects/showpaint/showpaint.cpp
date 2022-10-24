/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showpaint.h"

#include <kwinglutils.h>

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QPainter>

namespace KWin
{

static const qreal s_alpha = 0.2;
static const QVector<QColor> s_colors{
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray};

ShowPaintEffect::ShowPaintEffect()
{
    auto *toggleAction = new QAction(this);
    toggleAction->setObjectName(QStringLiteral("Toggle"));
    toggleAction->setText(i18n("Toggle Show Paint"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {});
    KGlobalAccel::self()->setShortcut(toggleAction, {});

    connect(toggleAction, &QAction::triggered, this, &ShowPaintEffect::toggle);
}

void ShowPaintEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_painted = QRegion();
    effects->paintScreen(mask, region, data);
    if (effects->isOpenGLCompositing()) {
        paintGL(data.projectionMatrix(), effects->renderTargetScale());
    } else if (effects->compositingType() == QPainterCompositing) {
        paintQPainter();
    }
    if (++m_colorIndex == s_colors.count()) {
        m_colorIndex = 0;
    }
}

void ShowPaintEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    m_painted |= region;
    effects->paintWindow(w, mask, region, data);
}

void ShowPaintEffect::paintGL(const QMatrix4x4 &projection, qreal scale)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projection);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    vbo->setColor(color);
    QVector<float> verts;
    verts.reserve(m_painted.rectCount() * 12);
    for (const QRect &r : m_painted) {
        const auto deviceRect = scaledRect(r, scale);
        verts << deviceRect.x() + deviceRect.width() << deviceRect.y();
        verts << deviceRect.x() << deviceRect.y();
        verts << deviceRect.x() << deviceRect.y() + deviceRect.height();
        verts << deviceRect.x() << deviceRect.y() + deviceRect.height();
        verts << deviceRect.x() + deviceRect.width() << deviceRect.y() + deviceRect.height();
        verts << deviceRect.x() + deviceRect.width() << deviceRect.y();
    }
    vbo->setData(verts.count() / 2, 2, verts.data(), nullptr);
    vbo->render(GL_TRIANGLES);
    glDisable(GL_BLEND);
}

void ShowPaintEffect::paintQPainter()
{
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    for (const QRect &r : m_painted) {
        effects->scenePainter()->fillRect(r, color);
    }
}

bool ShowPaintEffect::isActive() const
{
    return m_active;
}

void ShowPaintEffect::toggle()
{
    m_active = !m_active;
    effects->addRepaintFull();
}

} // namespace KWin
