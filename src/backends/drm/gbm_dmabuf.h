/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"
#include "dmabuftexture.h"
#include <libdrm/drm_fourcc.h>

#include <gbm.h>

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
        attributes.fd[i] = FileDescriptor{gbm_bo_get_fd_for_plane(bo, i)};
        attributes.offset[i] = gbm_bo_get_offset(bo, i);
        attributes.pitch[i] = gbm_bo_get_stride_for_plane(bo, i);
    }
#else
    if (attributes.planeCount > 1) {
        return attributes;
    }

    attributes.fd[0] = FileDescriptor{gbm_bo_get_fd(bo)};
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
    gbm_bo *bo = nullptr;
    if (modifiers.count() > 0 && !(modifiers.count() == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID)) {
#if HAVE_GBM_BO_CREATE_WITH_MODIFIERS2
        bo = gbm_bo_create_with_modifiers2(device,
                                           size.width(),
                                           size.height(),
                                           format,
                                           modifiers.constData(), modifiers.count(),
                                           GBM_BO_USE_RENDERING);
#else
        bo = gbm_bo_create_with_modifiers(device,
                                          size.width(),
                                          size.height(),
                                          format,
                                          modifiers.constData(), modifiers.count());
#endif
    }

    if (!bo && (modifiers.isEmpty() || modifiers.contains(DRM_FORMAT_MOD_INVALID))) {
        bo = gbm_bo_create(device,
                           size.width(),
                           size.height(),
                           format,
                           GBM_BO_USE_LINEAR);
    }
    return bo;
}

} // namespace KWin
