/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#include "core/graphicsbuffer.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <string.h>

#include <optional>

namespace KWin
{

inline std::optional<DmaBufAttributes> dmaBufAttributesForBo(gbm_bo *bo)
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
        if (!attributes.fd[i].isValid()) {
            qWarning() << "gbm_bo_get_fd_for_plane() failed:" << strerror(errno);
            return std::nullopt;
        }
        attributes.offset[i] = gbm_bo_get_offset(bo, i);
        attributes.pitch[i] = gbm_bo_get_stride_for_plane(bo, i);
    }
#else
    if (attributes.planeCount > 1) {
        return attributes;
    }

    attributes.fd[0] = FileDescriptor{gbm_bo_get_fd(bo)};
    if (!attributes.fd[0].isValid()) {
        qWarning() << "gbm_bo_get_fd() failed:" << strerror(errno);
        return std::nullopt;
    }
    attributes.offset[0] = gbm_bo_get_offset(bo, 0);
    attributes.pitch[0] = gbm_bo_get_stride_for_plane(bo, 0);
#endif

    return attributes;
}

} // namespace KWin
