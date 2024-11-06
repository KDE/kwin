/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "regionscreencastsource.h"
#include "screencastutils.h"

#include "cursor.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include <compositor.h>
#include <core/output.h>
#include <drm_fourcc.h>
#include <scene/workspacescene.h>
#include <workspace.h>

#include <QPainter>

namespace KWin
{

RegionScreenCastScrapper::RegionScreenCastScrapper(RegionScreenCastSource *source, Output *output)
    : m_source(source)
    , m_output(output)
{
    connect(output, &Output::enabledChanged, this, [this]() {
        if (!m_output->isEnabled()) {
            m_source->close();
        }
    });

    connect(output, &Output::geometryChanged, this, [this]() {
        m_source->close();
    });

    connect(output, &Output::outputChange, this, [this](const QRegion &damage) {
        if (!damage.isEmpty()) {
            m_source->update(m_output, damage);
        }
    });
}

RegionScreenCastSource::RegionScreenCastSource(const QRect &region, qreal scale, QObject *parent)
    : ScreenCastSource(parent)
    , m_region(region)
    , m_scale(scale)
{
    Q_ASSERT(m_region.isValid());
    Q_ASSERT(m_scale > 0);
}

RegionScreenCastSource::~RegionScreenCastSource()
{
    pause();
}

QSize RegionScreenCastSource::textureSize() const
{
    return m_region.size() * m_scale;
}

qreal RegionScreenCastSource::devicePixelRatio() const
{
    return m_scale;
}

quint32 RegionScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

void RegionScreenCastSource::update(Output *output, const QRegion &damage)
{
    blit(output);

    const QRegion effectiveDamage = damage
                                        .translated(-m_region.topLeft())
                                        .intersected(m_region);
    const QRegion nativeDamage = scaleRegion(effectiveDamage, m_scale);
    Q_EMIT frame(nativeDamage);
}

std::chrono::nanoseconds RegionScreenCastSource::clock() const
{
    return m_last;
}

void RegionScreenCastSource::blit(Output *output)
{
    m_last = output->renderLoop()->lastPresentationTimestamp();

    if (m_renderedTexture) {
        const auto [outputTexture, colorDescription] = Compositor::self()->scene()->textureForOutput(output);
        const auto outputGeometry = output->geometry();
        if (!outputTexture) {
            return;
        }

        GLFramebuffer::pushFramebuffer(m_target.get());

        ShaderBinder shaderBinder(ShaderTrait::MapTexture | ShaderTrait::ApplyColorPipeline);
        QMatrix4x4 projectionMatrix;
        projectionMatrix.scale(1, -1);
        projectionMatrix.ortho(m_region);
        projectionMatrix.translate(outputGeometry.left(), outputGeometry.top());

        shaderBinder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projectionMatrix);
        shaderBinder.shader()->setColorPipelineUniforms(colorDescription, ColorDescription::sRGB, RenderingIntent::RelativeColorimetricWithBPC);

        outputTexture->render(outputGeometry.size());
        GLFramebuffer::popFramebuffer();
    }
}

void RegionScreenCastSource::ensureTexture()
{
    if (!m_renderedTexture) {
        m_renderedTexture = GLTexture::allocate(GL_RGBA8, textureSize());
        if (!m_renderedTexture) {
            return;
        }
        m_renderedTexture->setContentTransform(OutputTransform::FlipY);
        m_renderedTexture->setFilter(GL_LINEAR);
        m_renderedTexture->setWrapMode(GL_CLAMP_TO_EDGE);

        m_target = std::make_unique<GLFramebuffer>(m_renderedTexture.get());
        const auto allOutputs = workspace()->outputs();
        for (auto output : allOutputs) {
            if (output->geometry().intersects(m_region)) {
                blit(output);
            }
        }
    }
}

void RegionScreenCastSource::render(GLFramebuffer *target)
{
    ensureTexture();

    GLFramebuffer::pushFramebuffer(target);
    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.scale(1, -1);
    projectionMatrix.ortho(QRect(QPoint(), target->size()));
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projectionMatrix);

    m_renderedTexture->render(target->size());

    ShaderManager::instance()->popShader();
    GLFramebuffer::popFramebuffer();
}

void RegionScreenCastSource::render(QImage *target)
{
    ensureTexture();
    grabTexture(m_renderedTexture.get(), target);
}

uint RegionScreenCastSource::refreshRate() const
{
    uint ret = 0;
    const auto allOutputs = workspace()->outputs();
    for (auto output : allOutputs) {
        if (output->geometry().intersects(m_region)) {
            ret = std::max<uint>(ret, output->refreshRate());
        }
    }
    return ret;
}

void RegionScreenCastSource::close()
{
    if (!m_closed) {
        m_closed = true;
        Q_EMIT closed();
    }
}

void RegionScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    m_scrappers.clear();
    m_active = false;
}

void RegionScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        if (output->geometry().intersects(m_region)) {
            m_scrappers.emplace_back(std::make_unique<RegionScreenCastScrapper>(this, output));
        }
    }

    if (m_scrappers.empty()) {
        close();
        return;
    }

    Compositor::self()->scene()->addRepaint(m_region);

    m_active = true;
}

bool RegionScreenCastSource::includesCursor(Cursor *cursor) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }

    return cursor->geometry().intersects(m_region);
}

QPointF RegionScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return point - m_region.topLeft();
}

QRectF RegionScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-m_region.topLeft());
}

} // namespace KWin

#include "moc_regionscreencastsource.cpp"
