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

class KWIN_EXPORT PlatformOpenGLSurfaceTexture : public PlatformSurfaceTexture
{
public:
    explicit PlatformOpenGLSurfaceTexture(OpenGLBackend *backend);
    ~PlatformOpenGLSurfaceTexture() override;

    bool isValid() const override;

    OpenGLBackend *backend() const;
    GLTexture *texture() const;

    virtual bool create() = 0;
    virtual void update(const QRegion &region) = 0;

protected:
    OpenGLBackend *m_backend;
    QScopedPointer<GLTexture> m_texture;
};

} // namespace KWin
