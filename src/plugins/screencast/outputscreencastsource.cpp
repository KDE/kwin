/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputscreencastsource.h"
#include "filteredsceneview.h"
#include "screencastlayer.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderdevice.h"
#include "cursor.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <drm_fourcc.h>

namespace KWin
{

OutputScreenCastSource::OutputScreenCastSource(LogicalOutput *output, std::optional<pid_t> pidToHide)
    : ScreenCastSource()
    , m_output(output)
    , m_pidToHide(pidToHide)
{
    connect(workspace(), &Workspace::outputRemoved, this, [this](LogicalOutput *output) {
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

void OutputScreenCastSource::setRenderCursor(bool enable)
{
    m_renderCursor = enable;
    if (m_cursorView) {
        m_cursorView->setExclusive(!enable);
    }
}

void OutputScreenCastSource::setNextPresentationTimestamp(std::chrono::nanoseconds timestamp, uint32_t refreshRate)
{
    m_sceneView->setNextPresentationTimestamp(timestamp, refreshRate, nullptr);
}

std::optional<ScreenCastSource::RenderResult> OutputScreenCastSource::render(GraphicsBuffer *buffer, const Region &bufferRepair)
{
    m_sceneView->prePaint();
    const auto _ = qScopeGuard([this]() {
        m_sceneView->postPaint();
    });

    m_layer->setFramebuffer(buffer, bufferRepair & Rect(QPoint(), buffer->size()));
    if (!m_layer->preparePresentationTest()) {
        return std::nullopt;
    }
    const auto beginInfo = m_layer->beginFrame();
    if (!beginInfo) {
        return std::nullopt;
    }
    const auto bufferDamage = m_sceneView->collectDamage() & Rect(QPoint(), buffer->size());
    m_layer->resetRepaints();
    FileDescriptor sync = m_sceneView->paint(*beginInfo, QPoint(), bufferDamage, nullptr);
    if (!m_layer->endFrame(bufferDamage, sync.duplicate(), nullptr)) {
        return std::nullopt;
    }

    return RenderResult{bufferDamage, std::move(sync)};
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

    m_layer = std::make_unique<ScreencastLayer>(m_output, Compositor::self()->backend()->supportedFormats());

    m_sceneView = std::make_unique<FilteredSceneView>(kwinApp()->scene(), m_output, m_layer.get(), m_renderDevice, m_pidToHide);
    m_sceneView->setViewport(m_output->geometryF());
    m_sceneView->setScale(m_output->scale());
    m_sceneView->setRefreshRate(m_output->refreshRate());
    connect(m_output, &LogicalOutput::changed, m_sceneView.get(), [this]() {
        m_sceneView->setViewport(m_output->geometryF());
        m_sceneView->setScale(m_output->scale());
        m_sceneView->setRefreshRate(m_output->refreshRate());
    });

    m_cursorView = std::make_unique<ItemTreeView>(m_sceneView.get(), kwinApp()->scene()->cursorItem(), m_output, nullptr, nullptr);
    m_cursorView->setExclusive(!m_renderCursor);

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

    m_cursorView.reset();
    m_sceneView.reset();
    m_layer.reset();

    m_active = false;
}

bool OutputScreenCastSource::includesCursor(Cursor *cursor) const
{
    return cursor->isOnOutput(m_output);
}

QPointF OutputScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return m_output->mapFromGlobal(point);
}

RectF OutputScreenCastSource::mapFromGlobal(const RectF &rect) const
{
    return m_output->mapFromGlobal(rect);
}

bool OutputScreenCastSource::followsStreamSize()
{
    return m_output->canResize();
}

void OutputScreenCastSource::resize(const QSize &size)
{
    if (m_output->pixelSize() == size) {
        return;
    }
    m_output->resize(size);
}

} // namespace KWin

#include "moc_outputscreencastsource.cpp"
