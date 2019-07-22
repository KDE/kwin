/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2017 Roman Gilg <subdiff@gmail.com>
Copyright 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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

