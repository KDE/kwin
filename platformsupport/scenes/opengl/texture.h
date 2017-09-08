/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_SCENE_OPENGL_TEXTURE_H
#define KWIN_SCENE_OPENGL_TEXTURE_H

#include <kwingltexture.h>
#include <kwingltexture_p.h>

namespace KWin
{

class OpenGLBackend;
class WindowPixmap;
class SceneOpenGLTexturePrivate;

class SceneOpenGLTexture
    : public GLTexture
{
public:
    SceneOpenGLTexture(OpenGLBackend *backend);
    virtual ~SceneOpenGLTexture();

    SceneOpenGLTexture & operator = (const SceneOpenGLTexture& tex);

    void discard() override final;

protected:
    bool load(WindowPixmap *pixmap);
    void updateFromPixmap(WindowPixmap *pixmap);

    SceneOpenGLTexture(SceneOpenGLTexturePrivate& dd);

private:
    Q_DECLARE_PRIVATE(SceneOpenGLTexture)

    friend class OpenGLWindowPixmap;
};

class SceneOpenGLTexturePrivate
    : public GLTexturePrivate
{
public:
    virtual ~SceneOpenGLTexturePrivate();

    virtual bool loadTexture(WindowPixmap *pixmap) = 0;
    virtual void updateTexture(WindowPixmap *pixmap);
    virtual OpenGLBackend *backend() = 0;

protected:
    SceneOpenGLTexturePrivate();

private:
    Q_DISABLE_COPY(SceneOpenGLTexturePrivate)
};

}

#endif
