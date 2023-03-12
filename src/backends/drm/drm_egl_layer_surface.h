/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QMap>
#include <QPointer>
#include <QRegion>
#include <optional>

#include "core/outputlayer.h"
#include "drm_plane.h"

namespace KWaylandServer
{
class SurfaceInterface;
class LinuxDmaBufV1ClientBuffer;
}

namespace KWin
{

class DrmFramebuffer;
class GbmSurface;
class DumbSwapchain;
class ShadowBuffer;
class EglGbmBackend;
class SurfaceItem;
class GLTexture;
class GbmBuffer;

class EglGbmLayerSurface : public QObject
{
    Q_OBJECT
public:
    enum class BufferTarget {
        Normal,
        Linear,
        Dumb
    };
    enum class FormatOption {
        PreferAlpha,
        RequireAlpha
    };
    EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target = BufferTarget::Normal, FormatOption formatOption = FormatOption::PreferAlpha);
    ~EglGbmLayerSurface();

    std::optional<OutputLayerBeginFrameInfo> startRendering(const QSize &bufferSize, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation, const QMap<uint32_t, QVector<uint64_t>> &formats);
    void aboutToStartPainting(DrmOutput *output, const QRegion &damagedRegion);
    bool endRendering(DrmPlane::Transformations renderOrientation, const QRegion &damagedRegion);

    bool doesSurfaceFit(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const;
    std::shared_ptr<GLTexture> texture() const;
    void destroyResources();
    EglGbmBackend *eglBackend() const;
    std::shared_ptr<DrmFramebuffer> renderTestBuffer(const QSize &bufferSize, const QMap<uint32_t, QVector<uint64_t>> &formats);

    std::shared_ptr<DrmFramebuffer> currentBuffer() const;

private:
    bool doesShadowBufferFit(ShadowBuffer *buffer, const QSize &size, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation) const;

    enum class MultiGpuImportMode {
        Dmabuf,
        DumbBuffer
    };
    struct Surface
    {
        std::shared_ptr<GbmSurface> gbmSurface;
        std::shared_ptr<DumbSwapchain> importSwapchain;
        MultiGpuImportMode importMode;
        std::shared_ptr<GbmBuffer> currentBuffer;
        std::shared_ptr<DrmFramebuffer> currentFramebuffer;
        bool forceLinear = false;
    };
    bool checkSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats);
    bool doesSurfaceFit(const Surface &surface, const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const;
    std::optional<Surface> createSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const;
    std::optional<Surface> createSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, MultiGpuImportMode importMode) const;
    std::shared_ptr<GbmSurface> createGbmSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, bool forceLinear) const;

    std::shared_ptr<DrmFramebuffer> doRenderTestBuffer(Surface &surface) const;
    std::shared_ptr<DrmFramebuffer> importBuffer(Surface &surface, const std::shared_ptr<GbmBuffer> &sourceBuffer) const;
    std::shared_ptr<DrmFramebuffer> importDmabuf(GbmBuffer *sourceBuffer) const;
    std::shared_ptr<DrmFramebuffer> importWithCpu(Surface &surface, GbmBuffer *sourceBuffer) const;

    Surface m_surface;
    Surface m_oldSurface;
    std::shared_ptr<ShadowBuffer> m_shadowBuffer;
    std::shared_ptr<ShadowBuffer> m_oldShadowBuffer;

    DrmGpu *const m_gpu;
    EglGbmBackend *const m_eglBackend;
    const BufferTarget m_bufferTarget;
    const FormatOption m_formatOption;
};

}
