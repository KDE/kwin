/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once
#include "kwin_export.h"

#include <memory>

namespace KWin
{
class GLFramebuffer;
class GLTexture;

struct DmaBufAttributes
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;

    int fd[4] = {-1, -1, -1, -1};
    int offset[4] = {0, 0, 0, 0};
    int pitch[4] = {0, 0, 0, 0};
};

class KWIN_EXPORT DmaBufTexture
{
public:
    explicit DmaBufTexture(std::shared_ptr<GLTexture> texture, const DmaBufAttributes &attributes);
    virtual ~DmaBufTexture();

    DmaBufAttributes attributes() const;
    GLTexture *texture() const;
    GLFramebuffer *framebuffer() const;

protected:
    std::shared_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    DmaBufAttributes m_attributes;
};

}
