/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QVector>
#include <cstdint>
#include <deque>
#include <epoxy/egl.h>
#include <memory>
#include <variant>

#include "drm_buffer_gbm.h"
#include "utils/damagejournal.h"

struct gbm_device;
struct gbm_surface;

namespace KWin
{

class GLFramebuffer;
class EglGbmBackend;

class GbmSwapchain : public std::enable_shared_from_this<GbmSwapchain>
{
public:
    explicit GbmSwapchain(DrmGpu *gpu, gbm_bo *initialBuffer, uint32_t flags);
    ~GbmSwapchain();

    std::pair<std::shared_ptr<GbmBuffer>, QRegion> acquire();
    void damage(const QRegion &damage);
    void releaseBuffer(GbmBuffer *buffer);

    DrmGpu *gpu() const;
    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;
    uint32_t flags() const;

    enum class Error {
        ModifiersUnsupported,
        Unknown
    };
    static std::variant<std::shared_ptr<GbmSwapchain>, Error> createSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format, uint32_t flags);
    static std::variant<std::shared_ptr<GbmSwapchain>, Error> createSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format, QVector<uint64_t> modifiers);

private:
    DrmGpu *const m_gpu;
    const QSize m_size;
    const uint32_t m_format;
    const uint64_t m_modifier;
    const uint32_t m_flags;

    std::deque<std::pair<gbm_bo *, uint32_t>> m_buffers;
    DamageJournal m_damageJournal;
};
}
