/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"
#include "renderloop_p.h"
#include "scene/surfaceitem.h"
#include "syncobjtimeline.h"

#include <drm_fourcc.h>
#include <ranges>

namespace KWin
{

RenderTimeSpan RenderTimeSpan::operator|(const RenderTimeSpan &other) const
{
    return RenderTimeSpan{
        .start = std::min(start, other.start),
        .end = std::max(end, other.end),
    };
}

CpuRenderTimeQuery::CpuRenderTimeQuery()
    : m_start(std::chrono::steady_clock::now())
{
}

void CpuRenderTimeQuery::end()
{
    m_end = std::chrono::steady_clock::now();
}

std::optional<RenderTimeSpan> CpuRenderTimeQuery::query()
{
    Q_ASSERT(m_end);
    return RenderTimeSpan{
        .start = m_start,
        .end = *m_end,
    };
}

OutputFrame::OutputFrame(RenderLoop *loop)
    : m_loop(loop)
{
}

OutputFrame::~OutputFrame() = default;

void OutputFrame::addFeedback(std::unique_ptr<PresentationFeedback> &&feedback)
{
    m_feedbacks.push_back(std::move(feedback));
}

std::optional<std::chrono::nanoseconds> OutputFrame::queryRenderTime() const
{
    if (m_renderTimeQueries.empty()) {
        return std::chrono::nanoseconds::zero();
    }
    const auto first = m_renderTimeQueries.front()->query();
    if (!first) {
        return std::nullopt;
    }
    RenderTimeSpan ret = *first;
    for (const auto &query : m_renderTimeQueries | std::views::drop(1)) {
        const auto opt = query->query();
        if (!opt) {
            return std::nullopt;
        }
        ret = ret | *opt;
    }
    return ret.end - ret.start;
}

void OutputFrame::presented(std::chrono::nanoseconds refreshDuration, std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    std::optional<std::chrono::nanoseconds> renderTime = queryRenderTime();
    RenderLoopPrivate::get(m_loop)->notifyFrameCompleted(timestamp, renderTime, mode);
    for (const auto &feedback : m_feedbacks) {
        feedback->presented(refreshDuration, timestamp, mode);
    }
}

void OutputFrame::failed()
{
    RenderLoopPrivate::get(m_loop)->notifyFrameFailed();
}

void OutputFrame::setContentType(ContentType type)
{
    m_contentType = type;
}

std::optional<ContentType> OutputFrame::contentType() const
{
    return m_contentType;
}

void OutputFrame::setPresentationMode(PresentationMode mode)
{
    m_presentationMode = mode;
}

PresentationMode OutputFrame::presentationMode() const
{
    return m_presentationMode;
}

void OutputFrame::setDamage(const QRegion &region)
{
    m_damage = region;
}

QRegion OutputFrame::damage() const
{
    return m_damage;
}

void OutputFrame::addRenderTimeQuery(std::unique_ptr<RenderTimeQuery> &&query)
{
    m_renderTimeQueries.push_back(std::move(query));
}

RenderBackend::RenderBackend(QObject *parent)
    : QObject(parent)
{
}

OutputLayer *RenderBackend::overlayLayer(Output *output)
{
    return nullptr;
}

OutputLayer *RenderBackend::cursorLayer(Output *output)
{
    return nullptr;
}

OverlayWindow *RenderBackend::overlayWindow() const
{
    return nullptr;
}

bool RenderBackend::checkGraphicsReset()
{
    return false;
}

DrmDevice *RenderBackend::drmDevice() const
{
    return nullptr;
}

bool RenderBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    return false;
}

QHash<uint32_t, QList<uint64_t>> RenderBackend::supportedFormats() const
{
    return QHash<uint32_t, QList<uint64_t>>{{DRM_FORMAT_XRGB8888, QList<uint64_t>{DRM_FORMAT_MOD_LINEAR}}};
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return nullptr;
}

} // namespace KWin

#include "moc_renderbackend.cpp"
