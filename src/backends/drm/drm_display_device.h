/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QRegion>

#include "drm_object_plane.h"

namespace KWin
{

class DrmBuffer;
class DrmGpu;
class DrmOutputLayer;

class DrmDisplayDevice
{
public:
    DrmDisplayDevice(DrmGpu *gpu);
    virtual ~DrmDisplayDevice() = default;

    DrmGpu *gpu() const;

    virtual bool present() = 0;
    virtual void frameFailed() const = 0;
    virtual void pageFlipped(std::chrono::nanoseconds timestamp) const = 0;
    virtual QRect renderGeometry() const = 0;
    virtual DrmOutputLayer *outputLayer() const = 0;
    virtual QVector<int32_t> regionToRects(const QRegion &region) const = 0;

protected:
    DrmGpu *const m_gpu;
};

}
