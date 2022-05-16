/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"
#include "dmabuftexture.h"

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

#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    for (int i = 0; i < attributes.planeCount; ++i) {
        attributes.fd[i] = gbm_bo_get_fd_for_plane(bo, i);
        attributes.offset[i] = gbm_bo_get_offset(bo, i);
        attributes.pitch[i] = gbm_bo_get_stride_for_plane(bo, i);
        attributes.modifier[i] = gbm_bo_get_modifier(bo);
    }
#else
    if (attributes.planeCount > 1) {
        return attributes;
    }

    attributes.fd[0] = gbm_bo_get_fd(bo);
    attributes.offset[0] = gbm_bo_get_offset(bo, 0);
    attributes.pitch[0] = gbm_bo_get_stride_for_plane(bo, 0);
    attributes.modifier[0] = gbm_bo_get_modifier(bo);
#endif

    return attributes;
}

} // namespace KWin
