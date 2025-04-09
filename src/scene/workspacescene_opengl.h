/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/decorationitem.h"
#include "scene/workspacescene.h"

#include "opengl/glutils.h"

namespace KWin
{

class EglBackend;

class KWIN_EXPORT WorkspaceSceneOpenGL : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneOpenGL(EglBackend *backend);
    ~WorkspaceSceneOpenGL() override;

    EglContext *openglContext() const override;
    std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedWindowImpl *impl) override;
    bool animationsSupported() const override;
    std::pair<std::shared_ptr<GLTexture>, ColorDescription> textureForOutput(Output *output) const override;

private:
    EglBackend *m_backend;
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
