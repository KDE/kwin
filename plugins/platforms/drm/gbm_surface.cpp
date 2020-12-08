/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
