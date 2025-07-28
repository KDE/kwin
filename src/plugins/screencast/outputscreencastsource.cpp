/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputscreencastsource.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "opengl/eglbackend.h"
#include "opengl/egldisplay.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"
#include "screencastlayer.h"
#include "workspace.h"

#include <drm_fourcc.h>

namespace KWin
{

OutputScreenCastSource::OutputScreenCastSource(Output *output, QObject *parent)
    : ScreenCastSource(parent)
    , m_output(output)
    , m_layer(std::make_unique<ScreencastLayer>(output, static_cast<EglBackend *>(Compositor::self()->backend())->openglContext()->displayObject()->nonExternalOnlySupportedDrmFormats()))
    , m_sceneView(std::make_unique<SceneView>(Compositor::self()->scene(), output, m_layer.get()))
    , m_cursorView(std::make_unique<ItemTreeView>(m_sceneView.get(), Compositor::self()->scene()->cursorItem(), output, nullptr))
{
    updateView();
    connect(output, &Output::changed, this, &OutputScreenCastSource::updateView);
    // prevent the layer from scheduling frames on the actual output
    m_layer->setRenderLoop(nullptr);
    // always hide the cursor from the primary view
    m_cursorView->setExclusive(true);
    connect(workspace(), &Workspace::outputRemoved, this, [this](Output *output) {
        if (m_output == output) {
            Q_EMIT closed();
        }
    });
}

OutputScreenCastSource::~OutputScreenCastSource()
{
    pause();
}

quint32 OutputScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

QSize OutputScreenCastSource::textureSize() const
{
    return m_output->pixelSize();
}

qreal OutputScreenCastSource::devicePixelRatio() const
{
    return m_output->scale();
}

QRegion OutputScreenCastSource::render(QImage *target, const QRegion &bufferRepair)
{
    auto texture = GLTexture::allocate(GL_RGBA8, target->size());
    if (!texture) {
        return QRegion{};
    }
    GLFramebuffer buffer(texture.get());
    const QRegion ret = render(&buffer, infiniteRegion());
    grabTexture(texture.get(), target);
    return ret;
}

QRegion OutputScreenCastSource::render(GLFramebuffer *target, const QRegion &bufferRepair)
{
    m_layer->setFramebuffer(target, scaleRegion(bufferRepair, 1.0 / devicePixelRatio(), QRect(QPoint(), m_sceneView->viewport().size().toSize())));
    if (!m_layer->preparePresentationTest()) {
        return QRegion{};
    }
    const auto beginInfo = m_layer->beginFrame();
    if (!beginInfo) {
        return QRegion{};
    }
    const auto logicalDamage = m_layer->repaints() | m_sceneView->prePaint();
    const auto repaints = beginInfo->repaint | logicalDamage;
    m_layer->resetRepaints();
    m_sceneView->paint(beginInfo->renderTarget, repaints);
    m_sceneView->postPaint();
    if (!m_layer->endFrame(repaints, logicalDamage, nullptr)) {
        return QRegion{};
    }
    return scaleRegion(logicalDamage, devicePixelRatio(), QRect(QPoint(), textureSize()));
}

std::chrono::nanoseconds OutputScreenCastSource::clock() const
{
    return m_output->renderLoop()->lastPresentationTimestamp();
}

uint OutputScreenCastSource::refreshRate() const
{
    return m_output->refreshRate();
}

void OutputScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    connect(m_layer.get(), &OutputLayer::repaintScheduled, this, &OutputScreenCastSource::frame);
    Q_EMIT frame();

    m_active = true;
}

void OutputScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    disconnect(m_layer.get(), &OutputLayer::repaintScheduled, this, &OutputScreenCastSource::frame);

    m_active = false;
}

bool OutputScreenCastSource::includesCursor(Cursor *cursor) const
{
    return cursor->isOnOutput(m_output) && m_cursorView->isVisible();
}

QPointF OutputScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return m_output->mapFromGlobal(point);
}

QRectF OutputScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return m_output->mapFromGlobal(rect);
}

void OutputScreenCastSource::updateView()
{
    m_sceneView->setViewport(m_output->geometryF());
    m_sceneView->setScale(m_output->scale());
}

} // namespace KWin

#include "moc_outputscreencastsource.cpp"
