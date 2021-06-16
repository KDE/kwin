/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surfaceitem.h"

namespace KWin
{

class GLTexture;
class OpenGLBackend;

class KWIN_EXPORT OpenGLSurfaceTextureProvider : public SurfaceTextureProvider
{
public:
    explicit OpenGLSurfaceTextureProvider(OpenGLBackend *backend);
    ~OpenGLSurfaceTextureProvider() override;

    bool isValid() const override;

    OpenGLBackend *backend() const;
    GLTexture *texture() const;

protected:
    OpenGLBackend *m_backend;
    QScopedPointer<GLTexture> m_texture;
};

} // namespace KWin
