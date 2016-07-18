/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_PLATFORM_H
#define KWIN_PLATFORM_H
#include <kwin_export.h>
#include <epoxy/egl.h>
#include <fixx11h.h>
#include <QImage>
#include <QObject>

namespace KWayland {
    namespace Server {
        class OutputConfigurationInterface;
    }
}

namespace KWin
{

class Edge;
class OpenGLBackend;
class QPainterBackend;
class Screens;
class ScreenEdges;
class WaylandCursorTheme;

class KWIN_EXPORT Platform : public QObject
{
    Q_OBJECT
public:
    virtual ~Platform();

    virtual void init() = 0;
    virtual Screens *createScreens(QObject *parent = nullptr);
    virtual OpenGLBackend *createOpenGLBackend();
    virtual QPainterBackend *createQPainterBackend();
    /**
     * Allows the platform to create a platform specific screen edge.
     * The default implementation creates a Edge.
     **/
    virtual Edge *createScreenEdge(ScreenEdges *parent);
    virtual void warpPointer(const QPointF &globalPos);
    /**
     * Whether our Compositing EGL display allows a surface less context
     * so that a sharing context could be created.
     **/
    virtual bool supportsQpaContext() const;
    /**
     * The EGLDisplay used by the compositing scene.
     **/
    EGLDisplay sceneEglDisplay() const;
    void setSceneEglDisplay(EGLDisplay display);
    /**
     * The EGLContext used by the compositing scene.
     **/
    virtual EGLContext sceneEglContext() const;

    /**
     * Implementing subclasses should provide a size in case the backend represents
     * a basic screen and uses the BasicScreens.
     *
     * Base implementation returns an invalid size.
     **/
    virtual QSize screenSize() const;
    /**
     * Implementing subclasses should provide all geometries in case the backend represents
     * a basic screen and uses the BasicScreens.
     *
     * Base implementation returns one QRect positioned at 0/0 with screenSize() as size.
     **/
    virtual QVector<QRect> screenGeometries() const;
    /**
     * Implement this method to receive configuration change requests through KWayland's
     * OutputManagement interface.
     *
     * Base implementation warns that the current backend does not implement this
     * functionality.
     */
    virtual void configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config);

    /**
     * Whether the Platform requires compositing for rendering.
     * Default implementation returns @c true. If the implementing Platform allows to be used
     * without compositing (e.g. rendering is done by the windowing system), re-implement this method.
     **/
    virtual bool requiresCompositing() const;
    /**
     * Whether Compositing is possible in the Platform.
     * Returning @c false in this method makes only sense if @link{requiresCompositing} returns @c false.
     *
     * The default implementation returns @c true.
     * @see requiresCompositing
     **/
    virtual bool compositingPossible() const;
    /**
     * Returns a user facing text explaining why compositing is not possible in case
     * @link{compositingPossible} returns @c false.
     *
     * The default implementation returns an empty string.
     * @see compositingPossible
     **/
    virtual QString compositingNotPossibleReason() const;
    /**
     * Whether OpenGL compositing is broken.
     * The Platform can implement this method if it is able to detect whether OpenGL compositing
     * broke (e.g. triggered a crash in a previous run).
     *
     * Default implementation returns @c false.
     * @see createOpenGLSafePoint
     **/
    virtual bool openGLCompositingIsBroken() const;
    enum class OpenGLSafePoint {
        PreInit,
        PostInit
    };
    /**
     * This method is invoked before and after creating the OpenGL rendering Scene.
     * An implementing Platform can use it to detect crashes triggered by the OpenGL implementation.
     * This can be used for @link{openGLCompositingIsBroken}.
     *
     * The default implementation does nothing.
     * @see openGLCompositingIsBroken.
     **/
    virtual void createOpenGLSafePoint(OpenGLSafePoint safePoint);

    bool usesSoftwareCursor() const {
        return m_softWareCursor;
    }
    QImage softwareCursor() const;
    QPoint softwareCursorHotspot() const;
    void markCursorAsRendered();

    bool handlesOutputs() const {
        return m_handlesOutputs;
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

public Q_SLOTS:
    void pointerMotion(const QPointF &position, quint32 time);
    void pointerButtonPressed(quint32 button, quint32 time);
    void pointerButtonReleased(quint32 button, quint32 time);
    void pointerAxisHorizontal(qreal delta, quint32 time);
    void pointerAxisVertical(qreal delta, quint32 time);
    void keyboardKeyPressed(quint32 key, quint32 time);
    void keyboardKeyReleased(quint32 key, quint32 time);
    void keyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void keymapChange(int fd, uint32_t size);
    void touchDown(qint32 id, const QPointF &pos, quint32 time);
    void touchUp(qint32 id, quint32 time);
    void touchMotion(qint32 id, const QPointF &pos, quint32 time);
    void touchCancel();
    void touchFrame();

Q_SIGNALS:
    void screensQueried();
    void initFailed();
    void cursorChanged();
    void readyChanged(bool);
    /**
     * Emitted by backends using a one screen (nested window) approach and when the size of that changes.
     **/
    void screenSizeChanged();

protected:
    explicit Platform(QObject *parent = nullptr);
    void setSoftWareCursor(bool set);
    void handleOutputs() {
        m_handlesOutputs = true;
    }
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

private:
    void triggerCursorRepaint();
    bool m_softWareCursor = false;
    struct {
        QRect lastRenderedGeometry;
    } m_cursor;
    bool m_handlesOutputs = false;
    bool m_ready = false;
    QSize m_initialWindowSize;
    QByteArray m_deviceIdentifier;
    bool m_pointerWarping = false;
    bool m_outputsEnabled = true;
    int m_initialOutputCount = 1;
    EGLDisplay m_eglDisplay;
};

}

Q_DECLARE_INTERFACE(KWin::Platform, "org.kde.kwin.Platform")

#endif
