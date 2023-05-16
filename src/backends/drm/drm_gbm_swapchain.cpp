/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_gbm_swapchain.h"

#include <errno.h>
#include <gbm.h>
#include <drm_fourcc.h>

#include "drm_backend.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "kwineglutils_p.h"
#include "libkwineffects/kwinglplatform.h"

namespace KWin
{

GbmSwapchain::GbmSwapchain(DrmGpu *gpu, gbm_bo *initialBuffer, uint32_t flags)
    : m_gpu(gpu)
    , m_size(gbm_bo_get_width(initialBuffer), gbm_bo_get_height(initialBuffer))
    , m_format(gbm_bo_get_format(initialBuffer))
    , m_modifier(gbm_bo_get_modifier(initialBuffer))
    , m_flags(flags)
    , m_buffers({std::make_pair(initialBuffer, 0)})
{
}

GbmSwapchain::~GbmSwapchain()
{
    while (!m_buffers.empty()) {
        gbm_bo_destroy(m_buffers.back().first);
        m_buffers.pop_back();
    }
}

std::pair<std::shared_ptr<GbmBuffer>, RegionF> GbmSwapchain::acquire()
{
    for (auto &[bo, bufferAge] : m_buffers) {
        bufferAge++;
    }
    if (m_buffers.empty()) {
        gbm_bo *newBo;
        if (m_modifier == DRM_FORMAT_MOD_INVALID) {
            newBo = gbm_bo_create(m_gpu->gbmDevice(), m_size.width(), m_size.height(), m_format, m_flags);
        } else {
            newBo = gbm_bo_create_with_modifiers(m_gpu->gbmDevice(), m_size.width(), m_size.height(), m_format, &m_modifier, 1);
        }
        if (!newBo) {
            qCWarning(KWIN_DRM) << "Creating gbm buffer failed!" << strerror(errno);
            return std::make_pair(nullptr, RegionF::infiniteRegion());
        } else {
            return std::make_pair(std::make_shared<GbmBuffer>(newBo, shared_from_this()), RegionF::infiniteRegion());
        }
    } else {
        const auto [bo, bufferAge] = m_buffers.front();
        m_buffers.pop_front();
        return std::make_pair(std::make_shared<GbmBuffer>(bo, shared_from_this()),
                              m_damageJournal.accumulate(bufferAge, RegionF::infiniteRegion()));
    }
}

void GbmSwapchain::damage(const RegionF &damage)
{
    m_damageJournal.add(damage);
}

void GbmSwapchain::resetDamage()
{
    m_damageJournal.clear();
}

void GbmSwapchain::releaseBuffer(GbmBuffer *buffer)
{
    if (m_buffers.size() < 3) {
        m_buffers.push_back(std::make_pair(buffer->bo(), 1));
    } else {
        gbm_bo_destroy(buffer->bo());
    }
}

std::variant<std::shared_ptr<GbmSwapchain>, GbmSwapchain::Error> GbmSwapchain::createSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format, uint32_t flags)
{
    gbm_bo *bo = gbm_bo_create(gpu->gbmDevice(), size.width(), size.height(), format, flags);
    if (bo) {
        return std::make_shared<GbmSwapchain>(gpu, bo, flags);
    } else {
        qCWarning(KWIN_DRM) << "Creating initial gbm buffer failed!" << strerror(errno);
        return Error::Unknown;
    }
}

std::variant<std::shared_ptr<GbmSwapchain>, GbmSwapchain::Error> GbmSwapchain::createSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format, QVector<uint64_t> modifiers)
{
    gbm_bo *bo = gbm_bo_create_with_modifiers(gpu->gbmDevice(), size.width(), size.height(), format, modifiers.constData(), modifiers.size());
    if (bo) {
        // scanout is implicitly assumed with gbm_bo_create_with_modifiers
        return std::make_shared<GbmSwapchain>(gpu, bo, GBM_BO_USE_SCANOUT);
    } else {
        if (errno == ENOSYS) {
            return Error::ModifiersUnsupported;
        } else {
            qCWarning(KWIN_DRM) << "Creating initial gbm buffer failed!" << strerror(errno);
            return Error::Unknown;
        }
    }
}

DrmGpu *GbmSwapchain::gpu() const
{
    return m_gpu;
}

QSize GbmSwapchain::size() const
{
    return m_size;
}

uint32_t GbmSwapchain::format() const
{
    return m_format;
}

uint64_t GbmSwapchain::modifier() const
{
    return m_modifier;
}

uint32_t GbmSwapchain::flags() const
{
    return m_flags;
}
}
