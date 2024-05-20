/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screensnapshots.h"

#include "effecthandler.h"
#include "globals.h"
#include <core/renderlayer.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/glframebuffer.h>
#include <opengl/gltexture.h>
#include <scene/workspacescene.h>

namespace KWin
{

bool renderOutput(Snapshot &snapshot, Output *screen)
{
    auto texture = GLTexture::allocate(GL_RGBA8, screen->pixelSize());
    if (!texture) {
        return false;
    }

    Scene *scene = effects->scene();
    RenderLayer layer(screen->renderLoop());
    SceneDelegate delegate(scene, screen);
    delegate.setLayer(&layer);

    effects->makeOpenGLContextCurrent();
    scene->prePaint(&delegate);

    snapshot.texture = std::move(texture);
    snapshot.framebuffer = std::make_unique<GLFramebuffer>(snapshot.texture.get());

    RenderTarget renderTarget(snapshot.framebuffer.get());
    scene->paint(renderTarget, screen->geometry());
    scene->postPaint();
    effects->doneOpenGLContextCurrent();
    return true;
}

bool paintScreenInTexture(Snapshot &current, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen)
{
    // Render the screen in an offscreen texture.
    const QSize nativeSize = screen->pixelSize();
    if (!current.texture || current.texture->size() != nativeSize) {
        current.texture = GLTexture::allocate(GL_RGBA8, nativeSize);
        if (!current.texture) {
            return false;
        }
        current.framebuffer = std::make_unique<GLFramebuffer>(current.texture.get());
    }

    RenderTarget fboRenderTarget(current.framebuffer.get());
    RenderViewport fboViewport(viewport.renderRect(), viewport.scale(), fboRenderTarget);

    GLFramebuffer::pushFramebuffer(current.framebuffer.get());
    effects->paintScreen(fboRenderTarget, fboViewport, mask, region, screen);
    GLFramebuffer::popFramebuffer();
    return true;
}

}
