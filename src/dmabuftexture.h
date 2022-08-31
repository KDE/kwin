/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include "core/dmabufattributes.h"

#include <memory>

namespace KWin
{
class GLFramebuffer;
class GLTexture;

class KWIN_EXPORT DmaBufTexture
{
public:
    explicit DmaBufTexture(std::shared_ptr<GLTexture> texture, DmaBufAttributes &&attributes);
    virtual ~DmaBufTexture();

    const DmaBufAttributes &attributes() const;
    GLTexture *texture() const;
    GLFramebuffer *framebuffer() const;

protected:
    std::shared_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    DmaBufAttributes m_attributes;
};

}
