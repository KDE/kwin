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

#include <memory>

struct gbm_bo;

namespace KWin
{

class GbmSurface;

class DrmSurfaceBuffer : public DrmBuffer
{
public:
    DrmSurfaceBuffer(int fd, const std::shared_ptr<GbmSurface> &surface);
    ~DrmSurfaceBuffer() override;

    bool needsModeChange(DrmBuffer *b) const override {
        if (DrmSurfaceBuffer *sb = dynamic_cast<DrmSurfaceBuffer*>(b)) {
            return hasBo() != sb->hasBo();
        } else {
            return true;
        }
    }

    bool hasBo() const {
        return m_bo != nullptr;
    }

    gbm_bo* getBo() const {
        return m_bo;
    }

    void releaseGbm() override;

private:
    std::shared_ptr<GbmSurface> m_surface;
    gbm_bo *m_bo = nullptr;
};

}

#endif

