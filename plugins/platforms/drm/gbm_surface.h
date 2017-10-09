/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#ifndef KWIN_DRM_GBM_SURFACE_H
#define KWIN_DRM_GBM_SURFACE_H

#include <stdint.h>

struct gbm_bo;
struct gbm_device;
struct gbm_surface;

namespace KWin
{

class GbmSurface
{
public:
    explicit GbmSurface(gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
    ~GbmSurface();

    gbm_bo *lockFrontBuffer();
    void releaseBuffer(gbm_bo *bo);

    operator bool() const {
        return m_surface != nullptr;
    }

    operator gbm_surface*() const {
        return m_surface;
    }

private:
    gbm_surface *m_surface;
};

}

#endif
