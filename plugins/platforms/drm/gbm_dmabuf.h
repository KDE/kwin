/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
