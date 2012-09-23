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

namespace KWin
{
class ColorCorrection;
class LanczosFilter;
class OpenGLBackend;

class SceneOpenGL
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
    virtual CompositingType compositingType() const {
        return OpenGLCompositing;
    }
    virtual bool hasPendingFlush() const;
    virtual int paint(QRegion damage, ToplevelList windows);
    virtual void windowAdded(Toplevel*);
    virtual void windowDeleted(Deleted*);
    virtual void screenGeometryChanged(const QSize &size);
    virtual OverlayWindow *overlayWindow();
    virtual bool waitSyncAvailable() const;

    void idle();

    /**
     * @brief Factory method to create a backend specific texture.
     *
     * @return :SceneOpenGL::Texture*
     **/
    Texture *createTexture();
    Texture *createTexture(const QPixmap& pix, GLenum target = GL_TEXTURE_2D);

    static SceneOpenGL *createScene();

protected:
    SceneOpenGL(Workspace* ws, OpenGLBackend *backend);
    virtual void paintBackground(QRegion region);
    QMatrix4x4 transformation(int mask, const ScreenPaintData &data) const;

    virtual void doPaintBackground(const QVector<float> &vertices) = 0;
    virtual SceneOpenGL::Window *createWindow(Toplevel *t) = 0;
public Q_SLOTS:
    virtual void windowOpacityChanged(KWin::Toplevel* c);
    virtual void windowGeometryShapeChanged(KWin::Toplevel* c);
    virtual void windowClosed(KWin::Toplevel* c, KWin::Deleted* deleted);
protected:
    bool init_ok;
private:
    QHash< Toplevel*, Window* > windows;
    bool debug;
    OpenGLBackend *m_backend;
};

class SceneOpenGL2 : public SceneOpenGL
{
    Q_OBJECT
public:
    SceneOpenGL2(OpenGLBackend *backend);
    virtual ~SceneOpenGL2();

    static bool supported(OpenGLBackend *backend);

    ColorCorrection *colorCorrection();

protected:
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    virtual void doPaintBackground(const QVector< float >& vertices);
    virtual SceneOpenGL::Window *createWindow(Toplevel *t);
    virtual void finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);

private Q_SLOTS:
    void slotColorCorrectedChanged();

private:
    void performPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);

private:
    QWeakPointer<LanczosFilter> m_lanczosFilter;
    ColorCorrection *m_colorCorrection;
};

#ifndef KWIN_HAVE_OPENGLES
class SceneOpenGL1 : public SceneOpenGL
{
public:
    SceneOpenGL1(OpenGLBackend *backend);
    virtual ~SceneOpenGL1();
    virtual void screenGeometryChanged(const QSize &size);
    virtual int paint(QRegion damage, ToplevelList windows);

    static bool supported(OpenGLBackend *backend);

protected:
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    virtual void doPaintBackground(const QVector< float >& vertices);
    virtual SceneOpenGL::Window *createWindow(Toplevel *t);

private:
    void setupModelViewProjectionMatrix();
    bool m_resetModelViewProjectionMatrix;
};
#endif

class SceneOpenGL::TexturePrivate
    : public GLTexturePrivate
{
public:
    virtual ~TexturePrivate();

    virtual void findTarget() = 0;
    virtual bool loadTexture(const Pixmap& pix, const QSize& size, int depth) = 0;
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
    Texture(OpenGLBackend *backend, const QPixmap& pix, GLenum target = GL_TEXTURE_2D);
    virtual ~Texture();

    Texture & operator = (const Texture& tex);

    using GLTexture::load;
    virtual bool load(const QImage& image, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    virtual void discard();

protected:
    void findTarget();
    virtual bool load(const Pixmap& pix, const QSize& size, int depth,
                      QRegion region);
    virtual bool load(const Pixmap& pix, const QSize& size, int depth);

    Texture(TexturePrivate& dd);

private:
    Q_DECLARE_PRIVATE(Texture)

    friend class SceneOpenGL::Window;
};

class SceneOpenGL::Window
    : public Scene::Window
{
public:
    virtual ~Window();
    virtual void performPaint(int mask, QRegion region, WindowPaintData data);
    virtual void pixmapDiscarded();
    bool bindTexture();
    void discardTexture();
    void checkTextureSize();
    void setScene(SceneOpenGL *scene) {
        m_scene = scene;
    }

protected:
    Window(Toplevel* c);
    enum TextureType {
        Content,
        DecorationTop,
        DecorationLeft,
        DecorationRight,
        DecorationBottom,
        Shadow
    };

    QMatrix4x4 transformation(int mask, const WindowPaintData &data) const;
    void paintDecoration(const QPixmap* decoration, TextureType decorationType, const QRegion& region, const QRect& rect, const WindowPaintData& data, const WindowQuadList& quads, bool updateDeco, bool hardwareClipping);
    void paintShadow(const QRegion &region, const WindowPaintData &data, bool hardwareClipping);
    void makeDecorationArrays(const WindowQuadList& quads, const QRect &rect, Texture *tex) const;
    void renderQuads(int, const QRegion& region, const WindowQuadList& quads, GLTexture* tex, bool normalized, bool hardwareClipping);
    /**
     * @brief Called from performPaint once it is determined whether the window will be painted.
     * This method has to be implemented by the concrete sub class to perform operations for setting
     * up the OpenGL state (e.g. pushing a matrix).
     *
     * @param mask The mask which is used to render the Window
     * @param data The WindowPaintData for this frame
     * @see performPaint
     * @see endRenderWindow
     **/
    virtual void beginRenderWindow(int mask, const WindowPaintData &data) = 0;
    /**
     * @brief Called from performPaint once the window and decoration has been rendered.
     * This method has to be implemented by the concrete sub class to perform operations for resetting
     * the OpenGL state after rendering this window (e.g. pop matrix).
     *
     * @param data The WindowPaintData with which this window got rendered
     **/
    virtual void endRenderWindow(const WindowPaintData &data) = 0;
    /**
     * @brief Prepare the OpenGL rendering state before the texture with @p type will be rendered.
     *
     * @param type The type of the Texture which will be rendered
     * @param opacity The opacity value to use for this rendering
     * @param brightness The brightness value to use for this rendering
     * @param saturation The saturation value to use for this rendering
     * @param screen The index of the screen to use for this rendering
     **/
    virtual void prepareStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen) = 0;
    /**
     * @brief Restores the OpenGL rendering state after the texture with @p type has been rendered.
     *
     * @param type The type of the Texture which has been rendered
     * @param opacity The opacity value used for the rendering
     * @param brightness The brightness value used for this rendering
     * @param saturation The saturation value used for this rendering
     * @param screen The index of the screen to use for this rendering
     **/
    virtual void restoreStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen) = 0;

    /**
     * @brief Returns the texture for the given @p type.
     *
     * @param type The Texture Type for which the texture should be retrieved
     * @return :GLTexture* the texture
     **/
    GLTexture *textureForType(TextureType type);

protected:
    SceneOpenGL *m_scene;

private:
    template<class T>
    void paintDecorations(const WindowPaintData &data, const QRegion &region, bool hardwareClipping);
    Texture *texture;
    Texture *topTexture;
    Texture *leftTexture;
    Texture *rightTexture;
    Texture *bottomTexture;
};

class SceneOpenGL2Window : public SceneOpenGL::Window
{
public:
    SceneOpenGL2Window(Toplevel *c);
    virtual ~SceneOpenGL2Window();

protected:
    virtual void beginRenderWindow(int mask, const WindowPaintData &data);
    virtual void endRenderWindow(const WindowPaintData &data);
    virtual void prepareStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen);
    virtual void restoreStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen);
};

#ifndef KWIN_HAVE_OPENGLES
class SceneOpenGL1Window : public SceneOpenGL::Window
{
public:
    SceneOpenGL1Window(Toplevel *c);
    virtual ~SceneOpenGL1Window();

protected:
    virtual void beginRenderWindow(int mask, const WindowPaintData &data);
    virtual void endRenderWindow(const WindowPaintData &data);
    virtual void prepareStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen);
    virtual void restoreStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen);
};
#endif

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

    Texture* m_texture;
    Texture* m_textTexture;
    Texture* m_oldTextTexture;
    QPixmap* m_textPixmap; // need to keep the pixmap around to workaround some driver problems
    Texture* m_iconTexture;
    Texture* m_oldIconTexture;
    Texture* m_selectionTexture;
    GLVertexBuffer* m_unstyledVBO;
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
    SceneOpenGLShadow(Toplevel *toplevel);
    virtual ~SceneOpenGLShadow();

    GLTexture *shadowTexture() {
        return m_texture;
    }
protected:
    virtual void buildQuads();
    virtual bool prepareBackend();
private:
    GLTexture *m_texture;
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
class OpenGLBackend
{
public:
    OpenGLBackend();
    virtual ~OpenGLBackend();
    /**
     * @return Time passes since start of rendering current frame.
     * @see startRenderTimer
     **/
    qint64 renderTime() {
        return m_renderTimer.elapsed();
    }
    virtual void screenGeometryChanged(const QSize &size) = 0;
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture) = 0;
    /**
     * @brief Backend specific code to prepare the rendering of a frame including flushing the
     * previously rendered frame to the screen if the backend works this way.
     **/
    virtual void prepareRenderingFrame() = 0;
    /**
     * @brief Backend specific code to handle the end of rendering a frame.
     *
     * @param mask The rendering mask of this frame
     * @param damage The actual updated region in this frame
     **/
    virtual void endRenderingFrame(int mask, const QRegion &damage) = 0;
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
    OverlayWindow *overlayWindow() {
        return m_overlayWindow;
    }
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
    bool waitSyncAvailable() const {
        return m_waitSync;
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
    /**
     * @brief Whether the backend used double buffering.
     *
     * @return bool @c true if double buffered, @c false otherwise
     **/
    bool isDoubleBuffer() const {
        return m_doubleBuffer;
    }
protected:
    /**
     * @brief Backend specific flushing of frame to screen.
     **/
    virtual void flushBuffer() = 0;
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
    void setHasWaitSync(bool enabled) {
        m_waitSync = enabled;
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
    /**
     * @brief Sets whether the OpenGL context uses double buffering.
     *
     * Should be called by the concrete subclass once it is determined whether the OpenGL context
     * uses double buffering.
     * If the subclass does not call this method, the backend defaults to @c false.
     *
     * @param doubleBuffer @c true if double buffering, @c false otherwise
     **/
    void setDoubleBuffer(bool doubleBuffer) {
        m_doubleBuffer = doubleBuffer;
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
     * @return int Rendering mask of previously rendered frame
     **/
    int lastMask() const {
        return m_lastMask;
    }
    void setLastMask(int mask) {
        m_lastMask = mask;
    }
    /**
     * @brief Starts the timer for how long it takes to render the frame.
     *
     * @see renderTime
     **/
    void startRenderTimer() {
        m_renderTimer.start();
    }

private:
    /**
     * @brief The OverlayWindow used by this Backend.
     **/
    OverlayWindow *m_overlayWindow;
    /**
     * @brief Whether VSync is available, defaults to @c false.
     **/
    bool m_waitSync;
    /**
     * @brief Whether direct rendering is used, defaults to @c false.
     **/
    bool m_directRendering;
    /**
     * @brief Whether double bufferering is available, defaults to @c false.
     **/
    bool m_doubleBuffer;
    /**
     * @brief Whether the initialization failed, of course default to @c false.
     **/
    bool m_failed;
    /**
     * @brief Damaged region of previously rendered frame.
     **/
    QRegion m_lastDamage;
    /**
     * @brief Rendering mask of previously rendered frame.
     **/
    int m_lastMask;
    /**
     * @brief Timer to measure how long a frame renders.
     **/
    QElapsedTimer m_renderTimer;
};

inline bool SceneOpenGL::hasPendingFlush() const
{
    return m_backend->hasPendingFlush();
}

} // namespace

#endif
