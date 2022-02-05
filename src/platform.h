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
#include "input.h"

#include <QImage>
#include <QObject>

#include <functional>

class QAction;

namespace KWaylandServer {
class OutputConfigurationV2Interface;
}

namespace KWin
{

class AbstractOutput;
class Edge;
class Compositor;
class DmaBufTexture;
class InputBackend;
class OverlayWindow;
class OpenGLBackend;
class Outline;
class OutlineVisual;
class QPainterBackend;
class Scene;
class ScreenEdges;
class Session;
class Toplevel;
class WaylandOutputConfig;

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

    virtual Session *session() const = 0;
    virtual bool initialize() = 0;
    virtual InputBackend *createInputBackend();
    virtual OpenGLBackend *createOpenGLBackend();
    virtual QPainterBackend *createQPainterBackend();
    virtual DmaBufTexture *createDmaBufTexture(const QSize &size) {
        Q_UNUSED(size);
        return nullptr;
    }

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
     * Whether our Compositing EGL display supports creating native EGL fences.
     */
    bool supportsNativeFence() const;
    /**
     * The EGLDisplay used by the compositing scene.
     */
    EGLDisplay sceneEglDisplay() const;
    void setSceneEglDisplay(EGLDisplay display);
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
     * Implementing subclasses should provide a size in case the backend represents
     * a basic screen and uses the BasicScreens.
     *
     * Base implementation returns an invalid size.
     */
    virtual QSize screenSize() const;
    /**
     * Implement this method to receive configuration change requests through KWayland's
     * OutputManagement interface.
     *
     * Base implementation warns that the current backend does not implement this
     * functionality.
     */
    void requestOutputsChange(KWaylandServer::OutputConfigurationV2Interface *config);

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

    // outputs with connections (org_kde_kwin_outputdevice)
    virtual Outputs outputs() const {
        return Outputs();
    }
    // actively compositing outputs (wl_output)
    virtual Outputs enabledOutputs() const {
        return Outputs();
    }
    AbstractOutput *findOutput(int screenId) const;
    AbstractOutput *findOutput(const QUuid &uuid) const;
    AbstractOutput *findOutput(const QString &name) const;
    AbstractOutput *outputAt(const QPoint &pos) const;

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

    /**
     * Returns @c true if rendering is split per screen; otherwise returns @c false.
     */
    bool isPerScreenRenderingEnabled() const;

    virtual AbstractOutput *createVirtualOutput(const QString &name, const QSize &size, qreal scaling);
    virtual void removeVirtualOutput(AbstractOutput *output);

    /**
     * @returns the primary output amomg the enabled outputs
     */
    AbstractOutput *primaryOutput() const {
        return m_primaryOutput;
    }

    /**
     * Assigns a the @p primary output among the enabled outputs
     */
    void setPrimaryOutput(AbstractOutput *primary);

    /**
     * Applies the output changes. Default implementation only sets values common between platforms
     */
    virtual bool applyOutputChanges(const WaylandOutputConfig &config);

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
    void cancelTouchSequence();
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
    virtual void sceneInitialized() {};

Q_SIGNALS:
    void screensQueried();
    void readyChanged(bool);
    /**
     * This signal is emitted when an output has been connected. The @a output is not ready
     * for compositing yet.
     */
    void outputAdded(AbstractOutput *output);
    /**
     * This signal is emitted when an output has been disconnected.
     */
    void outputRemoved(AbstractOutput *output);
    /**
     * This signal is emitted when the @a output has become activated and it is ready for
     * compositing.
     */
    void outputEnabled(AbstractOutput *output);
    /**
     * This signal is emitted when the @a output has been deactivated and it is no longer
     * being composited. The outputDisabled() signal is guaranteed to be emitted before the
     * output is removed.
     *
     * @see outputEnabled, outputRemoved
     */
    void outputDisabled(AbstractOutput *output);

    void primaryOutputChanged(AbstractOutput *primaryOutput);

protected:
    explicit Platform(QObject *parent = nullptr);
    void repaint(const QRect &rect);
    void setReady(bool ready);
    void setPerScreenRenderingEnabled(bool enabled);
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

private:
    void triggerCursorRepaint();
    struct {
        QRect lastRenderedGeometry;
    } m_cursor;
    bool m_ready = false;
    QSize m_initialWindowSize;
    QByteArray m_deviceIdentifier;
    bool m_pointerWarping = false;
    int m_initialOutputCount = 1;
    qreal m_initialOutputScale = 1;
    EGLDisplay m_eglDisplay;
    EGLContext m_globalShareContext = EGL_NO_CONTEXT;
    bool m_supportsGammaControl = false;
    bool m_supportsOutputChanges = false;
    bool m_isPerScreenRenderingEnabled = false;
    CompositingType m_selectedCompositor = NoCompositing;
    AbstractOutput *m_primaryOutput = nullptr;
};

}

Q_DECLARE_INTERFACE(KWin::Platform, "org.kde.kwin.Platform")

#endif
