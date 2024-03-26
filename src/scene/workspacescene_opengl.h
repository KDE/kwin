/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "platformsupport/scenes/opengl/openglbackend.h"

#include "scene/decorationitem.h"
#include "scene/shadowitem.h"
#include "scene/workspacescene.h"

#include "opengl/glutils.h"

namespace KWin
{
class OpenGLBackend;

class KWIN_EXPORT WorkspaceSceneOpenGL : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneOpenGL(OpenGLBackend *backend);
    ~WorkspaceSceneOpenGL() override;

    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;
    bool supportsNativeFence() const override;
    OpenGlContext *openglContext() const override;
    std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedWindowImpl *impl) override;
    std::unique_ptr<ShadowTextureProvider> createShadowTextureProvider(Shadow *shadow) override;
    bool animationsSupported() const override;

    OpenGLBackend *backend() const
    {
        return m_backend;
    }

    std::pair<std::shared_ptr<GLTexture>, ColorDescription> textureForOutput(Output *output) const override;

private:
    OpenGLBackend *m_backend;
    GLuint vao = 0;
};

/**
 * @short OpenGL implementation of Shadow.
 *
 * This class extends Shadow by the Elements required for OpenGL rendering.
 * @author Martin Gräßlin <mgraesslin@kde.org>
 */
class OpenGLShadowTextureProvider : public ShadowTextureProvider
{
public:
    explicit OpenGLShadowTextureProvider(Shadow *shadow);
    ~OpenGLShadowTextureProvider() override;

    GLTexture *shadowTexture()
    {
        return m_texture.get();
    }

    void update() override;

private:
    std::shared_ptr<GLTexture> m_texture;
};

class SceneOpenGLDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };
    explicit SceneOpenGLDecorationRenderer(Decoration::DecoratedWindowImpl *client);
    ~SceneOpenGLDecorationRenderer() override;

    void render(const QRegion &region) override;

    GLTexture *texture()
    {
        return m_texture.get();
    }
    GLTexture *texture() const
    {
        return m_texture.get();
    }

private:
    void renderPart(const QRectF &rect, const QRectF &partRect, const QPoint &textureOffset, qreal devicePixelRatio, bool rotated = false);
    static const QMargins texturePadForPart(const QRectF &rect, const QRectF &partRect);
    void resizeTexture();
    int toNativeSize(double size) const;
    std::unique_ptr<GLTexture> m_texture;
};

} // namespace
