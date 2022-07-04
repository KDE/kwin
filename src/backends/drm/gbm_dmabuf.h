/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"
#include "dmabuftexture.h"
#include <libdrm/drm_fourcc.h>

#include <gbm.h>

#if !GBM_CREATE_WITH_MODIFIERS2
inline struct gbm_bo *
gbm_bo_create_with_modifiers2(struct gbm_device *gbm,
                              uint32_t width, uint32_t height,
                              uint32_t format,
                              const uint64_t *modifiers,
                              const unsigned int count, quint32 flags)
{
    return gbm_bo_create_with_modifiers(gbm, width, height, format, modifiers, count);
}
#endif

namespace KWin
{

inline DmaBufAttributes dmaBufAttributesForBo(gbm_bo *bo)
{
    DmaBufAttributes attributes;
    attributes.planeCount = gbm_bo_get_plane_count(bo);
    attributes.width = gbm_bo_get_width(bo);
    attributes.height = gbm_bo_get_height(bo);
    attributes.format = gbm_bo_get_format(bo);
    attributes.modifier = gbm_bo_get_modifier(bo);

#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    for (int i = 0; i < attributes.planeCount; ++i) {
        attributes.fd[i] = gbm_bo_get_fd_for_plane(bo, i);
        attributes.offset[i] = gbm_bo_get_offset(bo, i);
        attributes.pitch[i] = gbm_bo_get_stride_for_plane(bo, i);
    }
#else
    if (attributes.planeCount > 1) {
        return attributes;
    }

    attributes.fd[0] = gbm_bo_get_fd(bo);
    attributes.offset[0] = gbm_bo_get_offset(bo, 0);
    attributes.pitch[0] = gbm_bo_get_stride_for_plane(bo, 0);
#endif

    return attributes;
}

inline DmaBufParams dmaBufParamsForBo(gbm_bo *bo)
{
    DmaBufParams attributes;
    attributes.planeCount = gbm_bo_get_plane_count(bo);
    attributes.width = gbm_bo_get_width(bo);
    attributes.height = gbm_bo_get_height(bo);
    attributes.format = gbm_bo_get_format(bo);
    attributes.modifier = gbm_bo_get_modifier(bo);
    return attributes;
}

inline gbm_bo *createGbmBo(gbm_device *device, const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    const uint32_t flags = GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR;
    gbm_bo *bo = nullptr;
    if (modifiers.count() > 0 && !(modifiers.count() == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID)) {
        bo = gbm_bo_create_with_modifiers2(device,
                                           size.width(),
                                           size.height(),
                                           format,
                                           modifiers.constData(), modifiers.count(), 0);
    }

    if (!bo && (modifiers.isEmpty() || modifiers.contains(DRM_FORMAT_MOD_INVALID))) {
        bo = gbm_bo_create(device,
                           size.width(),
                           size.height(),
                           format,
                           flags);
    }
    return bo;
}

} // namespace KWin
