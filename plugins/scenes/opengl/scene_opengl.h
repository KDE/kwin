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

#ifndef KWIN_SCENE_OPENGL_H
#define KWIN_SCENE_OPENGL_H

#include "scene.h"
#include "shadow.h"

#include "kwinglutils.h"

#include "decorations/decorationrenderer.h"
#include "platformsupport/scenes/opengl/backend.h"

namespace KWin
{
class LanczosFilter;
class OpenGLBackend;
class SyncManager;
class SyncObject;

class KWIN_EXPORT SceneOpenGL
    : public Scene
{
    Q_OBJECT
public:
    class EffectFrame;
    class Window;
    ~SceneOpenGL() override;
    bool initFailed() const override;
    bool hasPendingFlush() const override;
    qint64 paint(QRegion damage, QList<Toplevel *> windows) override;
    Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    void screenGeometryChanged(const QSize &size) override;
    OverlayWindow *overlayWindow() const override;
    bool usesOverlayWindow() const override;
    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    void triggerFence() override;
    virtual QMatrix4x4 projectionMatrix() const = 0;
    bool animationsSupported() const override;

    void insertWait();

    void idle() override;

    bool debug() const { return m_debug; }
    void initDebugOutput();

    /**
     * @brief Factory method to create a backend specific texture.
     *
     * @return :SceneOpenGL::Texture*
     */
    SceneOpenGLTexture *createTexture();

    OpenGLBackend *backend() const {
        return m_backend;
    }

    QVector<QByteArray> openGLPlatformInterfaceExtensions() const override;

    static SceneOpenGL *createScene(QObject *parent);

protected:
    SceneOpenGL(OpenGLBackend *backend, QObject *parent = nullptr);
    void paintBackground(QRegion region) override;
    void extendPaintRegion(QRegion &region, bool opaqueFullscreen) override;
    QMatrix4x4 transformation(int mask, const ScreenPaintData &data) const;
    void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data) override;
    void paintEffectQuickView(EffectQuickView *w) override;

    void handleGraphicsReset(GLenum status);

    virtual void doPaintBackground(const QVector<float> &vertices) = 0;
    virtual void updateProjectionMatrix() = 0;

protected:
    bool init_ok;
private:
    bool viewportLimitsMatched(const QSize &size) const;
private:
    bool m_debug;
    OpenGLBackend *m_backend;
    SyncManager *m_syncManager;
    SyncObject *m_currentFence;
};

class SceneOpenGL2 : public SceneOpenGL
{
    Q_OBJECT
public:
    explicit SceneOpenGL2(OpenGLBackend *backend, QObject *parent = nullptr);
    ~SceneOpenGL2() override;
    CompositingType compositingType() const override {
        return OpenGL2Compositing;
    }

    static bool supported(OpenGLBackend *backend);

    QMatrix4x4 projectionMatrix() const override { return m_projectionMatrix; }
    QMatrix4x4 screenProjectionMatrix() const override { return m_screenProjectionMatrix; }

protected:
    void paintSimpleScreen(int mask, QRegion region) override;
    void paintGenericScreen(int mask, ScreenPaintData data) override;
    void doPaintBackground(const QVector< float >& vertices) override;
    Scene::Window *createWindow(Toplevel *t) override;
    void finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data) override;
    void updateProjectionMatrix() override;
    void paintCursor() override;

private:
    void performPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    QMatrix4x4 createProjectionMatrix() const;

private:
    LanczosFilter *m_lanczosFilter;
    QScopedPointer<GLTexture> m_cursorTexture;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_screenProjectionMatrix;
    GLuint vao;
};

class SceneOpenGL::Window
    : public Scene::Window
{
public:
    ~Window() override;
    bool beginRenderWindow(int mask, const QRegion &region, WindowPaintData &data);
    void performPaint(int mask, QRegion region, WindowPaintData data) override = 0;
    void endRenderWindow();
    bool bindTexture();
    void setScene(SceneOpenGL *scene) {
        m_scene = scene;
    }

protected:
    WindowPixmap* createWindowPixmap() override;
    Window(Toplevel* c);
    enum TextureType {
        Content,
        Decoration,
        Shadow
    };

    QMatrix4x4 transformation(int mask, const WindowPaintData &data) const;
    GLTexture *getDecorationTexture() const;

protected:
    SceneOpenGL *m_scene;
    bool m_hardwareClipping;
};

class OpenGLWindowPixmap;

class SceneOpenGL2Window : public SceneOpenGL::Window
{
public:
    enum Leaf { ShadowLeaf = 0, DecorationLeaf, ContentLeaf, PreviousContentLeaf, LeafCount };

    struct LeafNode
    {
        LeafNode()
            : texture(nullptr),
              firstVertex(0),
              vertexCount(0),
              opacity(1.0),
              hasAlpha(false),
              coordinateType(UnnormalizedCoordinates)
        {
        }

        GLTexture *texture;
        int firstVertex;
        int vertexCount;
        float opacity;
        bool hasAlpha;
        TextureCoordinateType coordinateType;
    };

    explicit SceneOpenGL2Window(Toplevel *c);
    ~SceneOpenGL2Window() override;

protected:
    QMatrix4x4 modelViewProjectionMatrix(int mask, const WindowPaintData &data) const;
    QVector4D modulate(float opacity, float brightness) const;
    void setBlendEnabled(bool enabled);
    void setupLeafNodes(LeafNode *nodes, const WindowQuadList *quads, const WindowPaintData &data);
    void performPaint(int mask, QRegion region, WindowPaintData data) override;

private:
    void renderSubSurface(GLShader *shader, const QMatrix4x4 &mvp, const QMatrix4x4 &windowMatrix, OpenGLWindowPixmap *pixmap, const QRegion &region, bool hardwareClipping);
    /**
     * Whether prepareStates enabled blending and restore states should disable again.
     */
    bool m_blendingEnabled;
};

class OpenGLWindowPixmap : public WindowPixmap
{
public:
    explicit OpenGLWindowPixmap(Scene::Window *window, SceneOpenGL *scene);
    ~OpenGLWindowPixmap() override;
    SceneOpenGLTexture *texture() const;
    bool bind();
    bool isValid() const override;
protected:
    WindowPixmap *createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface) override;
private:
    explicit OpenGLWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface, WindowPixmap *parent, SceneOpenGL *scene);
    QScopedPointer<SceneOpenGLTexture> m_texture;
    SceneOpenGL *m_scene;
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

    void render(QRegion region, double opacity, double frameOpacity) override;

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
    void buildQuads() override;
    bool prepareBackend() override;
private:
    QSharedPointer<GLTexture> m_texture;
};

class SceneOpenGLDecorationRenderer : public Decoration::Renderer
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

    void render() override;
    void reparent(Deleted *deleted) override;

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

inline bool SceneOpenGL::hasPendingFlush() const
{
    return m_backend->hasPendingFlush();
}

inline bool SceneOpenGL::usesOverlayWindow() const
{
    return m_backend->usesOverlayWindow();
}

inline SceneOpenGLTexture* OpenGLWindowPixmap::texture() const
{
    return m_texture.data();
}

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
