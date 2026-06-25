/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_commit.h"
#include "core/renderbackend.h"
#include "drm_blob.h"
#include "drm_buffer.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_object.h"
#include "drm_output.h"
#include "drm_property.h"

#include <QCoreApplication>
#include <QThread>
#include <set>

#ifndef DRM_MODE_ATOMIC_FAILURE_STRING_LEN
#define DRM_MODE_ATOMIC_FAILURE_STRING_LEN 128
struct drm_mode_atomic_err_code
{
    __u64 failure_code;
    __u64 failure_objs_ptr;
    __u64 reserved;
    __u32 count_objs;
    char failure_string[DRM_MODE_ATOMIC_FAILURE_STRING_LEN];
};
enum drm_mode_atomic_failure_codes {
    DRM_MODE_ATOMIC_UNSPECIFIED_ERROR,
    DRM_MODE_ATOMIC_INVALID_API_USAGE,
    DRM_MODE_ATOMIC_NEED_FULL_MODESET,
    DRM_MODE_ATOMIC_ASYNC_PROP_CHANGED,
    DRM_MODE_ATOMIC_SCANOUT_BW,
    DRM_MODE_ATTOMIC_CONNECTOR_BW,
    DRM_MODE_ATTOMIC_PIPE_BW,
    DRM_MODE_ATOMIC_MEMORY_DOMAIN,
    DRM_MODE_ATOMIC_SPEC_VIOLOATION,
};
#endif

using namespace std::chrono_literals;

namespace KWin
{

DrmCommit::DrmCommit(DrmGpu *gpu)
    : m_gpu(gpu)
{
}

DrmCommit::~DrmCommit()
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
}

DrmGpu *DrmCommit::gpu() const
{
    return m_gpu;
}

void DrmCommit::setDefunct()
{
    m_defunct = true;
}

std::expected<void, OutputError> DrmCommit::errnoToError()
{
    switch (errno) {
    case EINVAL:
        return std::unexpected(OutputError{
            .code = OutputErrorCode::Other,
            .message = QStringLiteral("EINVAL"),
        });
    case EBUSY:
        return std::unexpected(OutputError{
            .code = OutputErrorCode::InvalidApiUsage,
            .message = QStringLiteral("EBUSY"),
        });
    case EACCES:
        return std::unexpected(OutputError{
            .code = OutputErrorCode::NoPermission,
            .message = QStringLiteral("EACCESS"),
        });
    default:
        return std::unexpected(OutputError{
            .code = OutputErrorCode::Other,
            .message = QString::fromLatin1(strerror(errno)),
        });
    }
}

DrmAtomicCommit::DrmAtomicCommit(DrmGpu *gpu, const QList<DrmPipeline *> &pipelines)
    : DrmCommit(gpu)
    , m_pipelines(pipelines)
{
}

void DrmAtomicCommit::addProperty(const DrmProperty &prop, uint64_t value)
{
    if (Q_UNLIKELY(!prop.isValid())) {
        qCWarning(KWIN_DRM) << "Trying to add an invalid property" << prop.name();
        return;
    }
    prop.checkValueInRange(value);
    m_properties[prop.drmObject()->id()][prop.propId()] = value;
}

void DrmAtomicCommit::addBlob(const DrmProperty &prop, const std::shared_ptr<DrmBlob> &blob)
{
    addProperty(prop, blob ? blob->blobId() : 0);
    m_blobs[&prop] = blob;
}

void DrmAtomicCommit::addBuffer(DrmPlane *plane, const std::shared_ptr<DrmFramebuffer> &buffer, const std::shared_ptr<OutputFrame> &frame)
{
    addProperty(plane->fbId, buffer ? buffer->framebufferId() : 0);
    m_buffers[plane] = buffer;
    m_frames[plane] = frame;
    // atomic commits with IN_FENCE_FD fail with NVidia and (as of kernel 6.9) with tearing
    if (plane->inFenceFd.isValid() && !plane->gpu()->drmDevice()->isNvidia() && !isTearing()) {
        addProperty(plane->inFenceFd, buffer ? buffer->syncFd().get() : -1);
    }
    m_planes.emplace(plane);
    if (frame) {
        if (m_targetPageflipTime) {
            m_targetPageflipTime = std::min(*m_targetPageflipTime, frame->targetPageflipTime());
        } else {
            m_targetPageflipTime = frame->targetPageflipTime();
        }
    }
}

void DrmAtomicCommit::setVrr(DrmCrtc *crtc, bool vrr)
{
    addProperty(crtc->vrrEnabled, vrr ? 1 : 0);
    m_vrr = vrr;
}

void DrmAtomicCommit::setPresentationMode(PresentationMode mode)
{
    m_mode = mode;
}

std::expected<void, OutputError> DrmAtomicCommit::test()
{
    uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_NONBLOCK;
    if (isTearing()) {
        flags |= DRM_MODE_PAGE_FLIP_ASYNC;
    }
    return doCommit(flags);
}

std::expected<void, OutputError> DrmAtomicCommit::testAllowModeset()
{
    return doCommit(DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET);
}

std::expected<void, OutputError> DrmAtomicCommit::commit()
{
    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;
    if (isTearing()) {
        flags |= DRM_MODE_PAGE_FLIP_ASYNC;
    }
    return doCommit(flags);
}

std::expected<void, OutputError> DrmAtomicCommit::commitModeset()
{
    m_modeset = true;
    return doCommit(DRM_MODE_ATOMIC_ALLOW_MODESET);
}

static const std::map<uint64_t, OutputErrorCode> s_errorCodeMap = {
    std::make_pair(DRM_MODE_ATOMIC_UNSPECIFIED_ERROR, OutputErrorCode::Other),
    std::make_pair(DRM_MODE_ATOMIC_INVALID_API_USAGE, OutputErrorCode::InvalidApiUsage),
    std::make_pair(DRM_MODE_ATOMIC_NEED_FULL_MODESET, OutputErrorCode::NeedsModeset),
    std::make_pair(DRM_MODE_ATOMIC_ASYNC_PROP_CHANGED, OutputErrorCode::AsyncFlipFailure),
    std::make_pair(DRM_MODE_ATOMIC_SCANOUT_BW, OutputErrorCode::ScanoutBandwidth),
    std::make_pair(DRM_MODE_ATTOMIC_CONNECTOR_BW, OutputErrorCode::ConnectorBandwidth),
    std::make_pair(DRM_MODE_ATTOMIC_PIPE_BW, OutputErrorCode::PipeBandwidth),
    std::make_pair(DRM_MODE_ATOMIC_MEMORY_DOMAIN, OutputErrorCode::MemoryDomain),
    std::make_pair(DRM_MODE_ATOMIC_SPEC_VIOLOATION, OutputErrorCode::OtherHardwareLimitation),
};

std::expected<void, OutputError> DrmAtomicCommit::doCommit(uint32_t flags)
{
    std::vector<uint32_t> objects;
    std::vector<uint32_t> propertyCounts;
    std::vector<uint32_t> propertyIds;
    std::vector<uint64_t> values;
    objects.reserve(m_properties.size());
    propertyCounts.reserve(m_properties.size());
    uint64_t totalPropertiesCount = 0;
    for (const auto &[object, properties] : m_properties) {
        objects.push_back(object);
        propertyCounts.push_back(properties.size());
        totalPropertiesCount += properties.size();
    }
    propertyIds.reserve(totalPropertiesCount);
    values.reserve(totalPropertiesCount);
    for (const auto &[object, properties] : m_properties) {
        for (const auto &[property, value] : properties) {
            propertyIds.push_back(property);
            values.push_back(value);
        }
    }
    drm_mode_atomic commitData{
        .flags = flags,
        .count_objs = uint32_t(objects.size()),
        .objs_ptr = reinterpret_cast<uint64_t>(objects.data()),
        .count_props_ptr = reinterpret_cast<uint64_t>(propertyCounts.data()),
        .props_ptr = reinterpret_cast<uint64_t>(propertyIds.data()),
        .prop_values_ptr = reinterpret_cast<uint64_t>(values.data()),
        .reserved = 0,
        .user_data = reinterpret_cast<uint64_t>(m_gpu),
    };
    std::unique_lock<std::mutex> lock;
    if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
        lock = m_gpu->lockPendingCommits();
    }
    const bool success = drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_ATOMIC, &commitData) == 0;
    if (success) {
        if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
            for (const DrmPipeline *pipeline : m_pipelines) {
                m_gpu->registerPendingCommit(lock, pipeline->crtc()->id(), this);
            }
        }
        return {};
    }
    drm_mode_atomic_err_code error{};
    if (m_gpu->commitFeedbackSupported()) {
        commitData.reserved = reinterpret_cast<uint64_t>(&error);
    }
    if (drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_ATOMIC, &commitData) == 0) {
        return {};
    }
    if (!m_gpu->commitFeedbackSupported()) {
        return errnoToError();
    }
    OutputErrorCode err = OutputErrorCode::Other;
    const auto it = s_errorCodeMap.find(error.failure_code);
    if (it != s_errorCodeMap.end()) {
        err = it->second;
    }

    // FIXME this method may be called from the commit thread, this is *not* safe atm! Options:
    // - make DrmGpu lock all commit threads when modifying its lists (should be pretty easy)
    // - return a different struct with object ids, and convert to OutputError in DrmPipeline
    const auto pipelines = m_gpu->pipelines();
    QList<BackendOutput *> outputs;

    // FIXME who allocates and frees the memory of this list?
    for (uint32_t obj : std::span(reinterpret_cast<uint32_t *>(error.failure_objs_ptr), error.count_objs)) {
        const auto it = std::ranges::find(pipelines, obj, [](DrmPipeline *pipeline) {
            return pipeline->connector()->id();
        });
        if (it != pipelines.end()) {
            outputs.push_back((*it)->output());
        }
    }
    return std::unexpected(OutputError{
        .code = err,
        .message = QString::fromLatin1(error.failure_string),
        .outputs = outputs,
    });
}

void DrmAtomicCommit::pageFlipped(std::chrono::nanoseconds timestamp)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    for (const auto &[plane, buffer] : m_buffers) {
        plane->setCurrentBuffer(buffer);
    }
    if (m_defunct) {
        return;
    }
    // de-duplicate frames, so that two planes committed
    // together don't cause problems
    std::set<OutputFrame *> frames;
    for (const auto &[plane, frame] : m_frames) {
        if (frame) {
            frames.insert(frame.get());
        }
    }
    for (const auto &frame : frames) {
        frame->presented(timestamp, m_mode);
    }
    m_frames.clear();
    // Iterate a copy: pageFlipped() can reset DrmCommitThread::m_committed, which
    // destroys this commit - and with it m_pipelines - mid-loop.
    const auto pipelines = m_pipelines;
    for (const auto pipeline : pipelines) {
        pipeline->pageFlipped(timestamp);
    }
}

bool DrmAtomicCommit::areBuffersReadable() const
{
    return std::ranges::all_of(m_buffers, [](const auto &pair) {
        const auto &[plane, buffer] = pair;
        return !buffer || buffer->isReadable();
    });
}

void DrmAtomicCommit::setDeadline(std::chrono::steady_clock::time_point deadline)
{
    for (const auto &[plane, buffer] : m_buffers) {
        if (buffer) {
            buffer->setDeadline(deadline);
        }
    }
}

std::optional<bool> DrmAtomicCommit::isVrr() const
{
    return m_vrr;
}

const std::unordered_set<DrmPlane *> &DrmAtomicCommit::modifiedPlanes() const
{
    return m_planes;
}

void DrmAtomicCommit::merge(DrmAtomicCommit *onTop)
{
    for (const auto &[obj, properties] : onTop->m_properties) {
        auto &ownProperties = m_properties[obj];
        for (const auto &[prop, value] : properties) {
            ownProperties[prop] = value;
        }
    }
    for (const auto &[plane, buffer] : onTop->m_buffers) {
        m_buffers[plane] = buffer;
        m_frames[plane] = onTop->m_frames[plane];
        m_planes.emplace(plane);
    }
    for (const auto &[prop, blob] : onTop->m_blobs) {
        m_blobs[prop] = blob;
    }
    if (onTop->m_vrr) {
        m_vrr = onTop->m_vrr;
    }
    if (!m_targetPageflipTime) {
        m_targetPageflipTime = onTop->m_targetPageflipTime;
    } else if (onTop->m_targetPageflipTime) {
        *m_targetPageflipTime = std::min(*m_targetPageflipTime, *onTop->m_targetPageflipTime);
    }
    if (m_allowedVrrDelay && onTop->m_allowedVrrDelay) {
        *m_allowedVrrDelay = std::min(*m_allowedVrrDelay, *onTop->m_allowedVrrDelay);
    } else {
        m_allowedVrrDelay.reset();
    }
}

void DrmAtomicCommit::setAllowedVrrDelay(std::optional<std::chrono::nanoseconds> allowedDelay)
{
    m_allowedVrrDelay = allowedDelay;
}

std::optional<std::chrono::nanoseconds> DrmAtomicCommit::allowedVrrDelay() const
{
    return m_allowedVrrDelay;
}

std::optional<std::chrono::steady_clock::time_point> DrmAtomicCommit::targetPageflipTime() const
{
    return m_targetPageflipTime;
}

bool DrmAtomicCommit::isReadyFor(std::chrono::steady_clock::time_point pageflipTarget) const
{
    static constexpr auto s_pageflipSlop = 500us;
    return (!m_targetPageflipTime || pageflipTarget + s_pageflipSlop >= *m_targetPageflipTime) && areBuffersReadable();
}

bool DrmAtomicCommit::isTearing() const
{
    return m_mode == PresentationMode::Async || m_mode == PresentationMode::AdaptiveAsync;
}

DrmLegacyCommit::DrmLegacyCommit(DrmPipeline *pipeline, const std::shared_ptr<DrmFramebuffer> &buffer, const std::shared_ptr<OutputFrame> &frame)
    : DrmCommit(pipeline->gpu())
    , m_pipeline(pipeline)
    , m_crtc(m_pipeline->crtc())
    , m_buffer(buffer)
    , m_frame(frame)
{
}

bool DrmLegacyCommit::doModeset(DrmConnector *connector, DrmConnectorMode *mode)
{
    uint32_t connectorId = connector->id();
    if (drmModeSetCrtc(gpu()->fd(), m_crtc->id(), m_buffer->framebufferId(), 0, 0, &connectorId, 1, mode->nativeMode()) == 0) {
        m_crtc->setCurrent(m_buffer);
        return true;
    } else {
        return false;
    }
}

bool DrmLegacyCommit::doPageflip(PresentationMode mode)
{
    m_mode = mode;
    uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT;
    if (mode == PresentationMode::Async || mode == PresentationMode::AdaptiveAsync) {
        flags |= DRM_MODE_PAGE_FLIP_ASYNC;
    }
    auto lock = gpu()->lockPendingCommits();
    const bool success = drmModePageFlip(gpu()->fd(), m_crtc->id(), m_buffer->framebufferId(), flags, gpu()) == 0;
    if (success) {
        gpu()->registerPendingCommit(lock, m_crtc->id(), this);
    }
    return success;
}

void DrmLegacyCommit::pageFlipped(std::chrono::nanoseconds timestamp)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    m_crtc->setCurrent(m_buffer);
    if (m_defunct) {
        return;
    }
    if (m_frame) {
        m_frame->presented(timestamp, m_mode);
        m_frame.reset();
    }
    m_pipeline->pageFlipped(timestamp);
}

}
