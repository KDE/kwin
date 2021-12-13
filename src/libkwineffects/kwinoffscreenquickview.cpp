/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinoffscreenquickview.h"

#include "kwinglutils.h"
#include "logging_p.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickView>
#include <QQuickRenderControl>
#include <QStyleHints>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QTimer>

#include <KDeclarative/QmlObjectSharedEngine>

namespace KWin
{

class EffectQuickRenderControl : public QQuickRenderControl
{
    Q_OBJECT

public:
    explicit EffectQuickRenderControl(QWindow *renderWindow, QObject *parent = nullptr)
        : QQuickRenderControl(parent)
        , m_renderWindow(renderWindow)
    {
    }

    QWindow *renderWindow(QPoint *offset) override
    {
        if (offset) {
            *offset = QPoint(0, 0);
        }
        return m_renderWindow;
    }

private:
    QPointer<QWindow> m_renderWindow;
};

class Q_DECL_HIDDEN OffscreenQuickView::Private
{
public:
    QQuickWindow *m_view;
    QQuickRenderControl *m_renderControl;
    QScopedPointer<QOffscreenSurface> m_offscreenSurface;
    QScopedPointer<QOpenGLContext> m_glcontext;
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;

    QTimer *m_repaintTimer;
    QImage m_image;
    QScopedPointer<GLTexture> m_textureExport;
    // if we should capture a QImage after rendering into our BO.
    // Used for either software QtQuick rendering and nonGL kwin rendering
    bool m_useBlit = false;
    bool m_visible = true;
    bool m_automaticRepaint = true;

    QList<QTouchEvent::TouchPoint> touchPoints;
    Qt::TouchPointStates touchState;
    QTouchDevice *touchDevice;

    ulong lastMousePressTime = 0;
    Qt::MouseButton lastMousePressButton = Qt::NoButton;

    void releaseResources();

    void updateTouchState(Qt::TouchPointState state, qint32 id, const QPointF& pos);
};

class Q_DECL_HIDDEN OffscreenQuickScene::Private
{
public:
    KDeclarative::QmlObjectSharedEngine *qmlObject = nullptr;
};

OffscreenQuickView::OffscreenQuickView(QObject *parent)
    : OffscreenQuickView(parent, effects ? ExportMode::Texture : ExportMode::Image)
{
}

OffscreenQuickView::OffscreenQuickView(QObject *parent, ExportMode exportMode)
    : OffscreenQuickView(parent, nullptr, exportMode)
{
}

OffscreenQuickView::OffscreenQuickView(QObject *parent, QWindow *renderWindow)
    : OffscreenQuickView(parent, renderWindow, effects ? ExportMode::Texture : ExportMode::Image)
{
}

OffscreenQuickView::OffscreenQuickView(QObject *parent, QWindow *renderWindow, ExportMode exportMode)
    : QObject(parent)
    , d(new OffscreenQuickView::Private)
{
    d->m_renderControl = new EffectQuickRenderControl(renderWindow, this);

    d->m_view = new QQuickWindow(d->m_renderControl);
    d->m_view->setFlags(Qt::FramelessWindowHint);
    d->m_view->setColor(Qt::transparent);

    if (exportMode == ExportMode::Image) {
        d->m_useBlit = true;
    }

    const bool usingGl = d->m_view->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;

    if (!usingGl) {
        qCDebug(LIBKWINEFFECTS) << "QtQuick Software rendering mode detected";
        d->m_useBlit = true;
        d->m_renderControl->initialize(nullptr);
    } else {
        QSurfaceFormat format;
        format.setOption(QSurfaceFormat::ResetNotification);
        format.setDepthBufferSize(16);
        format.setStencilBufferSize(8);

        auto shareContext = QOpenGLContext::globalShareContext();
        d->m_glcontext.reset(new QOpenGLContext);
        d->m_glcontext->setShareContext(shareContext);
        d->m_glcontext->setFormat(format);
        d->m_glcontext->create();

        // and the offscreen surface
        d->m_offscreenSurface.reset(new QOffscreenSurface);
        d->m_offscreenSurface->setFormat(d->m_glcontext->format());
        d->m_offscreenSurface->create();

        d->m_glcontext->makeCurrent(d->m_offscreenSurface.data());
        d->m_renderControl->initialize(d->m_glcontext.data());
        d->m_glcontext->doneCurrent();

        // On Wayland, contexts are implicitly shared and QOpenGLContext::globalShareContext() is null.
        if (shareContext && !d->m_glcontext->shareContext()) {
            qCDebug(LIBKWINEFFECTS) << "Failed to create a shared context, falling back to raster rendering";
            // still render via GL, but blit for presentation
            d->m_useBlit = true;
        }
    }

    auto updateSize = [this]() { contentItem()->setSize(d->m_view->size()); };
    updateSize();
    connect(d->m_view, &QWindow::widthChanged, this, updateSize);
    connect(d->m_view, &QWindow::heightChanged, this, updateSize);

    d->m_repaintTimer = new QTimer(this);
    d->m_repaintTimer->setSingleShot(true);
    d->m_repaintTimer->setInterval(10);

    connect(d->m_repaintTimer, &QTimer::timeout, this, &OffscreenQuickView::update);
    connect(d->m_renderControl, &QQuickRenderControl::renderRequested, this, &OffscreenQuickView::handleRenderRequested);
    connect(d->m_renderControl, &QQuickRenderControl::sceneChanged, this, &OffscreenQuickView::handleSceneChanged);

    d->touchDevice = new QTouchDevice{};
    d->touchDevice->setCapabilities(QTouchDevice::Position);
    d->touchDevice->setType(QTouchDevice::TouchScreen);
    d->touchDevice->setMaximumTouchPoints(10);
}

OffscreenQuickView::~OffscreenQuickView()
{
    if (d->m_glcontext) {
        // close the view whilst we have an active GL context
        d->m_glcontext->makeCurrent(d->m_offscreenSurface.data());
    }

    delete d->m_renderControl; // Always delete render control first.
    d->m_renderControl = nullptr;

    delete d->m_view;
    d->m_view = nullptr;
}

bool OffscreenQuickView::automaticRepaint() const
{
    return d->m_automaticRepaint;
}

void OffscreenQuickView::setAutomaticRepaint(bool set)
{
    if (d->m_automaticRepaint != set) {
        d->m_automaticRepaint = set;

        // If there's an in-flight update, disable it.
        if (!d->m_automaticRepaint) {
            d->m_repaintTimer->stop();
        }
    }
}

void OffscreenQuickView::handleSceneChanged()
{
    if (d->m_automaticRepaint) {
        d->m_repaintTimer->start();
    }
    Q_EMIT sceneChanged();
}

void OffscreenQuickView::handleRenderRequested()
{
    if (d->m_automaticRepaint) {
        d->m_repaintTimer->start();
    }
    Q_EMIT renderRequested();
}

void OffscreenQuickView::update()
{
    if (!d->m_visible) {
        return;
    }
    if (d->m_view->size().isEmpty()) {
        return;
    }

    bool usingGl = d->m_glcontext;

    if (usingGl) {
        if (!d->m_glcontext->makeCurrent(d->m_offscreenSurface.data())) {
            // probably a context loss event, kwin is about to reset all the effects anyway
            return;
        }

        const QSize nativeSize = d->m_view->size() * d->m_view->effectiveDevicePixelRatio();
        if (d->m_fbo.isNull() || d->m_fbo->size() != nativeSize) {
            d->m_textureExport.reset(nullptr);
            d->m_fbo.reset(new QOpenGLFramebufferObject(nativeSize, QOpenGLFramebufferObject::CombinedDepthStencil));
            if (!d->m_fbo->isValid()) {
                d->m_fbo.reset();
                d->m_glcontext->doneCurrent();
                return;
            }
        }
        d->m_view->setRenderTarget(d->m_fbo.data());
    }

    d->m_renderControl->polishItems();
    d->m_renderControl->sync();

    d->m_renderControl->render();
    if (usingGl) {
        d->m_view->resetOpenGLState();
    }

    if (d->m_useBlit) {
        d->m_image = d->m_renderControl->grab();
    }

    if (usingGl) {
        QOpenGLFramebufferObject::bindDefault();
        d->m_glcontext->doneCurrent();
    }
    Q_EMIT repaintNeeded();
}

void OffscreenQuickView::forwardMouseEvent(QEvent *e)
{
    if (!d->m_visible) {
        return;
    }
    switch (e->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(e);
        const QPoint widgetPos = d->m_view->mapFromGlobal(me->pos());
        QMouseEvent cloneEvent(me->type(), widgetPos, me->pos(), me->button(), me->buttons(), me->modifiers());
        QCoreApplication::sendEvent(d->m_view, &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());

        if (e->type() == QEvent::MouseButtonPress) {
            const ulong doubleClickInterval = static_cast<ulong>(QGuiApplication::styleHints()->mouseDoubleClickInterval());
            const bool doubleClick = (me->timestamp() - d->lastMousePressTime < doubleClickInterval) && me->button() == d->lastMousePressButton;
            d->lastMousePressTime = me->timestamp();
            d->lastMousePressButton = me->button();
            if (doubleClick) {
                d->lastMousePressButton = Qt::NoButton;
                QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick, me->localPos(), me->windowPos(), me->screenPos(), me->button(), me->buttons(), me->modifiers());
                QCoreApplication::sendEvent(d->m_view, &doubleClickEvent);
            }
        }

        return;
    }
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:
    {
        QHoverEvent *he = static_cast<QHoverEvent *>(e);
        const QPointF widgetPos = d->m_view->mapFromGlobal(he->pos());
        const QPointF oldWidgetPos = d->m_view->mapFromGlobal(he->oldPos());
        QHoverEvent cloneEvent(he->type(), widgetPos, oldWidgetPos, he->modifiers());
        QCoreApplication::sendEvent(d->m_view, &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());
        return;
    }
    case QEvent::Wheel:
    {
        QWheelEvent *we = static_cast<QWheelEvent *>(e);
        const QPointF widgetPos = d->m_view->mapFromGlobal(we->pos());
        QWheelEvent cloneEvent(widgetPos, we->globalPosF(), we->pixelDelta(), we->angleDelta(), we->buttons(),
                               we->modifiers(), we->phase(), we->inverted());
        QCoreApplication::sendEvent(d->m_view, &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());
        return;
    }
    default:
        return;
    }
}

void OffscreenQuickView::forwardKeyEvent(QKeyEvent *keyEvent)
{
    if (!d->m_visible) {
        return;
    }
    QCoreApplication::sendEvent(d->m_view, keyEvent);
}

bool OffscreenQuickView::forwardTouchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(time)

    d->updateTouchState(Qt::TouchPointPressed, id, pos);

    QTouchEvent event(QEvent::TouchBegin, d->touchDevice, Qt::NoModifier, d->touchState, d->touchPoints);
    QCoreApplication::sendEvent(d->m_view, &event);

    return event.isAccepted();
}

bool OffscreenQuickView::forwardTouchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(time)

    d->updateTouchState(Qt::TouchPointMoved, id, pos);

    QTouchEvent event(QEvent::TouchUpdate, d->touchDevice, Qt::NoModifier, d->touchState, d->touchPoints);
    QCoreApplication::sendEvent(d->m_view, &event);

    return event.isAccepted();
}

bool OffscreenQuickView::forwardTouchUp(qint32 id, quint32 time)
{
    Q_UNUSED(time)

    d->updateTouchState(Qt::TouchPointReleased, id, QPointF{});

    QTouchEvent event(QEvent::TouchEnd, d->touchDevice, Qt::NoModifier, d->touchState, d->touchPoints);
    QCoreApplication::sendEvent(d->m_view, &event);

    return event.isAccepted();
}

QRect OffscreenQuickView::geometry() const
{
    return d->m_view->geometry();
}

QQuickItem *OffscreenQuickView::contentItem() const
{
    return d->m_view->contentItem();
}

void OffscreenQuickView::setVisible(bool visible)
{
    if (d->m_visible == visible) {
        return;
    }
    d->m_visible = visible;

    if (visible){
        Q_EMIT d->m_renderControl->renderRequested();
    } else {
        // deferred to not change GL context
        QTimer::singleShot(0, this, [this]() {
            d->releaseResources();
        });
    }
}

bool OffscreenQuickView::isVisible() const
{
    return d->m_visible;
}

void OffscreenQuickView::show()
{
    setVisible(true);
}

void OffscreenQuickView::hide()
{
    setVisible(false);
}

GLTexture *OffscreenQuickView::bufferAsTexture()
{
    if (d->m_useBlit) {
        if (d->m_image.isNull()) {
            return nullptr;
        }
        d->m_textureExport.reset(new GLTexture(d->m_image));
    } else {
        if (!d->m_fbo) {
            return nullptr;
        }
        if (!d->m_textureExport) {
            d->m_textureExport.reset(new GLTexture(d->m_fbo->texture(), d->m_fbo->format().internalTextureFormat(), d->m_fbo->size()));
        }
    }
    return d->m_textureExport.data();
}

QImage OffscreenQuickView::bufferAsImage() const
{
    return d->m_image;
}

QSize OffscreenQuickView::size() const
{
    return d->m_view->geometry().size();
}

void OffscreenQuickView::setGeometry(const QRect &rect)
{
    const QRect oldGeometry = d->m_view->geometry();
    d->m_view->setGeometry(rect);
    Q_EMIT geometryChanged(oldGeometry, rect);
}

void OffscreenQuickView::Private::releaseResources()
{
    if (m_glcontext) {
        m_glcontext->makeCurrent(m_offscreenSurface.data());
        m_view->releaseResources();
        m_glcontext->doneCurrent();
    } else {
        m_view->releaseResources();
    }
}

void OffscreenQuickView::Private::updateTouchState(Qt::TouchPointState state, qint32 id, const QPointF &pos)
{
    // Remove the points that were previously in a released state, since they
    // are no longer relevant. Additionally, reset the state of all remaining
    // points to Stationary so we only have one touch point with a different
    // state.
    touchPoints.erase(std::remove_if(touchPoints.begin(), touchPoints.end(), [](QTouchEvent::TouchPoint &point) {
        if (point.state() == Qt::TouchPointReleased) {
            return true;
        }
        point.setState(Qt::TouchPointStationary);
        return false;
    }), touchPoints.end());

    // QtQuick Pointer Handlers incorrectly consider a touch point with ID 0
    // to be an invalid touch point. This has been fixed in Qt 6 but could not
    // be fixed for Qt 5. Instead, we offset kwin's internal IDs with this
    // offset to trick QtQuick into treating them as valid points.
    static const qint32 idOffset = 111;

    // Find the touch point that has changed. This is separate from the above
    // loop because removing the released touch points invalidates iterators.
    auto changed = std::find_if(touchPoints.begin(), touchPoints.end(), [id](const QTouchEvent::TouchPoint &point) {
        return point.id() == id + idOffset;
    });

    switch (state) {
    case Qt::TouchPointPressed: {
            if (changed != touchPoints.end()) {
                return;
            }

            QTouchEvent::TouchPoint point;
            point.setId(id + idOffset);
            point.setState(Qt::TouchPointPressed);
            point.setScreenPos(pos);
            point.setScenePos(m_view->mapFromGlobal(pos.toPoint()));
            point.setPos(m_view->mapFromGlobal(pos.toPoint()));

            touchPoints.append(point);
        }
        break;
    case Qt::TouchPointMoved: {
            if (changed == touchPoints.end()) {
                return;
            }

            auto &point = *changed;
            point.setLastPos(point.pos());
            point.setLastScenePos(point.scenePos());
            point.setLastScreenPos(point.screenPos());
            point.setScenePos(m_view->mapFromGlobal(pos.toPoint()));
            point.setPos(m_view->mapFromGlobal(pos.toPoint()));
            point.setScreenPos(pos);
            point.setState(Qt::TouchPointMoved);
        }
        break;
    case Qt::TouchPointReleased: {
            if (changed == touchPoints.end()) {
                return;
            }

            auto &point = *changed;
            point.setLastPos(point.pos());
            point.setLastScreenPos(point.screenPos());
            point.setState(Qt::TouchPointReleased);
        }
        break;
    default:
        break;
    }

    // The touch state value is used in QTouchEvent and includes all the states
    // that the current touch points are in.
    touchState = std::accumulate(touchPoints.begin(), touchPoints.end(), Qt::TouchPointStates{}, [](auto init, const auto &point) {
        return init | point.state();
    });
}

OffscreenQuickScene::OffscreenQuickScene(QObject *parent)
    : OffscreenQuickView(parent)
    , d(new OffscreenQuickScene::Private)
{
    d->qmlObject = new KDeclarative::QmlObjectSharedEngine(this);
}

OffscreenQuickScene::OffscreenQuickScene(QObject *parent, QWindow *renderWindow)
    : OffscreenQuickView(parent, renderWindow)
    , d(new OffscreenQuickScene::Private)
{
    d->qmlObject = new KDeclarative::QmlObjectSharedEngine(this);
}

OffscreenQuickScene::OffscreenQuickScene(QObject *parent, QWindow *renderWindow, ExportMode exportMode)
    : OffscreenQuickView(parent, renderWindow, exportMode)
    , d(new OffscreenQuickScene::Private)
{
    d->qmlObject = new KDeclarative::QmlObjectSharedEngine(this);
}

OffscreenQuickScene::OffscreenQuickScene(QObject *parent, OffscreenQuickView::ExportMode exportMode)
    : OffscreenQuickView(parent, exportMode)
    , d(new OffscreenQuickScene::Private)
{
    d->qmlObject = new KDeclarative::QmlObjectSharedEngine(this);
}

OffscreenQuickScene::~OffscreenQuickScene()
{
    delete d->qmlObject;
}

void OffscreenQuickScene::setSource(const QUrl &source)
{
    d->qmlObject->setSource(source);

    QQuickItem *item = rootItem();
    if (!item) {
        qCDebug(LIBKWINEFFECTS) << "Could not load effect quick view" << source;
        return;
    }
    item->setParentItem(contentItem());

    auto updateSize = [item, this]() { item->setSize(contentItem()->size()); };
    updateSize();
    connect(contentItem(), &QQuickItem::widthChanged, item, updateSize);
    connect(contentItem(), &QQuickItem::heightChanged, item, updateSize);
}

QQmlContext *OffscreenQuickScene::rootContext() const
{
    return d->qmlObject->rootContext();
}

QQuickItem *OffscreenQuickScene::rootItem() const
{
    return qobject_cast<QQuickItem *>(d->qmlObject->rootObject());
}

} // namespace KWin

#include "kwinoffscreenquickview.moc"
