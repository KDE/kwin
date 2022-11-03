/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_PLATFORM_H
#define KWIN_PLATFORM_H
#include <epoxy/egl.h>
#include <kwin_export.h>
#include <kwinglobals.h>

#include <QImage>
#include <QObject>

#include <functional>
#include <memory>
#include <optional>

namespace KWin
{

class Window;
class Output;
class Edge;
class Compositor;
class DmaBufTexture;
class InputBackend;
class OpenGLBackend;
class Outline;
class OutlineVisual;
class QPainterBackend;
class Scene;
class ScreenEdges;
class OutputConfiguration;
struct DmaBufParams;

class KWIN_EXPORT Outputs : public QVector<Output *>
{
public:
    Outputs(){};
    template<typename T>
    Outputs(const QVector<T> &other)
    {
        resize(other.size());
        std::copy(other.constBegin(), other.constEnd(), begin());
    }
};

class KWIN_EXPORT Platform : public QObject
{
    Q_OBJECT
public:
    ~Platform() override;

    virtual bool initialize() = 0;
    virtual std::unique_ptr<InputBackend> createInputBackend();
    virtual std::unique_ptr<OpenGLBackend> createOpenGLBackend();
    virtual std::unique_ptr<QPainterBackend> createQPainterBackend();
    virtual std::optional<DmaBufParams> testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers);
    virtual std::shared_ptr<DmaBufTexture> createDmaBufTexture(const QSize &size, quint32 format, const uint64_t modifier);
    std::shared_ptr<DmaBufTexture> createDmaBufTexture(const DmaBufParams &attributes);

    /**
     * Allows the platform to create a platform specific screen edge.
     * The default implementation creates a Edge.
     */
    virtual std::unique_ptr<Edge> createScreenEdge(ScreenEdges *parent);
    /**
     * Allows the platform to create a platform specific Cursor.
     * The default implementation creates an InputRedirectionCursor.
     */
    virtual void createPlatformCursor(QObject *parent = nullptr);
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
     * Starts an interactive window selection process.
     *
     * Once the user selected a window the @p callback is invoked with the selected Window as
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
    virtual void startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName = QByteArray());

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
     * Returns a PlatformCursorImage. By default this is created by softwareCursor and
     * softwareCursorHotspot. An implementing subclass can use this to provide a better
     * suited PlatformCursorImage.
     *
     * @see softwareCursor
     * @see softwareCursorHotspot
     * @since 5.9
     */
    virtual PlatformCursorImage cursorImage() const;

    bool isReady() const
    {
        return m_ready;
    }
    void setInitialWindowSize(const QSize &size)
    {
        m_initialWindowSize = size;
    }
    void setDeviceIdentifier(const QByteArray &identifier)
    {
        m_deviceIdentifier = identifier;
    }
    int initialOutputCount() const
    {
        return m_initialOutputCount;
    }
    void setInitialOutputCount(int count)
    {
        m_initialOutputCount = count;
    }
    qreal initialOutputScale() const
    {
        return m_initialOutputScale;
    }
    void setInitialOutputScale(qreal scale)
    {
        m_initialOutputScale = scale;
    }

    /**
     * Creates the OutlineVisual for the given @p outline.
     * Default implementation creates an OutlineVisual suited for composited usage.
     */
    virtual std::unique_ptr<OutlineVisual> createOutline(Outline *outline);

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
    bool supportsGammaControl() const
    {
        return m_supportsGammaControl;
    }

    virtual Outputs outputs() const = 0;
    Output *findOutput(const QString &name) const;

    /**
     * A string of information to include in kwin debug output
     * It should not be translated.
     *
     * The base implementation prints the name.
     * @since 5.12
     */
    virtual QString supportInformation() const;

    virtual Output *createVirtualOutput(const QString &name, const QSize &size, qreal scale);
    virtual void removeVirtualOutput(Output *output);

    /**
     * Applies the output changes. Default implementation only sets values common between platforms
     */
    virtual bool applyOutputChanges(const OutputConfiguration &config);

public Q_SLOTS:
    virtual void sceneInitialized(){};

Q_SIGNALS:
    void outputsQueried();
    void readyChanged(bool);
    /**
     * This signal is emitted when an output has been connected. The @a output is not ready
     * for compositing yet.
     */
    void outputAdded(Output *output);
    /**
     * This signal is emitted when an output has been disconnected.
     */
    void outputRemoved(Output *output);

protected:
    explicit Platform(QObject *parent = nullptr);
    void setReady(bool ready);
    QSize initialWindowSize() const
    {
        return m_initialWindowSize;
    }
    QByteArray deviceIdentifier() const
    {
        return m_deviceIdentifier;
    }
    void setSupportsGammaControl(bool set)
    {
        m_supportsGammaControl = set;
    }

private:
    bool m_ready = false;
    QSize m_initialWindowSize;
    QByteArray m_deviceIdentifier;
    int m_initialOutputCount = 1;
    qreal m_initialOutputScale = 1;
    EGLDisplay m_eglDisplay;
    EGLContext m_globalShareContext = EGL_NO_CONTEXT;
    bool m_supportsGammaControl = false;
};

} // namespace KWin

#endif
