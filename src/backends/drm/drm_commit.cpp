/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_commit.h"
#include "drm_blob.h"
#include "drm_buffer.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_object.h"
#include "drm_property.h"

#include <QApplication>
#include <QThread>

namespace KWin
{

DrmCommit::DrmCommit(DrmGpu *gpu)
    : m_gpu(gpu)
{
}

DrmCommit::~DrmCommit()
{
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());
}

DrmGpu *DrmCommit::gpu() const
{
    return m_gpu;
}

DrmAtomicCommit::DrmAtomicCommit(const QList<DrmPipeline *> &pipelines)
    : DrmCommit(pipelines.front()->gpu())
    , m_pipelines(pipelines)
{
}

void DrmAtomicCommit::addProperty(const DrmProperty &prop, uint64_t value)
{
    m_properties[prop.drmObject()->id()][prop.propId()] = value;
}

void DrmAtomicCommit::addBlob(const DrmProperty &prop, const std::shared_ptr<DrmBlob> &blob)
{
    addProperty(prop, blob ? blob->blobId() : 0);
    m_blobs[&prop] = blob;
}

void DrmAtomicCommit::addBuffer(DrmPlane *plane, const std::shared_ptr<DrmFramebuffer> &buffer)
{
    addProperty(plane->fbId, buffer ? buffer->framebufferId() : 0);
    m_buffers[plane] = buffer;
    m_planes.emplace(plane);
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

bool DrmAtomicCommit::test()
{
    return doCommit(DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_NONBLOCK);
}

bool DrmAtomicCommit::testAllowModeset()
{
    return doCommit(DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET);
}

bool DrmAtomicCommit::commit()
{
    return doCommit(DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT);
}

bool DrmAtomicCommit::commitModeset()
{
    m_modeset = true;
    return doCommit(DRM_MODE_ATOMIC_ALLOW_MODESET);
}

bool DrmAtomicCommit::doCommit(uint32_t flags)
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
        .user_data = reinterpret_cast<uint64_t>(this),
    };
    return drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_ATOMIC, &commitData) == 0;
}

void DrmAtomicCommit::pageFlipped(std::chrono::nanoseconds timestamp) const
{
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());
    for (const auto &[plane, buffer] : m_buffers) {
        plane->setCurrentBuffer(buffer);
    }
    DrmPipeline::PageflipType type = DrmPipeline::PageflipType::Normal;
    if (m_modeset) {
        type = DrmPipeline::PageflipType::Modeset;
    } else if (m_cursorOnly) {
        type = DrmPipeline::PageflipType::CursorOnly;
    }
    for (const auto pipeline : std::as_const(m_pipelines)) {
        pipeline->pageFlipped(timestamp, type, m_mode);
    }
}

bool DrmAtomicCommit::areBuffersReadable() const
{
    return std::all_of(m_buffers.begin(), m_buffers.end(), [](const auto &pair) {
        const auto &[plane, buffer] = pair;
        return !buffer || buffer->isReadable();
    });
}

std::optional<bool> DrmAtomicCommit::isVrr() const
{
    return m_vrr.value_or(false);
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
        m_planes.emplace(plane);
    }
    for (const auto &[prop, blob] : onTop->m_blobs) {
        m_blobs[prop] = blob;
    }
    if (onTop->m_vrr) {
        m_vrr = onTop->m_vrr;
    }
    m_cursorOnly &= onTop->isCursorOnly();
}

void DrmAtomicCommit::setCursorOnly(bool cursor)
{
    m_cursorOnly = cursor;
}

bool DrmAtomicCommit::isCursorOnly() const
{
    return m_cursorOnly;
}

DrmLegacyCommit::DrmLegacyCommit(DrmPipeline *pipeline, const std::shared_ptr<DrmFramebuffer> &buffer)
    : DrmCommit(pipeline->gpu())
    , m_pipeline(pipeline)
    , m_buffer(buffer)
{
}

bool DrmLegacyCommit::doModeset(DrmConnector *connector, DrmConnectorMode *mode)
{
    m_modeset = true;
    uint32_t connectorId = connector->id();
    if (drmModeSetCrtc(gpu()->fd(), m_pipeline->crtc()->id(), m_buffer->framebufferId(), 0, 0, &connectorId, 1, mode->nativeMode()) == 0) {
        m_pipeline->crtc()->setCurrent(m_buffer);
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
    return drmModePageFlip(gpu()->fd(), m_pipeline->crtc()->id(), m_buffer->framebufferId(), flags, this) == 0;
}

void DrmLegacyCommit::pageFlipped(std::chrono::nanoseconds timestamp) const
{
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());
    m_pipeline->crtc()->setCurrent(m_buffer);
    m_pipeline->pageFlipped(timestamp, m_modeset ? DrmPipeline::PageflipType::Modeset : DrmPipeline::PageflipType::Normal, m_mode);
}
}
