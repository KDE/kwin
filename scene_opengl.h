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
#include "kwingltexture_p.h"

#include "decorations/decorationrenderer.h"

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
    class Texture;
    class TexturePrivate;
    class Window;
    virtual ~SceneOpenGL();
    virtual bool initFailed() const;
    virtual bool hasPendingFlush() const;
    virtual qint64 paint(QRegion damage, ToplevelList windows);
    virtual Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame);
    virtual Shadow *createShadow(Toplevel *toplevel);
    virtual void screenGeometryChanged(const QSize &size);
    virtual OverlayWindow *overlayWindow();
    virtual bool usesOverlayWindow() const;
    virtual bool blocksForRetrace() const;
    virtual bool syncsToVBlank() const;
    virtual bool makeOpenGLContextCurrent() override;
    virtual void doneOpenGLContextCurrent() override;
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    virtual void triggerFence() override;
    virtual QMatrix4x4 projectionMatrix() const = 0;
    bool animationsSupported() const override;

    void insertWait();

    void idle();

    bool debug() const { return m_debug; }
    void initDebugOutput();

    /**
     * @brief Factory method to create a backend specific texture.
     *
     * @return :SceneOpenGL::Texture*
     **/
    Texture *createTexture();

    OpenGLBackend *backend() const {
        return m_backend;
    }

    /**
     * Copy a region of pixels from the current read to the current draw buffer
     */
    static void copyPixels(const QRegion &region);

    static SceneOpenGL *createScene(QObject *parent);

protected:
    SceneOpenGL(OpenGLBackend *backend, QObject *parent = nullptr);
    virtual void paintBackground(QRegion region);
    virtual void extendPaintRegion(QRegion &region, bool opaqueFullscreen);
    QMatrix4x4 transformation(int mask, const ScreenPaintData &data) const;
    virtual void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data);

    void handleGraphicsReset(GLenum status);

    virtual void doPaintBackground(const QVector<float> &vertices) = 0;
    virtual void updateProjectionMatrix() = 0;

Q_SIGNALS:
    void resetCompositing();

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
    virtual ~SceneOpenGL2();
    virtual CompositingType compositingType() const {
        return OpenGL2Compositing;
    }

    static bool supported(OpenGLBackend *backend);

    QMatrix4x4 projectionMatrix() const override { return m_projectionMatrix; }
    QMatrix4x4 screenProjectionMatrix() const override { return m_screenProjectionMatrix; }

protected:
    virtual void paintSimpleScreen(int mask, QRegion region);
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    virtual void doPaintBackground(const QVector< float >& vertices);
    virtual Scene::Window *createWindow(Toplevel *t);
    virtual void finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    virtual void updateProjectionMatrix() override;

private Q_SLOTS:
    void resetLanczosFilter();

private:
    void performPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    QMatrix4x4 createProjectionMatrix() const;

private:
    LanczosFilter *m_lanczosFilter;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_screenProjectionMatrix;
    GLuint vao;
};

class SceneOpenGL::TexturePrivate
    : public GLTexturePrivate
{
public:
    virtual ~TexturePrivate();

    virtual bool loadTexture(WindowPixmap *pixmap) = 0;
    virtual void updateTexture(WindowPixmap *pixmap);
    virtual OpenGLBackend *backend() = 0;

protected:
    TexturePrivate();

private:
    Q_DISABLE_COPY(TexturePrivate)
};

class SceneOpenGL::Texture
    : public GLTexture
{
public:
    Texture(OpenGLBackend *backend);
    virtual ~Texture();

    Texture & operator = (const Texture& tex);

    void discard() override final;

protected:
    bool load(WindowPixmap *pixmap);
    void updateFromPixmap(WindowPixmap *pixmap);

    Texture(TexturePrivate& dd);

private:
    Q_DECLARE_PRIVATE(Texture)

    friend class OpenGLWindowPixmap;
};

class SceneOpenGL::Window
    : public Scene::Window
{
public:
    virtual ~Window();
    bool beginRenderWindow(int mask, const QRegion &region, WindowPaintData &data);
    virtual void performPaint(int mask, QRegion region, WindowPaintData data) = 0;
    void endRenderWindow();
    bool bindTexture();
    void setScene(SceneOpenGL *scene) {
        m_scene = scene;
    }

protected:
    virtual WindowPixmap* createWindowPixmap();
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

class SceneOpenGL2Window : public SceneOpenGL::Window
{
public:
    enum Leaf { ShadowLeaf = 0, DecorationLeaf, ContentLeaf, PreviousContentLeaf, LeafCount };

    struct LeafNode
    {
        LeafNode()
            : texture(0),
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
    virtual ~SceneOpenGL2Window();

protected:
    QMatrix4x4 modelViewProjectionMatrix(int mask, const WindowPaintData &data) const;
    QVector4D modulate(float opacity, float brightness) const;
    void setBlendEnabled(bool enabled);
    void setupLeafNodes(LeafNode *nodes, const WindowQuadList *quads, const WindowPaintData &data);
    virtual void performPaint(int mask, QRegion region, WindowPaintData data);

private:
    /**
     * Whether prepareStates enabled blending and restore states should disable again.
     **/
    bool m_blendingEnabled;
};

class OpenGLWindowPixmap : public WindowPixmap
{
public:
    explicit OpenGLWindowPixmap(Scene::Window *window, SceneOpenGL *scene);
    virtual ~OpenGLWindowPixmap();
    SceneOpenGL::Texture *texture() const;
    bool bind();
    bool isValid() const override;
protected:
    WindowPixmap *createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface) override;
private:
    explicit OpenGLWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface, WindowPixmap *parent, SceneOpenGL *scene);
    QScopedPointer<SceneOpenGL::Texture> m_texture;
    SceneOpenGL *m_scene;
};

class SceneOpenGL::EffectFrame
    : public Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame, SceneOpenGL *scene);
    virtual ~EffectFrame();

    virtual void free();
    virtual void freeIconFrame();
    virtual void freeTextFrame();
    virtual void freeSelection();

    virtual void render(QRegion region, double opacity, double frameOpacity);

    virtual void crossFadeIcon();
    virtual void crossFadeText();

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
 **/
class SceneOpenGLShadow
    : public Shadow
{
public:
    explicit SceneOpenGLShadow(Toplevel *toplevel);
    virtual ~SceneOpenGLShadow();

    GLTexture *shadowTexture() {
        return m_texture.data();
    }
protected:
    virtual void buildQuads();
    virtual bool prepareBackend();
private:
    QSharedPointer<GLTexture> m_texture;
};

/**
 * @short Profiler to detect whether we have triple buffering
 * The strategy is to start setBlocksForRetrace(false) but assume blocking and have the system prove that assumption wrong
 **/
class KWIN_EXPORT SwapProfiler
{
public:
    SwapProfiler();
    void init();
    void begin();
    /**
     * @return char being 'd' for double, 't' for triple (or more - but non-blocking) buffering and
     * 0 (NOT '0') otherwise, so you can act on "if (char result = SwapProfiler::end()) { fooBar(); }
     **/
    char end();
private:
    QElapsedTimer m_timer;
    qint64  m_time;
    int m_counter;
};

/**
 * @brief The OpenGLBackend creates and holds the OpenGL context and is responsible for Texture from Pixmap.
 *
 * The OpenGLBackend is an abstract base class used by the SceneOpenGL to abstract away the differences
 * between various OpenGL windowing systems such as GLX and EGL.
 *
 * A concrete implementation has to create and release the OpenGL context in a way so that the
 * SceneOpenGL does not have to care about it.
 *
 * In addition a major task for this class is to generate the SceneOpenGL::TexturePrivate which is
 * able to perform the texture from pixmap operation in the given backend.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 **/
class KWIN_EXPORT OpenGLBackend
{
public:
    OpenGLBackend();
    virtual ~OpenGLBackend();

    virtual void init() = 0;
    /**
     * @return Time passes since start of rendering current frame.
     * @see startRenderTimer
     **/
    qint64 renderTime() {
        return m_renderTimer.nsecsElapsed();
    }
    virtual void screenGeometryChanged(const QSize &size) = 0;
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture) = 0;

    /**
     * @brief Backend specific code to prepare the rendering of a frame including flushing the
     * previously rendered frame to the screen if the backend works this way.
     *
     * @return A region that if not empty will be repainted in addition to the damaged region
     **/
    virtual QRegion prepareRenderingFrame() = 0;

    /**
     * @brief Backend specific code to handle the end of rendering a frame.
     *
     * @param renderedRegion The possibly larger region that has been rendered
     * @param damagedRegion The damaged region that should be posted
     **/
    virtual void endRenderingFrame(const QRegion &damage, const QRegion &damagedRegion) = 0;
    virtual void endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion);
    virtual bool makeCurrent() = 0;
    virtual void doneCurrent() = 0;
    virtual bool usesOverlayWindow() const = 0;
    /**
     * Whether the rendering needs to be split per screen.
     * Default implementation returns @c false.
     **/
    virtual bool perScreenRendering() const;
    virtual QRegion prepareRenderingForScreen(int screenId);
    /**
     * @brief Compositor is going into idle mode, flushes any pending paints.
     **/
    void idle();

    /**
     * @return bool Whether the scene needs to flush a frame.
     **/
    bool hasPendingFlush() const {
        return !m_lastDamage.isEmpty();
    }

    /**
     * @brief Returns the OverlayWindow used by the backend.
     *
     * A backend does not have to use an OverlayWindow, this is mostly for the X world.
     * In case the backend does not use an OverlayWindow it is allowed to return @c null.
     * It's the task of the caller to check whether it is @c null.
     *
     * @return :OverlayWindow*
     **/
    virtual OverlayWindow *overlayWindow();
    /**
     * @brief Whether the creation of the Backend failed.
     *
     * The SceneOpenGL should test whether the Backend got constructed correctly. If this method
     * returns @c true, the SceneOpenGL should not try to start the rendering.
     *
     * @return bool @c true if the creation of the Backend failed, @c false otherwise.
     **/
    bool isFailed() const {
        return m_failed;
    }
    /**
     * @brief Whether the Backend provides VSync.
     *
     * Currently only the GLX backend can provide VSync.
     *
     * @return bool @c true if VSync support is available, @c false otherwise
     **/
    bool syncsToVBlank() const {
        return m_syncsToVBlank;
    }
    /**
     * @brief Whether VSync blocks execution until the screen is in the retrace
     *
     * Case for waitVideoSync and non triple buffering buffer swaps
     *
     **/
    bool blocksForRetrace() const {
        return m_blocksForRetrace;
    }
    /**
     * @brief Whether the backend uses direct rendering.
     *
     * Some OpenGLScene modes require direct rendering. E.g. the OpenGL 2 should not be used
     * if direct rendering is not supported by the Scene.
     *
     * @return bool @c true if the GL context is direct, @c false if indirect
     **/
    bool isDirectRendering() const {
        return m_directRendering;
    }

    bool supportsBufferAge() const {
        return m_haveBufferAge;
    }

    /**
     * @returns whether the context is surfaceless
     **/
    bool isSurfaceLessContext() const {
        return m_surfaceLessContext;
    }

    /**
     * Returns the damage that has accumulated since a buffer of the given age was presented.
     */
    QRegion accumulatedDamageHistory(int bufferAge) const;

    /**
     * Saves the given region to damage history.
     */
    void addToDamageHistory(const QRegion &region);

    /**
     * The backend specific extensions (e.g. EGL/GLX extensions).
     *
     * Not the OpenGL (ES) extension!
     **/
    QList<QByteArray> extensions() const {
        return m_extensions;
    }

    /**
     * @returns whether the backend specific extensions contains @p extension.
     **/
    bool hasExtension(const QByteArray &extension) const {
        return m_extensions.contains(extension);
    }

protected:
    /**
     * @brief Backend specific flushing of frame to screen.
     **/
    virtual void present() = 0;
    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     **/
    void setFailed(const QString &reason);
    /**
     * @brief Sets whether the backend provides VSync.
     *
     * Should be called by the concrete subclass once it is determined whether VSync is supported.
     * If the subclass does not call this method, the backend defaults to @c false.
     * @param enabled @c true if VSync support available, @c false otherwise.
     **/
    void setSyncsToVBlank(bool enabled) {
        m_syncsToVBlank = enabled;
    }
    /**
     * @brief Sets whether the VSync iplementation blocks
     *
     * Should be called by the concrete subclass once it is determined how VSync works.
     * If the subclass does not call this method, the backend defaults to @c false.
     * @param enabled @c true if VSync blocks, @c false otherwise.
     **/
    void setBlocksForRetrace(bool enabled) {
        m_blocksForRetrace = enabled;
    }
    /**
     * @brief Sets whether the OpenGL context is direct.
     *
     * Should be called by the concrete subclass once it is determined whether the OpenGL context is
     * direct or indirect.
     * If the subclass does not call this method, the backend defaults to @c false.
     *
     * @param direct @c true if the OpenGL context is direct, @c false if indirect
     **/
    void setIsDirectRendering(bool direct) {
        m_directRendering = direct;
    }

    void setSupportsBufferAge(bool value) {
        m_haveBufferAge = value;
    }

    /**
     * @return const QRegion& Damage of previously rendered frame
     **/
    const QRegion &lastDamage() const {
        return m_lastDamage;
    }
    void setLastDamage(const QRegion &damage) {
        m_lastDamage = damage;
    }
    /**
     * @brief Starts the timer for how long it takes to render the frame.
     *
     * @see renderTime
     **/
    void startRenderTimer() {
        m_renderTimer.start();
    }

    /**
     * @param set whether the context is surface less
     **/
    void setSurfaceLessContext(bool set) {
        m_surfaceLessContext = set;
    }

    /**
     * Sets the platform-specific @p extensions.
     *
     * These are the EGL/GLX extensions, not the OpenGL extensions
     **/
    void setExtensions(const QList<QByteArray> &extensions) {
        m_extensions = extensions;
    }

    SwapProfiler m_swapProfiler;

private:
    /**
     * @brief Whether VSync is available and used, defaults to @c false.
     **/
    bool m_syncsToVBlank;
    /**
     * @brief Whether present() will block execution until the next vertical retrace @c false.
     **/
    bool m_blocksForRetrace;
    /**
     * @brief Whether direct rendering is used, defaults to @c false.
     **/
    bool m_directRendering;
    /**
     * @brief Whether the backend supports GLX_EXT_buffer_age / EGL_EXT_buffer_age.
     */
    bool m_haveBufferAge;
    /**
     * @brief Whether the initialization failed, of course default to @c false.
     **/
    bool m_failed;
    /**
     * @brief Damaged region of previously rendered frame.
     **/
    QRegion m_lastDamage;
    /**
     * @brief The damage history for the past 10 frames.
     */
    QList<QRegion> m_damageHistory;
    /**
     * @brief Timer to measure how long a frame renders.
     **/
    QElapsedTimer m_renderTimer;
    bool m_surfaceLessContext = false;

    QList<QByteArray> m_extensions;
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
    virtual ~SceneOpenGLDecorationRenderer();

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

inline SceneOpenGL::Texture* OpenGLWindowPixmap::texture() const
{
    return m_texture.data();
}

} // namespace

#endif
