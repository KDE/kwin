/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "texture.h"
#include "backend.h"
#include "scene.h"

namespace KWin
{

SceneOpenGLTexture::SceneOpenGLTexture(OpenGLBackend *backend)
    : GLTexture(*backend->createBackendTexture(this))
{
}

SceneOpenGLTexture::~SceneOpenGLTexture()
{
}

SceneOpenGLTexture& SceneOpenGLTexture::operator = (const SceneOpenGLTexture& tex)
{
    d_ptr = tex.d_ptr;
    return *this;
}

void SceneOpenGLTexture::discard()
{
    d_ptr = d_func()->backend()->createBackendTexture(this);
}

bool SceneOpenGLTexture::load(WindowPixmap *pixmap)
{
    if (!pixmap->isValid()) {
        return false;
    }

    // decrease the reference counter for the old texture
    d_ptr = d_func()->backend()->createBackendTexture(this); //new TexturePrivate();

    Q_D(SceneOpenGLTexture);
    return d->loadTexture(pixmap);
}

void SceneOpenGLTexture::updateFromPixmap(WindowPixmap *pixmap)
{
    Q_D(SceneOpenGLTexture);
    d->updateTexture(pixmap);
}

SceneOpenGLTexturePrivate::SceneOpenGLTexturePrivate()
{
}

SceneOpenGLTexturePrivate::~SceneOpenGLTexturePrivate()
{
}

void SceneOpenGLTexturePrivate::updateTexture(WindowPixmap *pixmap)
{
    Q_UNUSED(pixmap)
}

}
