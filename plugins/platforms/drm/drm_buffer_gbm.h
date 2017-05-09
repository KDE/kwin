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

struct gbm_bo;
struct gbm_surface;

namespace KWin
{

class DrmBackend;

class DrmSurfaceBuffer : public DrmBuffer
{
public:
    DrmSurfaceBuffer(DrmBackend *backend, gbm_surface *surface);
    ~DrmSurfaceBuffer();

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
    void releaseGbm() override;

private:
    gbm_surface *m_surface = nullptr;
    gbm_bo *m_bo = nullptr;
};

}

#endif

