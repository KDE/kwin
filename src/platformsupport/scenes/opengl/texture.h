/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwingltexture.h>
#include <kwingltexture_p.h>

namespace KWin
{

class OpenGLBackend;
class SceneOpenGLTexturePrivate;
class WindowPixmap;

class SceneOpenGLTexture : public GLTexture
{
public:
    explicit SceneOpenGLTexture(OpenGLBackend *backend);
    ~SceneOpenGLTexture() override;

    SceneOpenGLTexture & operator = (const SceneOpenGLTexture& tex);

    void discard() override final;

private:
    SceneOpenGLTexture(SceneOpenGLTexturePrivate& dd);

    bool load(WindowPixmap *pixmap);
    void updateFromPixmap(WindowPixmap *pixmap);

    Q_DECLARE_PRIVATE(SceneOpenGLTexture)

    friend class OpenGLWindowPixmap;
};

class SceneOpenGLTexturePrivate : public GLTexturePrivate
{
public:
    ~SceneOpenGLTexturePrivate() override;

    virtual bool loadTexture(WindowPixmap *pixmap) = 0;
    virtual void updateTexture(WindowPixmap *pixmap);
    virtual OpenGLBackend *backend() = 0;

protected:
    SceneOpenGLTexturePrivate();

private:
    Q_DISABLE_COPY(SceneOpenGLTexturePrivate)
};

}
