/*
 * Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#pragma once

#include "dmabuftexture.h"
#include <gbm.h>
#include <QSize>
#include <epoxy/egl.h>

namespace KWin
{

class GbmDmaBuf : public DmaBufTexture
{
public:
    ~GbmDmaBuf();

    int fd() const override
    {
        return m_fd;
    }
    quint32 stride() const override {
        return gbm_bo_get_stride(m_bo);
    }

    static GbmDmaBuf *createBuffer(const QSize &size, gbm_device *device);

private:
    GbmDmaBuf(GLTexture *texture, gbm_bo *bo, int fd);
    struct gbm_bo *const m_bo;
    const int m_fd;
};

}
