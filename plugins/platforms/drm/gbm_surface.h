/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_GBM_SURFACE_H
#define KWIN_DRM_GBM_SURFACE_H

#include <cstdint>

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

    gbm_surface* surface() const {
        return m_surface;
    }

private:
    gbm_surface *m_surface;
};

}

#endif
