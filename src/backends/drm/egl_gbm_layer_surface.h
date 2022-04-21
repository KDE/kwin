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
#include <QSharedPointer>
#include <optional>

#include "drm_object_plane.h"
#include "outputlayer.h"

namespace KWaylandServer
{
class SurfaceInterface;
class LinuxDmaBufV1ClientBuffer;
}

namespace KWin
{

class DrmBuffer;
class GbmSurface;
class DumbSwapchain;
class ShadowBuffer;
class EglGbmBackend;
class SurfaceItem;
class GLTexture;

class EglGbmLayerSurface : public QObject
{
    Q_OBJECT
public:
    EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend);
    ~EglGbmLayerSurface();

    OutputLayerBeginFrameInfo startRendering(const QSize &bufferSize, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation, const QMap<uint32_t, QVector<uint64_t>> &formats);
    void aboutToStartPainting(DrmOutput *output, const QRegion &damagedRegion);
    std::optional<std::tuple<QSharedPointer<DrmBuffer>, QRegion>> endRendering(DrmPlane::Transformations renderOrientation, const QRegion &damagedRegion);

    bool doesSurfaceFit(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const;
    QSharedPointer<GLTexture> texture() const;
    void destroyResources();
    EglGbmBackend *eglBackend() const;
    QSharedPointer<DrmBuffer> renderTestBuffer(const QSize &bufferSize, const QMap<uint32_t, QVector<uint64_t>> &formats);

private:
    bool checkGbmSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats);
    bool createGbmSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers);
    bool createGbmSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats);
    bool doesGbmSurfaceFit(GbmSurface *surf, const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const;

    bool doesShadowBufferFit(ShadowBuffer *buffer, const QSize &size, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation) const;
    bool doesSwapchainFit(DumbSwapchain *swapchain) const;

    QSharedPointer<DrmBuffer> importBuffer();
    QSharedPointer<DrmBuffer> importDmabuf();
    QSharedPointer<DrmBuffer> importWithCpu();

    enum class MultiGpuImportMode {
        Dmabuf,
        DumbBuffer,
        DumbBufferXrgb8888,
        Failed
    };
    MultiGpuImportMode m_importMode = MultiGpuImportMode::Dmabuf;

    QRegion m_currentDamage;
    QSharedPointer<GbmSurface> m_gbmSurface;
    QSharedPointer<GbmSurface> m_oldGbmSurface;
    QSharedPointer<ShadowBuffer> m_shadowBuffer;
    QSharedPointer<ShadowBuffer> m_oldShadowBuffer;
    QSharedPointer<DumbSwapchain> m_importSwapchain;
    QSharedPointer<DumbSwapchain> m_oldImportSwapchain;

    DrmGpu *const m_gpu;
    EglGbmBackend *const m_eglBackend;
};

}
