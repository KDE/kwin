/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCENE_OPENGL_H
#define KWIN_SCENE_OPENGL_H

#include "openglbackend.h"

#include "decorationitem.h"
#include "scene.h"
#include "shadow.h"

#include "kwinglutils.h"

namespace KWin
{
class OpenGLBackend;

class KWIN_EXPORT SceneOpenGL
    : public Scene
{
    Q_OBJECT
public:
    struct RenderNode
    {
        GLTexture *texture = nullptr;
        WindowQuadList quads;
        QMatrix4x4 transformMatrix;
        int firstVertex = 0;
        int vertexCount = 0;
        qreal opacity = 1;
        bool hasAlpha = false;
        TextureCoordinateType coordinateType = UnnormalizedCoordinates;
        qreal scale = 1.0;
    };

    struct RenderContext
    {
        QVector<RenderNode> renderNodes;
        QStack<QMatrix4x4> transformStack;
        QStack<qreal> opacityStack;
        const QRegion clip;
        const bool hardwareClipping;
        const qreal renderTargetScale;
    };

    explicit SceneOpenGL(OpenGLBackend *backend);
    ~SceneOpenGL() override;
    bool initFailed() const override;
    void paint(RenderTarget *renderTarget, const QRegion &region) override;
    Shadow *createShadow(Window *window) override;
    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;
    bool supportsNativeFence() const override;
    DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    bool animationsSupported() const override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    void render(Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

    OpenGLBackend *backend() const
    {
        return m_backend;
    }

    QVector<QByteArray> openGLPlatformInterfaceExtensions() const override;
    std::shared_ptr<GLTexture> textureForOutput(Output *output) const override;

    static std::unique_ptr<SceneOpenGL> createScene(OpenGLBackend *backend);
    static bool supported(OpenGLBackend *backend);

protected:
    void paintBackground(const QRegion &region) override;
    void paintOffscreenQuickView(OffscreenQuickView *w) override;

private:
    void doPaintBackground(const QVector<float> &vertices);
    QMatrix4x4 modelViewProjectionMatrix(const WindowPaintData &data) const;
    QVector4D modulate(float opacity, float brightness) const;
    void setBlendEnabled(bool enabled);
    void createRenderNode(Item *item, RenderContext *context);

    bool init_ok = true;
    OpenGLBackend *m_backend;
    GLuint vao = 0;
    bool m_blendingEnabled = false;
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
        return m_texture.data();
    }
    GLTexture *texture() const
    {
        return m_texture.data();
    }

private:
    void renderPart(const QRect &rect, const QRect &partRect, const QPoint &textureOffset, qreal devicePixelRatio, bool rotated = false);
    static const QMargins texturePadForPart(const QRect &rect, const QRect &partRect);
    void resizeTexture();
    int toNativeSize(int size) const;
    QScopedPointer<GLTexture> m_texture;
};

} // namespace

#endif
