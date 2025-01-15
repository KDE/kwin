/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/offscreenquickview.h"
#include "effect/effecthandler.h"

#include "logging_p.h"
#include "opengl/glutils.h"
#include "opengl/openglcontext.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickView>
#include <QStyleHints>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickGraphicsDevice>
#include <QQuickOpenGLUtils>
#include <QQuickRenderTarget>
#include <QTimer>
#include <private/qeventpoint_p.h> // for QMutableEventPoint

namespace KWin
{

class Q_DECL_HIDDEN OffscreenQuickView::Private
{
public:
    std::unique_ptr<QQuickWindow> m_view;
    std::unique_ptr<QQuickRenderControl> m_renderControl;
    std::unique_ptr<QOffscreenSurface> m_offscreenSurface;
    std::unique_ptr<QOpenGLContext> m_glcontext;
    std::unique_ptr<QOpenGLFramebufferObject> m_fbo;

    std::unique_ptr<QTimer> m_repaintTimer;
    QImage m_image;
    std::unique_ptr<GLTexture> m_textureExport;
    // if we should capture a QImage after rendering into our BO.
    // Used for either software QtQuick rendering and nonGL kwin rendering
    bool m_useBlit = false;
    bool m_visible = true;
    bool m_hasAlphaChannel = true;
    bool m_automaticRepaint = true;

    std::optional<qreal> m_explicitDpr;

    QList<QEventPoint> touchPoints;
    QSet<uint32_t> acceptedTouchPoints;
    QPointingDevice *touchDevice;

    ulong lastMousePressTime = 0;
    Qt::MouseButton lastMousePressButton = Qt::NoButton;

    void releaseResources();

    void updateTouchState(Qt::TouchPointState state, qint32 id, const QPointF &pos);
};

class Q_DECL_HIDDEN OffscreenQuickScene::Private
{
public:
    Private()
    {
    }

    std::unique_ptr<QQmlComponent> qmlComponent;
    std::unique_ptr<QQuickItem> quickItem;
};

OffscreenQuickView::OffscreenQuickView(ExportMode exportMode, bool alpha)
    : d(new OffscreenQuickView::Private)
{
    d->m_renderControl = std::make_unique<QQuickRenderControl>();

    d->m_view = std::make_unique<QQuickWindow>(d->m_renderControl.get());
    Q_ASSERT(d->m_view->setProperty("_KWIN_WINDOW_IS_OFFSCREEN", true) || true);
    d->m_view->setFlags(Qt::FramelessWindowHint);
    d->m_view->setColor(Qt::transparent);

    d->m_hasAlphaChannel = alpha;
    if (exportMode == ExportMode::Image) {
        d->m_useBlit = true;
    }

    const bool usingGl = d->m_view->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;

    if (!usingGl) {
        qCDebug(LIBKWINEFFECTS) << "QtQuick Software rendering mode detected";
        d->m_useBlit = true;
        // explicilty do not call QQuickRenderControl::initialize, see Qt docs
    } else {
        QSurfaceFormat format;
        format.setOption(QSurfaceFormat::ResetNotification);
        format.setDepthBufferSize(16);
        format.setStencilBufferSize(8);
        if (alpha) {
            format.setAlphaBufferSize(8);
        }

        d->m_view->setFormat(format);

        auto shareContext = QOpenGLContext::globalShareContext();
        d->m_glcontext = std::make_unique<QOpenGLContext>();
        d->m_glcontext->setShareContext(shareContext);
        d->m_glcontext->setFormat(format);
        d->m_glcontext->create();

        // and the offscreen surface
        d->m_offscreenSurface = std::make_unique<QOffscreenSurface>();
        d->m_offscreenSurface->setFormat(d->m_glcontext->format());
        d->m_offscreenSurface->create();

        d->m_glcontext->makeCurrent(d->m_offscreenSurface.get());
        d->m_view->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(d->m_glcontext.get()));
        d->m_renderControl->initialize();
        d->m_glcontext->doneCurrent();

        // On Wayland, contexts are implicitly shared and QOpenGLContext::globalShareContext() is null.
        if (shareContext && !d->m_glcontext->shareContext()) {
            qCDebug(LIBKWINEFFECTS) << "Failed to create a shared context, falling back to raster rendering";
            // still render via GL, but blit for presentation
            d->m_useBlit = true;
        }
    }

    auto updateSize = [this]() {
        contentItem()->setSize(d->m_view->size());
    };
    updateSize();
    connect(d->m_view.get(), &QWindow::widthChanged, this, updateSize);
    connect(d->m_view.get(), &QWindow::heightChanged, this, updateSize);

    d->m_repaintTimer = std::make_unique<QTimer>();
    d->m_repaintTimer->setSingleShot(true);
    d->m_repaintTimer->setInterval(10);

    connect(d->m_repaintTimer.get(), &QTimer::timeout, this, &OffscreenQuickView::update);
    connect(d->m_renderControl.get(), &QQuickRenderControl::renderRequested, this, &OffscreenQuickView::handleRenderRequested);
    connect(d->m_renderControl.get(), &QQuickRenderControl::sceneChanged, this, &OffscreenQuickView::handleSceneChanged);

    d->touchDevice = new QPointingDevice(QStringLiteral("ForwardingTouchDevice"), {}, QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 10, {});
}

OffscreenQuickView::~OffscreenQuickView()
{
    disconnect(d->m_renderControl.get(), &QQuickRenderControl::renderRequested, this, &OffscreenQuickView::handleRenderRequested);
    disconnect(d->m_renderControl.get(), &QQuickRenderControl::sceneChanged, this, &OffscreenQuickView::handleSceneChanged);

    if (d->m_glcontext) {
        // close the view whilst we have an active GL context
        d->m_glcontext->makeCurrent(d->m_offscreenSurface.get());
    }

    d->m_view.reset();
    d->m_renderControl.reset();
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

void OffscreenQuickView::setDevicePixelRatio(qreal dpr)
{
    d->m_explicitDpr = dpr;
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

    bool usingGl = d->m_glcontext != nullptr;
    OpenGlContext *previousContext = OpenGlContext::currentContext();

    if (usingGl) {
        if (!d->m_glcontext->makeCurrent(d->m_offscreenSurface.get())) {
            // probably a context loss event, kwin is about to reset all the effects anyway
            return;
        }

        qreal dpr = d->m_view->screen() ? d->m_view->screen()->devicePixelRatio() : 1.0;
        if (d->m_explicitDpr.has_value()) {
            dpr = d->m_explicitDpr.value();
        }

        const QSize nativeSize = d->m_view->size() * dpr;
        if (!d->m_fbo || d->m_fbo->size() != nativeSize) {
            d->m_textureExport.reset(nullptr);

            QOpenGLFramebufferObjectFormat fboFormat;
            fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            fboFormat.setInternalTextureFormat(GL_RGBA8);

            d->m_fbo = std::make_unique<QOpenGLFramebufferObject>(nativeSize, fboFormat);
            if (!d->m_fbo->isValid()) {
                d->m_fbo.reset();
                d->m_glcontext->doneCurrent();
                return;
            }
        }

        QQuickRenderTarget renderTarget = QQuickRenderTarget::fromOpenGLTexture(d->m_fbo->texture(), d->m_fbo->size());
        renderTarget.setDevicePixelRatio(dpr);
        d->m_view->setRenderTarget(renderTarget);
    }

    d->m_renderControl->polishItems();
    if (usingGl) {
        d->m_renderControl->beginFrame();
    }
    d->m_renderControl->sync();
    d->m_renderControl->render();
    if (usingGl) {
        d->m_renderControl->endFrame();
    }

    if (usingGl) {
        QQuickOpenGLUtils::resetOpenGLState();
    }

    if (d->m_useBlit) {
        if (usingGl) {
            d->m_image = d->m_fbo->toImage();
            d->m_image.setDevicePixelRatio(d->m_view->effectiveDevicePixelRatio());
        } else {
            d->m_image = d->m_view->grabWindow();
        }
    }

    if (usingGl) {
        QOpenGLFramebufferObject::bindDefault();
        d->m_glcontext->doneCurrent();
        if (previousContext) {
            previousContext->makeCurrent();
        }
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
    case QEvent::MouseButtonRelease: {
        QMouseEvent *me = static_cast<QMouseEvent *>(e);
        const QPoint widgetPos = d->m_view->mapFromGlobal(me->pos());
        QMouseEvent cloneEvent(me->type(), widgetPos, me->pos(), me->button(), me->buttons(), me->modifiers());
        cloneEvent.setAccepted(false);
        QCoreApplication::sendEvent(d->m_view.get(), &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());

        if (e->type() == QEvent::MouseButtonPress) {
            const ulong doubleClickInterval = static_cast<ulong>(QGuiApplication::styleHints()->mouseDoubleClickInterval());
            const bool doubleClick = (me->timestamp() - d->lastMousePressTime < doubleClickInterval) && me->button() == d->lastMousePressButton;
            d->lastMousePressTime = me->timestamp();
            d->lastMousePressButton = me->button();
            if (doubleClick) {
                d->lastMousePressButton = Qt::NoButton;
                QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick, me->position(), me->globalPosition(), me->button(), me->buttons(), me->modifiers());
                QCoreApplication::sendEvent(d->m_view.get(), &doubleClickEvent);
            }
        }

        return;
    }
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove: {
        QHoverEvent *he = static_cast<QHoverEvent *>(e);
        const QPointF widgetPos = d->m_view->mapFromGlobal(he->pos());
        const QPointF oldWidgetPos = d->m_view->mapFromGlobal(he->oldPos());
        QHoverEvent cloneEvent(he->type(), widgetPos, oldWidgetPos, he->modifiers());
        cloneEvent.setAccepted(false);
        QCoreApplication::sendEvent(d->m_view.get(), &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());
        return;
    }
    case QEvent::Wheel: {
        QWheelEvent *we = static_cast<QWheelEvent *>(e);
        const QPointF widgetPos = d->m_view->mapFromGlobal(we->position().toPoint());
        QWheelEvent cloneEvent(widgetPos, we->globalPosition(), we->pixelDelta(), we->angleDelta(), we->buttons(),
                               we->modifiers(), we->phase(), we->inverted());
        cloneEvent.setAccepted(false);
        QCoreApplication::sendEvent(d->m_view.get(), &cloneEvent);
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
    QCoreApplication::sendEvent(d->m_view.get(), keyEvent);
}

bool OffscreenQuickView::forwardTouchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    d->updateTouchState(Qt::TouchPointPressed, id, pos);

    QTouchEvent event(QEvent::TouchBegin, d->touchDevice, Qt::NoModifier, d->touchPoints);
    event.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    event.setAccepted(false);
    QCoreApplication::sendEvent(d->m_view.get(), &event);

    const bool ret = event.isAccepted();
    if (ret) {
        d->acceptedTouchPoints.insert(id);
    }
    return ret;
}

bool OffscreenQuickView::forwardTouchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    d->updateTouchState(Qt::TouchPointMoved, id, pos);

    if (!d->acceptedTouchPoints.contains(id)) {
        return false;
    }

    QTouchEvent event(QEvent::TouchUpdate, d->touchDevice, Qt::NoModifier, d->touchPoints);
    event.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    event.setAccepted(false);
    QCoreApplication::sendEvent(d->m_view.get(), &event);

    return event.isAccepted();
}

bool OffscreenQuickView::forwardTouchUp(qint32 id, std::chrono::microseconds time)
{
    d->updateTouchState(Qt::TouchPointReleased, id, QPointF{});

    if (!d->acceptedTouchPoints.contains(id)) {
        return false;
    }

    QTouchEvent event(QEvent::TouchEnd, d->touchDevice, Qt::NoModifier, d->touchPoints);
    event.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    event.setAccepted(false);
    QCoreApplication::sendEvent(d->m_view.get(), &event);

    d->acceptedTouchPoints.remove(id);

    return event.isAccepted();
}

void OffscreenQuickView::forwardTouchCancel()
{
    d->acceptedTouchPoints.clear();
    d->touchPoints.clear();
    QTouchEvent event(QEvent::TouchCancel, d->touchDevice, Qt::NoModifier, d->touchPoints);
    event.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    event.setAccepted(false);
    QCoreApplication::sendEvent(d->m_view.get(), &event);
}

QRect OffscreenQuickView::geometry() const
{
    return d->m_view->geometry();
}

void OffscreenQuickView::setOpacity(qreal opacity)
{
    d->m_view->setOpacity(opacity);
}

qreal OffscreenQuickView::opacity() const
{
    return d->m_view->opacity();
}

bool OffscreenQuickView::hasAlphaChannel() const
{
    return d->m_hasAlphaChannel;
}

QQuickItem *OffscreenQuickView::contentItem() const
{
    return d->m_view->contentItem();
}

QQuickWindow *OffscreenQuickView::window() const
{
    return d->m_view.get();
}

void OffscreenQuickView::setVisible(bool visible)
{
    if (d->m_visible == visible) {
        return;
    }
    d->m_visible = visible;

    if (visible) {
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
        d->m_textureExport = GLTexture::upload(d->m_image);
    } else {
        if (!d->m_fbo) {
            return nullptr;
        }
        if (!d->m_textureExport) {
            d->m_textureExport = GLTexture::createNonOwningWrapper(d->m_fbo->texture(), d->m_fbo->format().internalTextureFormat(), d->m_fbo->size());
        }
    }
    return d->m_textureExport.get();
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
    // QWindow::setGeometry() won't sync output if there's no platform window.
    d->m_view->setScreen(QGuiApplication::screenAt(rect.center()));
    Q_EMIT geometryChanged(oldGeometry, rect);
}

void OffscreenQuickView::Private::releaseResources()
{
    if (m_glcontext) {
        m_glcontext->makeCurrent(m_offscreenSurface.get());
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
                          if (point.state() == QEventPoint::Released) {
                              return true;
                          }
                          QMutableEventPoint::setState(point, QEventPoint::Stationary);
                          return false;
                      }),
                      touchPoints.end());

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
        QMutableEventPoint::setState(point, QEventPoint::Pressed);
        QMutableEventPoint::setId(point, id + idOffset);
        QMutableEventPoint::setGlobalPosition(point, pos);
        QMutableEventPoint::setScenePosition(point, m_view->mapFromGlobal(pos.toPoint()));
        QMutableEventPoint::setPosition(point, m_view->mapFromGlobal(pos.toPoint()));

        touchPoints.append(point);
    } break;
    case Qt::TouchPointMoved: {
        if (changed == touchPoints.end()) {
            return;
        }

        auto &point = *changed;
        QMutableEventPoint::setGlobalLastPosition(point, point.globalPosition());
        QMutableEventPoint::setState(point, QEventPoint::Updated);
        QMutableEventPoint::setScenePosition(point, m_view->mapFromGlobal(pos.toPoint()));
        QMutableEventPoint::setPosition(point, m_view->mapFromGlobal(pos.toPoint()));
        QMutableEventPoint::setGlobalPosition(point, pos);
    } break;
    case Qt::TouchPointReleased: {
        if (changed == touchPoints.end()) {
            return;
        }

        auto &point = *changed;
        QMutableEventPoint::setGlobalLastPosition(point, point.globalPosition());
        QMutableEventPoint::setState(point, QEventPoint::Released);
    } break;
    default:
        break;
    }
}

OffscreenQuickScene::OffscreenQuickScene(OffscreenQuickView::ExportMode exportMode, bool alpha)
    : OffscreenQuickView(exportMode, alpha)
    , d(new OffscreenQuickScene::Private)
{
}

OffscreenQuickScene::~OffscreenQuickScene() = default;

void OffscreenQuickScene::setSource(const QUrl &source)
{
    setSource(source, QVariantMap());
}

void OffscreenQuickScene::setSource(const QUrl &source, const QVariantMap &initialProperties)
{
    if (!d->qmlComponent) {
        d->qmlComponent = std::make_unique<QQmlComponent>(effects->qmlEngine());
    }

    d->qmlComponent->loadUrl(source);
    if (d->qmlComponent->isError()) {
        qCWarning(LIBKWINEFFECTS).nospace() << "Failed to load effect quick view " << source << ": " << d->qmlComponent->errors();
        d->qmlComponent.reset();
        return;
    }

    d->quickItem.reset();

    std::unique_ptr<QObject> qmlObject(d->qmlComponent->createWithInitialProperties(initialProperties));
    QQuickItem *item = qobject_cast<QQuickItem *>(qmlObject.get());
    if (!item) {
        qCWarning(LIBKWINEFFECTS) << "Root object of effect quick view" << source << "is not a QQuickItem";
        return;
    }

    qmlObject.release();
    d->quickItem.reset(item);

    item->setParentItem(contentItem());

    auto updateSize = [item, this]() {
        item->setSize(contentItem()->size());
    };
    updateSize();
    connect(contentItem(), &QQuickItem::widthChanged, item, updateSize);
    connect(contentItem(), &QQuickItem::heightChanged, item, updateSize);
}

QQuickItem *OffscreenQuickScene::rootItem() const
{
    return d->quickItem.get();
}

} // namespace KWin

#include "moc_offscreenquickview.cpp"
