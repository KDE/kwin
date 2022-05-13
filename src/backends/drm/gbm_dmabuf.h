/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

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

    for (int i = 0; i < attributes.planeCount; ++i) {
        attributes.fd[i] = gbm_bo_get_fd_for_plane(bo, i);
        attributes.offset[i] = gbm_bo_get_offset(bo, i);
        attributes.pitch[i] = gbm_bo_get_stride_for_plane(bo, i);
        attributes.modifier[i] = gbm_bo_get_modifier(bo);
    }

    return attributes;
}

} // namespace KWin
