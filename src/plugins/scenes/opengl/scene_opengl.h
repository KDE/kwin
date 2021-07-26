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
class LanczosFilter;
class OpenGLBackend;

class KWIN_EXPORT SceneOpenGL
    : public Scene
{
    Q_OBJECT
public:
    class EffectFrame;
    ~SceneOpenGL() override;
    bool initFailed() const override;
    void paint(int screenId, const QRegion &damage, const QList<Toplevel *> &windows,
               RenderLoop *renderLoop) override;
    Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    OverlayWindow *overlayWindow() const override;
    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;
    bool supportsSurfacelessContext() const override;
    bool supportsNativeFence() const override;
    DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    virtual QMatrix4x4 projectionMatrix() const = 0;
    bool animationsSupported() const override;
    PlatformSurfaceTexture *createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    PlatformSurfaceTexture *createPlatformSurfaceTextureX11(SurfacePixmapX11 *pixmap) override;
    PlatformSurfaceTexture *createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    bool debug() const { return m_debug; }
    void initDebugOutput();

    OpenGLBackend *backend() const {
        return m_backend;
    }

    QVector<QByteArray> openGLPlatformInterfaceExtensions() const override;
    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *output) const override;

    static SceneOpenGL *createScene(QObject *parent);

protected:
    SceneOpenGL(OpenGLBackend *backend, QObject *parent = nullptr);
    void paintBackground(const QRegion &region) override;
    void aboutToStartPainting(int screenId, const QRegion &damage) override;
    void extendPaintRegion(QRegion &region, bool opaqueFullscreen) override;
    QMatrix4x4 transformation(int mask, const ScreenPaintData &data) const;
    void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data) override;
    void paintEffectQuickView(EffectQuickView *w) override;

    void handleGraphicsReset(GLenum status);

    virtual void doPaintBackground(const QVector<float> &vertices) = 0;
    virtual void updateProjectionMatrix(const QRect &geometry) = 0;

protected:
    bool init_ok;
private:
    bool viewportLimitsMatched(const QSize &size) const;

private:
    bool m_resetOccurred = false;
    bool m_debug;
    OpenGLBackend *m_backend;
};

class SceneOpenGL2 : public SceneOpenGL
{
    Q_OBJECT
public:
    explicit SceneOpenGL2(OpenGLBackend *backend, QObject *parent = nullptr);
    ~SceneOpenGL2() override;
    CompositingType compositingType() const override {
        return OpenGLCompositing;
    }

    static bool supported(OpenGLBackend *backend);

    QMatrix4x4 projectionMatrix() const override { return m_projectionMatrix; }
    QMatrix4x4 screenProjectionMatrix() const override { return m_screenProjectionMatrix; }

protected:
    void paintSimpleScreen(int mask, const QRegion &region) override;
    void paintGenericScreen(int mask, const ScreenPaintData &data) override;
    void doPaintBackground(const QVector< float >& vertices) override;
    Scene::Window *createWindow(Toplevel *t) override;
    void finalDrawWindow(EffectWindowImpl* w, int mask, const QRegion &region, WindowPaintData& data) override;
    void updateProjectionMatrix(const QRect &geometry) override;
    void paintCursor(const QRegion &region) override;

private:
    void performPaintWindow(EffectWindowImpl* w, int mask, const QRegion &region, WindowPaintData& data);

    LanczosFilter *m_lanczosFilter;
    QScopedPointer<GLTexture> m_cursorTexture;
    bool m_cursorTextureDirty = false;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_screenProjectionMatrix;
    GLuint vao;
};

class OpenGLWindow final : public Scene::Window
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
    };

    struct RenderContext
    {
        QVector<RenderNode> renderNodes;
        const int paintFlags;
        const QRegion clip;
        const WindowPaintData &paintData;
        const bool hardwareClipping;
    };

    OpenGLWindow(Toplevel *toplevel, SceneOpenGL *scene);
    ~OpenGLWindow() override;

    void performPaint(int mask, const QRegion &region, const WindowPaintData &data) override;
    QSharedPointer<GLTexture> windowTexture() override;

private:
    QMatrix4x4 modelViewProjectionMatrix(int mask, const WindowPaintData &data) const;
    QVector4D modulate(float opacity, float brightness) const;
    void setBlendEnabled(bool enabled);
    void createRenderNode(Item *item, RenderContext *context);

    SceneOpenGL *m_scene;
    bool m_blendingEnabled = false;
};

class SceneOpenGL::EffectFrame
    : public Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame, SceneOpenGL *scene);
    ~EffectFrame() override;

    void free() override;
    void freeIconFrame() override;
    void freeTextFrame() override;
    void freeSelection() override;

    void render(const QRegion &region, double opacity, double frameOpacity) override;

    void crossFadeIcon() override;
    void crossFadeText() override;

    static void cleanup();

private:
    void updateTexture();
    void updateTextTexture();

    GLTexture *m_texture;
    GLTexture *m_textTexture;
    GLTexture *m_oldTextTexture;
    QPixmap *m_textPixmap; // need to keep the pixmap around to workaround some driver problems
    GLTexture *m_iconTexture;
    GLTexture *m_oldIconTexture;
    GLTexture *m_selectionTexture;
    GLVertexBuffer *m_unstyledVBO;
    SceneOpenGL *m_scene;

    static GLTexture* m_unstyledTexture;
    static QPixmap* m_unstyledPixmap; // need to keep the pixmap around to workaround some driver problems
    static void updateUnstyledTexture(); // Update OpenGL unstyled frame texture
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
    explicit SceneOpenGLShadow(Toplevel *toplevel);
    ~SceneOpenGLShadow() override;

    GLTexture *shadowTexture() {
        return m_texture.data();
    }
protected:
    bool prepareBackend() override;
private:
    QSharedPointer<GLTexture> m_texture;
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

    GLTexture *texture() {
        return m_texture.data();
    }
    GLTexture *texture() const {
        return m_texture.data();
    }

private:
    void resizeTexture();
    QScopedPointer<GLTexture> m_texture;
};

class KWIN_EXPORT OpenGLFactory : public SceneFactory
{
    Q_OBJECT
    Q_INTERFACES(KWin::SceneFactory)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Scene" FILE "opengl.json")

public:
    explicit OpenGLFactory(QObject *parent = nullptr);
    ~OpenGLFactory() override;

    Scene *create(QObject *parent = nullptr) const override;
};

} // namespace

#endif
