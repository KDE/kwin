/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_BUFFER_GBM_H
#define KWIN_DRM_BUFFER_GBM_H

#include "drm_buffer.h"

#include <QSharedPointer>

struct gbm_bo;

namespace KWaylandServer
{
class ClientBuffer;
}

namespace KWin
{

class GbmSurface;

class GbmBuffer : public QObject
{
    Q_OBJECT
public:
    GbmBuffer(GbmSurface *surface, gbm_bo *bo);
    GbmBuffer(gbm_bo *buffer, KWaylandServer::ClientBuffer *clientBuffer);
    virtual ~GbmBuffer();

    void releaseBuffer();
    bool map(uint32_t flags);

    void *mappedData() const;
    uint32_t stride() const;
    KWaylandServer::ClientBuffer *clientBuffer() const;
    gbm_bo* getBo() const;

protected:
    GbmSurface *m_surface = nullptr;
    gbm_bo *m_bo = nullptr;
    KWaylandServer::ClientBuffer *m_clientBuffer = nullptr;

    void *m_data = nullptr;
    void *m_mapping = nullptr;
    uint32_t m_stride = 0;
};

class DrmGbmBuffer : public DrmBuffer, public GbmBuffer
{
public:
    DrmGbmBuffer(DrmGpu *gpu, GbmSurface *surface, gbm_bo *bo);
    DrmGbmBuffer(DrmGpu *gpu, gbm_bo *buffer, KWaylandServer::ClientBuffer *clientBuffer);
    ~DrmGbmBuffer() override;

    bool needsModeChange(DrmBuffer *b) const override;
    bool hasBo() const;

private:
    void initialize();
};

}

#endif

