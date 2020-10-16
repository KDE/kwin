/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_PLATFORM_H
#define KWIN_PLATFORM_H
#include <kwin_export.h>
#include <kwinglobals.h>
#include <epoxy/egl.h>
#include <fixx11h.h>
#include "fixqopengl.h"
#include "input.h"

#include <QImage>
#include <QObject>

#include <functional>

class QAction;

namespace KWaylandServer {
class OutputConfigurationInterface;
}

namespace KWin
{
namespace ColorCorrect {
class Manager;
}

class AbstractOutput;
class Edge;
class Compositor;
class DmaBufTexture;
class OverlayWindow;
class OpenGLBackend;
class Outline;
class OutlineVisual;
class QPainterBackend;
class Scene;
class Screens;
class ScreenEdges;
class Toplevel;

namespace Decoration
{
class Renderer;
class DecoratedClientImpl;
}

class KWIN_EXPORT Outputs : public QVector<AbstractOutput*>
{
public:
    Outputs(){};
    template <typename T>
    Outputs(const QVector<T> &other) {
        resize(other.size());
        std::copy(other.constBegin(), other.constEnd(), begin());
    }
};

class KWIN_EXPORT Platform : public QObject
{
    Q_OBJECT
public:
    ~Platform() override;

    virtual void init() = 0;
    virtual Screens *createScreens(QObject *parent = nullptr);
    virtual OpenGLBackend *createOpenGLBackend();
    virtual QPainterBackend *createQPainterBackend();
    virtual DmaBufTexture *createDmaBufTexture(const QSize &size) {
        Q_UNUSED(size);
        return nullptr;
    }

    /**
     * Informs the Platform that it is about to go down and shall do appropriate cleanup.
     * Child classes can override this function but must call the parent implementation in
     * the end.
     */
    virtual void prepareShutdown();

    /**
     * Allows the platform to create a platform specific screen edge.
     * The default implementation creates a Edge.
     */
    virtual Edge *createScreenEdge(ScreenEdges *parent);
    /**
     * Allows the platform to create a platform specific Cursor.
     * The default implementation creates an InputRedirectionCursor.
     */
    virtual void createPlatformCursor(QObject *parent = nullptr);
    virtual void warpPointer(const QPointF &globalPos);
    /**
     * Whether our Compositing EGL display allows a surface less context
     * so that a sharing context could be created.
     */
    bool supportsSurfacelessContext() const;
    /**
     * The EGLDisplay used by the compositing scene.
     */
    EGLDisplay sceneEglDisplay() const;
    void setSceneEglDisplay(EGLDisplay display);
    /**
     * The EGLContext used by the compositing scene.
     */
    virtual EGLContext sceneEglContext() const {
        return m_context;
    }
    /**
     * Sets the @p context used by the compositing scene.
     */
    void setSceneEglContext(EGLContext context) {
        m_context = context;
    }
    /**
     * Returns the compositor-wide shared EGL context. This function may return EGL_NO_CONTEXT
     * if the underlying rendering backend does not use EGL.
     *
     * Note that the returned context should never be made current. Instead, create a context
     * that shares with this one and make the new context current.
     */
    EGLContext sceneEglGlobalShareContext() const;
    /**
     * Sets the global share context to @a context. This function is intended to be called only
     * by rendering backends.
     */
    void setSceneEglGlobalShareContext(EGLContext context);
    /**
     * The first (in case of multiple) EGLSurface used by the compositing scene.
     */
    EGLSurface sceneEglSurface() const {
        return m_surface;
    }
    /**
     * Sets the first @p surface used by the compositing scene.
     * @see sceneEglSurface
     */
    void setSceneEglSurface(EGLSurface surface) {
        m_surface = surface;
    }

    /**
     * The EglConfig used by the compositing scene.
     */
    EGLConfig sceneEglConfig() const {
        return m_eglConfig;
    }
    /**
     * Sets the @p config used by the compositing scene.
     * @see sceneEglConfig
     */
    void setSceneEglConfig(EGLConfig config) {
        m_eglConfig = config;
    }

    /**
     * Implementing subclasses should provide a size in case the backend represents
     * a basic screen and uses the BasicScreens.
     *
     * Base implementation returns an invalid size.
     */
    virtual QSize screenSize() const;
    /**
     * Implementing subclasses should provide all geometries in case the backend represents
     * a basic screen and uses the BasicScreens.
     *
     * Base implementation returns one QRect positioned at 0/0 with screenSize() as size.
     */
    virtual QVector<QRect> screenGeometries() const;

    /**
     * Implementing subclasses should provide all geometries in case the backend represents
     * a basic screen and uses the BasicScreens.
     *
     * Base implementation returns a screen with a scale of 1.
     */
    virtual QVector<qreal> screenScales() const;
    /**
     * Implement this method to receive configuration change requests through KWayland's
     * OutputManagement interface.
     *
     * Base implementation warns that the current backend does not implement this
     * functionality.
     */
    void requestOutputsChange(KWaylandServer::OutputConfigurationInterface *config);

    /**
     * Whether the Platform requires compositing for rendering.
     * Default implementation returns @c true. If the implementing Platform allows to be used
     * without compositing (e.g. rendering is done by the windowing system), re-implement this method.
     */
    virtual bool requiresCompositing() const;
    /**
     * Whether Compositing is possible in the Platform.
     * Returning @c false in this method makes only sense if requiresCompositing returns @c false.
     *
     * The default implementation returns @c true.
     * @see requiresCompositing
     */
    virtual bool compositingPossible() const;
    /**
     * Returns a user facing text explaining why compositing is not possible in case
     * compositingPossible returns @c false.
     *
     * The default implementation returns an empty string.
     * @see compositingPossible
     */
    virtual QString compositingNotPossibleReason() const;
    /**
     * Whether OpenGL compositing is broken.
     * The Platform can implement this method if it is able to detect whether OpenGL compositing
     * broke (e.g. triggered a crash in a previous run).
     *
     * Default implementation returns @c false.
     * @see createOpenGLSafePoint
     */
    virtual bool openGLCompositingIsBroken() const;
    enum class OpenGLSafePoint {
        PreInit,
        PostInit,
        PreFrame,
        PostFrame,
        PostLastGuardedFrame
    };
    /**
     * This method is invoked before and after creating the OpenGL rendering Scene.
     * An implementing Platform can use it to detect crashes triggered by the OpenGL implementation.
     * This can be used for openGLCompositingIsBroken.
     *
     * The default implementation does nothing.
     * @see openGLCompositingIsBroken.
     */
    virtual void createOpenGLSafePoint(OpenGLSafePoint safePoint);

    /**
     * Starts an interactive window selection process.
     *
     * Once the user selected a window the @p callback is invoked with the selected Toplevel as
     * argument. In case the user cancels the interactive window selection or selecting a window is currently
     * not possible (e.g. screen locked) the @p callback is invoked with a @c nullptr argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor unless
     * @p cursorName is provided. The argument @p cursorName is a QByteArray instead of Qt::CursorShape
     * to support the "pirate" cursor for kill window which is not wrapped by Qt::CursorShape.
     *
     * The default implementation forwards to InputRedirection.
     *
     * @param callback The function to invoke once the interactive window selection ends
     * @param cursorName The optional name of the cursor shape to use, default is crosshair
     */
    virtual void startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName = QByteArray());

    /**
     * Starts an interactive position selection process.
     *
     * Once the user selected a position on the screen the @p callback is invoked with
     * the selected point as argument. In case the user cancels the interactive position selection
     * or selecting a position is currently not possible (e.g. screen locked) the @p callback
     * is invoked with a point at @c -1 as x and y argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor.
     *
     * The default implementation forwards to InputRedirection.
     *
     * @param callback The function to invoke once the interactive position selection ends
     */
    virtual void startInteractivePositionSelection(std::function<void(const QPoint &)> callback);

    /**
     * Platform specific preparation for an @p action which is used for KGlobalAccel.
     *
     * A platform might need to do preparation for an @p action before
     * it can be used with KGlobalAccel.
     *
     * Code using KGlobalAccel should invoke this method for the @p action
     * prior to setting up any shortcuts and connections.
     *
     * The default implementation does nothing.
     *
     * @param action The action which will be used with KGlobalAccel.
     * @since 5.10
     */
    virtual void setupActionForGlobalAccel(QAction *action);

    bool usesSoftwareCursor() const {
        return m_softWareCursor;
    }

    /**
     * Returns a PlatformCursorImage. By default this is created by softwareCursor and
     * softwareCursorHotspot. An implementing subclass can use this to provide a better
     * suited PlatformCursorImage.
     *
     * @see softwareCursor
     * @see softwareCursorHotspot
     * @since 5.9
     */
    virtual PlatformCursorImage cursorImage() const;

    /**
     * The Platform cursor image should be hidden.
     * @see showCursor
     * @see doHideCursor
     * @see isCursorHidden
     * @since 5.9
     */
    void hideCursor();

    /**
     * The Platform cursor image should be shown again.
     * @see hideCursor
     * @see doShowCursor
     * @see isCursorHidden
     * @since 5.9
     */
    void showCursor();

    /**
     * Whether the cursor is currently hidden.
     * @see showCursor
     * @see hideCursor
     * @since 5.9
     */
    bool isCursorHidden() const {
        return m_hideCursorCounter > 0;
    }
    bool isReady() const {
        return m_ready;
    }
    void setInitialWindowSize(const QSize &size) {
        m_initialWindowSize = size;
    }
    void setDeviceIdentifier(const QByteArray &identifier) {
        m_deviceIdentifier = identifier;
    }
    bool supportsPointerWarping() const {
        return m_pointerWarping;
    }
    bool areOutputsEnabled() const {
        return m_outputsEnabled;
    }
    void setOutputsEnabled(bool enabled) {
        m_outputsEnabled = enabled;
    }
    int initialOutputCount() const {
        return m_initialOutputCount;
    }
    void setInitialOutputCount(int count) {
        m_initialOutputCount = count;
    }
    qreal initialOutputScale() const {
        return m_initialOutputScale;
    }
    void setInitialOutputScale(qreal scale) {
        m_initialOutputScale = scale;
    }

    /**
     * Creates the OverlayWindow required for X11 based compositors.
     * Default implementation returns @c nullptr.
     */
    virtual OverlayWindow *createOverlayWindow();

    /**
     * Queries the current X11 time stamp of the X server.
     */
    void updateXTime();

    /**
     * Creates the OutlineVisual for the given @p outline.
     * Default implementation creates an OutlineVisual suited for composited usage.
     */
    virtual OutlineVisual *createOutline(Outline *outline);

    /**
     * Creates the Decoration::Renderer for the given @p client.
     *
     * The default implementation creates a Renderer suited for the Compositor, @c nullptr if there is no Compositor.
     */
    virtual Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *client);

    /**
     * Platform specific way to invert the screen.
     * Default implementation invokes the invert effect
     */
    virtual void invertScreen();

    /**
     * Default implementation creates an EffectsHandlerImp;
     */
    virtual void createEffectsHandler(Compositor *compositor, Scene *scene);

    /**
     * The CompositingTypes supported by the Platform.
     * The first item should be the most preferred one.
     * @since 5.11
     */
    virtual QVector<CompositingType> supportedCompositors() const = 0;

    /**
     * Whether gamma control is supported by the backend.
     * @since 5.12
     */
    bool supportsGammaControl() const {
        return m_supportsGammaControl;
    }

    ColorCorrect::Manager *colorCorrectManager() {
        return m_colorCorrect;
    }

    // outputs with connections (org_kde_kwin_outputdevice)
    virtual Outputs outputs() const {
        return Outputs();
    }
    // actively compositing outputs (wl_output)
    virtual Outputs enabledOutputs() const {
        return Outputs();
    }
    AbstractOutput *findOutput(int screenId);
    AbstractOutput *findOutput(const QByteArray &uuid);

    /**
     * A string of information to include in kwin debug output
     * It should not be translated.
     *
     * The base implementation prints the name.
     * @since 5.12
     */
    virtual QString supportInformation() const;

    /**
     * The compositor plugin which got selected from @ref supportedCompositors.
     * Prior to selecting a compositor this returns @c NoCompositing.
     *
     * This method allows the platforms to limit the offerings in @ref supportedCompositors
     * in case they do not support runtime compositor switching
     */
    CompositingType selectedCompositor() const
    {
        return m_selectedCompositor;
    }
    /**
     * Used by Compositor to set the used compositor.
     */
    void setSelectedCompositor(CompositingType type)
    {
        m_selectedCompositor = type;
    }

public Q_SLOTS:
    void pointerMotion(const QPointF &position, quint32 time);
    void pointerButtonPressed(quint32 button, quint32 time);
    void pointerButtonReleased(quint32 button, quint32 time);
    void pointerAxisHorizontal(qreal delta, quint32 time, qint32 discreteDelta = 0,
        InputRedirection::PointerAxisSource source = InputRedirection::PointerAxisSourceUnknown);
    void pointerAxisVertical(qreal delta, quint32 time, qint32 discreteDelta = 0,
        InputRedirection::PointerAxisSource source = InputRedirection::PointerAxisSourceUnknown);
    void keyboardKeyPressed(quint32 key, quint32 time);
    void keyboardKeyReleased(quint32 key, quint32 time);
    void keyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void keymapChange(int fd, uint32_t size);
    void touchDown(qint32 id, const QPointF &pos, quint32 time);
    void touchUp(qint32 id, quint32 time);
    void touchMotion(qint32 id, const QPointF &pos, quint32 time);
    void touchCancel();
    void touchFrame();

    void processSwipeGestureBegin(int fingerCount, quint32 time);
    void processSwipeGestureUpdate(const QSizeF &delta, quint32 time);
    void processSwipeGestureEnd(quint32 time);
    void processSwipeGestureCancelled(quint32 time);
    void processPinchGestureBegin(int fingerCount, quint32 time);
    void processPinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time);
    void processPinchGestureEnd(quint32 time);
    void processPinchGestureCancelled(quint32 time);

    void cursorRendered(const QRect &geometry);

Q_SIGNALS:
    void screensQueried();
    void initFailed();
    void readyChanged(bool);
    /**
     * Emitted by backends using a one screen (nested window) approach and when the size of that changes.
     */
    void screenSizeChanged();

protected:
    explicit Platform(QObject *parent = nullptr);
    void setSoftWareCursor(bool set);
    void repaint(const QRect &rect);
    void setReady(bool ready);
    QSize initialWindowSize() const {
        return m_initialWindowSize;
    }
    QByteArray deviceIdentifier() const {
        return m_deviceIdentifier;
    }
    void setSupportsPointerWarping(bool set) {
        m_pointerWarping = set;
    }
    void setSupportsGammaControl(bool set) {
        m_supportsGammaControl = set;
    }

    /**
     * Whether the backend is supposed to change the configuration of outputs.
     */
    void supportsOutputChanges() {
        m_supportsOutputChanges = true;
    }

    /**
     * Actual platform specific way to hide the cursor.
     * Sub-classes need to implement if they support hiding the cursor.
     *
     * This method is invoked by hideCursor if the cursor needs to be hidden.
     * The default implementation does nothing.
     *
     * @see doShowCursor
     * @see hideCursor
     * @see showCursor
     */
    virtual void doHideCursor();
    /**
     * Actual platform specific way to show the cursor.
     * Sub-classes need to implement if they support showing the cursor.
     *
     * This method is invoked by showCursor if the cursor needs to be shown again.
     *
     * @see doShowCursor
     * @see hideCursor
     * @see showCursor
     */
    virtual void doShowCursor();

private:
    void triggerCursorRepaint();
    bool m_softWareCursor = false;
    struct {
        QRect lastRenderedGeometry;
    } m_cursor;
    bool m_ready = false;
    QSize m_initialWindowSize;
    QByteArray m_deviceIdentifier;
    bool m_pointerWarping = false;
    bool m_outputsEnabled = true;
    int m_initialOutputCount = 1;
    qreal m_initialOutputScale = 1;
    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig = nullptr;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLContext m_globalShareContext = EGL_NO_CONTEXT;
    EGLSurface m_surface = EGL_NO_SURFACE;
    int m_hideCursorCounter = 0;
    ColorCorrect::Manager *m_colorCorrect = nullptr;
    bool m_supportsGammaControl = false;
    bool m_supportsOutputChanges = false;
    CompositingType m_selectedCompositor = NoCompositing;
};

}

Q_DECLARE_INTERFACE(KWin::Platform, "org.kde.kwin.Platform")

#endif
