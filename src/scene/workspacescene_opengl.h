/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglbackend.h"

#include "scene/decorationitem.h"
#include "scene/workspacescene.h"
#include "shadow.h"

#include "kwinglutils.h"

namespace KWin
{
class OpenGLBackend;

class KWIN_EXPORT WorkspaceSceneOpenGL : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneOpenGL(OpenGLBackend *backend);
    ~WorkspaceSceneOpenGL() override;

    std::unique_ptr<Shadow> createShadow(Window *window) override;
    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;
    bool supportsNativeFence() const override;
    DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    bool animationsSupported() const override;

    OpenGLBackend *backend() const
    {
        return m_backend;
    }

    std::shared_ptr<GLTexture> textureForOutput(Output *output) const override;

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
class SceneOpenGLShadow
    : public Shadow
{
public:
    explicit SceneOpenGLShadow(Window *window);
    ~SceneOpenGLShadow() override;

    GLTexture *shadowTexture()
    {
        return m_texture.get();
    }

protected:
    bool prepareBackend() override;

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
    explicit SceneOpenGLDecorationRenderer(Decoration::DecoratedClientImpl *client);
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
    void renderPart(const QRect &rect, const QRect &partRect, const QPoint &textureOffset, qreal devicePixelRatio, bool rotated = false);
    static const QMargins texturePadForPart(const QRect &rect, const QRect &partRect);
    void resizeTexture();
    int toNativeSize(int size) const;
    std::unique_ptr<GLTexture> m_texture;
};

} // namespace
