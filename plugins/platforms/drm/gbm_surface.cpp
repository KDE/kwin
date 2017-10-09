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
#include "gbm_surface.h"

#include <gbm.h>

namespace KWin
{

GbmSurface::GbmSurface(gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
    : m_surface(gbm_surface_create(gbm, width, height, format, flags))
{
}

GbmSurface::~GbmSurface()
{
    if (m_surface) {
        gbm_surface_destroy(m_surface);
    }
}

gbm_bo *GbmSurface::lockFrontBuffer()
{
    if (!m_surface) {
        return nullptr;
    }
    return gbm_surface_lock_front_buffer(m_surface);
}

void GbmSurface::releaseBuffer(gbm_bo *bo)
{
    if (!bo || !m_surface) {
        return;
    }
    gbm_surface_release_buffer(m_surface, bo);
}

}
