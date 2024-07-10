/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"
#include "renderloop_p.h"
#include "scene/surfaceitem.h"
#include "syncobjtimeline.h"

#include <QCoreApplication>
#include <QThread>
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

OutputFrame::OutputFrame(RenderLoop *loop, std::chrono::nanoseconds refreshDuration)
    : m_loop(loop)
    , m_refreshDuration(refreshDuration)
    , m_targetPageflipTime(loop->nextPresentationTimestamp())
    , m_predictedRenderTime(loop->predictedRenderTime())
{
}

OutputFrame::~OutputFrame()
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    if (!m_presented && m_loop) {
        RenderLoopPrivate::get(m_loop)->notifyFrameDropped();
    }
}

void OutputFrame::addFeedback(std::unique_ptr<PresentationFeedback> &&feedback)
{
    m_feedbacks.push_back(std::move(feedback));
}

std::optional<RenderTimeSpan> OutputFrame::queryRenderTime() const
{
    if (m_renderTimeQueries.empty()) {
        return RenderTimeSpan{};
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
    return ret;
}

void OutputFrame::presented(std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    Q_ASSERT(!m_presented);
    m_presented = true;

    const auto renderTime = queryRenderTime();
    if (m_loop) {
        RenderLoopPrivate::get(m_loop)->notifyFrameCompleted(timestamp, renderTime, mode, this);
    }
    for (const auto &feedback : m_feedbacks) {
        feedback->presented(m_refreshDuration, timestamp, mode);
    }
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

std::chrono::steady_clock::time_point OutputFrame::targetPageflipTime() const
{
    return m_targetPageflipTime;
}

std::chrono::nanoseconds OutputFrame::refreshDuration() const
{
    return m_refreshDuration;
}

std::chrono::nanoseconds OutputFrame::predictedRenderTime() const
{
    return m_predictedRenderTime;
}

std::optional<double> OutputFrame::brightness() const
{
    return m_brightness;
}

void OutputFrame::setBrightness(double brightness)
{
    m_brightness = brightness;
}

std::optional<double> OutputFrame::artificialHdrHeadroom() const
{
    return m_artificialHdrHeadroom;
}

void OutputFrame::setArtificialHdrHeadroom(double edr)
{
    m_artificialHdrHeadroom = edr;
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

void RenderBackend::repairPresentation(Output *output)
{
}

} // namespace KWin

#include "moc_renderbackend.cpp"
