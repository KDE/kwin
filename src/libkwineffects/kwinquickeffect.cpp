/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinquickeffect.h"

#include "logging_p.h"
#include "sharedqmlengine.h"

#include <QQmlEngine>
#include <QQmlIncubator>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

namespace KWin
{

class QuickSceneViewIncubator : public QQmlIncubator
{
public:
    QuickSceneViewIncubator(const std::function<void(QuickSceneViewIncubator *)> &statusChangedCallback)
        : QQmlIncubator(QQmlIncubator::Asynchronous)
        , m_statusChangedCallback(statusChangedCallback)
    {
    }

    void statusChanged(QQmlIncubator::Status status) override
    {
        m_statusChangedCallback(this);
    }

private:
    std::function<void(QuickSceneViewIncubator *)> m_statusChangedCallback;
};

class QuickSceneEffectPrivate
{
public:
    static QuickSceneEffectPrivate *get(QuickSceneEffect *effect)
    {
        return effect->d.get();
    }
    bool isItemOnScreen(QQuickItem *item, EffectScreen *screen) const;

    SharedQmlEngine::Ptr qmlEngine;
    std::unique_ptr<QQmlComponent> qmlComponent;
    QUrl source;
    std::map<EffectScreen *, std::unique_ptr<QQmlIncubator>> incubators;
    std::map<EffectScreen *, std::unique_ptr<QuickSceneView>> views;
    QPointer<QuickSceneView> mouseImplicitGrab;
    bool running = false;
    std::unique_ptr<QWindow> dummyWindow;
};

bool QuickSceneEffectPrivate::isItemOnScreen(QQuickItem *item, EffectScreen *screen) const
{
    if (!item || !screen || !views.contains(screen)) {
        return false;
    }

    const auto &view = views.at(screen);
    return item->window() == view->window();
}

QuickSceneView::QuickSceneView(QuickSceneEffect *effect, EffectScreen *screen)
    : OffscreenQuickView(effect, QuickSceneEffectPrivate::get(effect)->dummyWindow.get())
    , m_effect(effect)
    , m_screen(screen)
{
    setGeometry(screen->geometry());
    connect(screen, &EffectScreen::geometryChanged, this, [this, screen]() {
        setGeometry(screen->geometry());
    });
}

QuickSceneView::~QuickSceneView()
{
}

QQuickItem *QuickSceneView::rootItem() const
{
    return m_rootItem.get();
}

void QuickSceneView::setRootItem(QQuickItem *item)
{
    Q_ASSERT_X(item, "setRootItem", "root item cannot be null");
    m_rootItem.reset(item);
    m_rootItem->setParentItem(contentItem());

    auto updateSize = [this]() {
        m_rootItem->setSize(contentItem()->size());
    };
    updateSize();
    connect(contentItem(), &QQuickItem::widthChanged, m_rootItem.get(), updateSize);
    connect(contentItem(), &QQuickItem::heightChanged, m_rootItem.get(), updateSize);
}

QuickSceneEffect *QuickSceneView::effect() const
{
    return m_effect;
}

EffectScreen *QuickSceneView::screen() const
{
    return m_screen;
}

bool QuickSceneView::isDirty() const
{
    return m_dirty;
}

void QuickSceneView::markDirty()
{
    m_dirty = true;
}

void QuickSceneView::resetDirty()
{
    m_dirty = false;
}

void QuickSceneView::scheduleRepaint()
{
    markDirty();
    effects->addRepaint(geometry());
}

QuickSceneEffect::QuickSceneEffect(QObject *parent)
    : Effect(parent)
    , d(new QuickSceneEffectPrivate)
{
}

QuickSceneEffect::~QuickSceneEffect()
{
}

bool QuickSceneEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void QuickSceneEffect::checkItemDraggedOutOfScreen(QQuickItem *item)
{
    const QRectF globalGeom = QRectF(item->mapToGlobal(QPointF(0, 0)), QSizeF(item->width(), item->height()));
    QList<EffectScreen *> screens;

    for (const auto &[screen, view] : d->views) {
        if (!d->isItemOnScreen(item, screen) && screen->geometry().intersects(globalGeom.toRect())) {
            screens << screen;
        }
    }

    Q_EMIT itemDraggedOutOfScreen(item, screens);
}

void QuickSceneEffect::checkItemDroppedOutOfScreen(const QPointF &globalPos, QQuickItem *item)
{
    const auto it = std::find_if(d->views.begin(), d->views.end(), [this, globalPos, item](const auto &view) {
        EffectScreen *screen = view.first;
        return !d->isItemOnScreen(item, screen) && screen->geometry().contains(globalPos.toPoint());
    });
    if (it != d->views.end()) {
        Q_EMIT itemDroppedOutOfScreen(globalPos, item, it->first);
    }
}

bool QuickSceneEffect::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::CursorChange) {
        if (const QWindow *window = qobject_cast<QWindow *>(watched)) {
            effects->defineCursor(window->cursor().shape());
        }
    }
    return false;
}

bool QuickSceneEffect::isRunning() const
{
    return d->running;
}

void QuickSceneEffect::setRunning(bool running)
{
    if (d->running != running) {
        if (running) {
            startInternal();
        } else {
            stopInternal();
        }
    }
}

QUrl QuickSceneEffect::source() const
{
    return d->source;
}

void QuickSceneEffect::setSource(const QUrl &url)
{
    if (isRunning()) {
        qWarning() << "Cannot change QuickSceneEffect.source while running";
        return;
    }
    if (d->source != url) {
        d->source = url;
        d->qmlComponent.reset();
    }
}

QuickSceneView *QuickSceneEffect::viewForScreen(EffectScreen *screen) const
{
    const auto it = d->views.find(screen);
    return it == d->views.end() ? nullptr : it->second.get();
}

QuickSceneView *QuickSceneEffect::viewAt(const QPoint &pos) const
{
    const auto it = std::find_if(d->views.begin(), d->views.end(), [pos](const auto &view) {
        return view.second->geometry().contains(pos);
    });
    return it == d->views.end() ? nullptr : it->second.get();
}

QuickSceneView *QuickSceneEffect::activeView() const
{
    auto it = std::find_if(d->views.begin(), d->views.end(), [](const auto &view) {
        return view.second->window()->activeFocusItem();
    });
    if (it == d->views.end()) {
        it = d->views.find(effects->activeScreen());
    }
    return it == d->views.end() ? nullptr : it->second.get();
}

KWin::QuickSceneView *QuickSceneEffect::getView(Qt::Edge edge)
{
    auto screenView = activeView();

    QuickSceneView *candidate = nullptr;

    for (const auto &[screen, view] : d->views) {
        switch (edge) {
        case Qt::LeftEdge:
            if (view->geometry().left() < screenView->geometry().left()) {
                // Look for the nearest view from the current
                if (!candidate || view->geometry().left() > candidate->geometry().left() || (view->geometry().left() == candidate->geometry().left() && view->geometry().top() > candidate->geometry().top())) {
                    candidate = view.get();
                }
            }
            break;
        case Qt::TopEdge:
            if (view->geometry().top() < screenView->geometry().top()) {
                if (!candidate || view->geometry().top() > candidate->geometry().top() || (view->geometry().top() == candidate->geometry().top() && view->geometry().left() > candidate->geometry().left())) {
                    candidate = view.get();
                }
            }
            break;
        case Qt::RightEdge:
            if (view->geometry().right() > screenView->geometry().right()) {
                if (!candidate || view->geometry().right() < candidate->geometry().right() || (view->geometry().right() == candidate->geometry().right() && view->geometry().top() > candidate->geometry().top())) {
                    candidate = view.get();
                }
            }
            break;
        case Qt::BottomEdge:
            if (view->geometry().bottom() > screenView->geometry().bottom()) {
                if (!candidate || view->geometry().bottom() < candidate->geometry().bottom() || (view->geometry().bottom() == candidate->geometry().bottom() && view->geometry().left() > candidate->geometry().left())) {
                    candidate = view.get();
                }
            }
            break;
        }
    }

    return candidate;
}

void QuickSceneEffect::activateView(QuickSceneView *view)
{
    if (!view) {
        return;
    }

    auto *av = activeView();
    // Already properly active?
    if (view == av && av->window()->activeFocusItem()) {
        return;
    }

    for (const auto &[screen, otherView] : d->views) {
        if (otherView.get() == view && !view->window()->activeFocusItem()) {
            QFocusEvent focusEvent(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
            qApp->sendEvent(view->window(), &focusEvent);
        } else if (otherView.get() != view && otherView->window()->activeFocusItem()) {
            QFocusEvent focusEvent(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
            qApp->sendEvent(otherView->window(), &focusEvent);
        }
    }

    Q_EMIT activeViewChanged(view);
}

// Screen views are repainted just before kwin performs its compositing cycle to avoid stalling for vblank
void QuickSceneEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (effects->waylandDisplay()) {
        const auto it = d->views.find(data.screen);
        if (it != d->views.end() && it->second->isDirty()) {
            it->second->update();
            it->second->resetDirty();
        }
    } else {
        for (const auto &[screen, screenView] : d->views) {
            if (screenView->isDirty()) {
                screenView->update();
                screenView->resetDirty();
            }
        }
    }
}

void QuickSceneEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    if (effects->waylandDisplay()) {
        const auto it = d->views.find(data.screen());
        if (it != d->views.end()) {
            effects->renderOffscreenQuickView(it->second.get());
        }
    } else {
        for (const auto &[screen, screenView] : d->views) {
            effects->renderOffscreenQuickView(screenView.get());
        }
    }
}

bool QuickSceneEffect::isActive() const
{
    return !d->views.empty() && !effects->isScreenLocked();
}

QVariantMap QuickSceneEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap();
}

void QuickSceneEffect::handleScreenAdded(EffectScreen *screen)
{
    addScreen(screen);
}

void QuickSceneEffect::handleScreenRemoved(EffectScreen *screen)
{
    d->views.erase(screen);
    d->incubators.erase(screen);
}

void QuickSceneEffect::addScreen(EffectScreen *screen)
{
    auto properties = initialProperties(screen);
    properties["width"] = screen->geometry().width();
    properties["height"] = screen->geometry().height();

    auto incubator = new QuickSceneViewIncubator([this, screen](QuickSceneViewIncubator *incubator) {
        if (incubator->isReady()) {
            auto view = new QuickSceneView(this, screen);
            view->setRootItem(qobject_cast<QQuickItem *>(incubator->object()));
            if (view->contentItem()) {
                view->contentItem()->setFocus(false);
            }
            view->setAutomaticRepaint(false);
            connect(view, &QuickSceneView::repaintNeeded, this, [view]() {
                effects->addRepaint(view->geometry());
            });
            connect(view, &QuickSceneView::renderRequested, view, &QuickSceneView::scheduleRepaint);
            connect(view, &QuickSceneView::sceneChanged, view, &QuickSceneView::scheduleRepaint);
            view->scheduleRepaint();
            d->views[screen].reset(view);
        } else if (incubator->isError()) {
            qCWarning(LIBKWINEFFECTS) << "Could not create a view for QML file" << d->qmlComponent->url();
            qCWarning(LIBKWINEFFECTS) << incubator->errors();
        }
    });

    incubator->setInitialProperties(properties);
    d->incubators[screen].reset(incubator);
    d->qmlComponent->create(*incubator);
}

void QuickSceneEffect::startInternal()
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (Q_UNLIKELY(d->source.isEmpty())) {
        qWarning() << "QuickSceneEffect.source is empty. Did you forget to call setSource()?";
        return;
    }

    if (!d->qmlEngine) {
        d->qmlEngine = SharedQmlEngine::engine();
    }

    if (!d->qmlComponent) {
        d->qmlComponent.reset(new QQmlComponent(d->qmlEngine.get()));
        d->qmlComponent->loadUrl(d->source);
        if (d->qmlComponent->isError()) {
            qWarning().nospace() << "Failed to load " << d->source << ": " << d->qmlComponent->errors();
            d->qmlComponent.reset();
            return;
        }
    }

    effects->setActiveFullScreenEffect(this);
    d->running = true;

    // Install an event filter to monitor cursor shape changes.
    qApp->installEventFilter(this);

    // This is an ugly hack to make hidpi rendering work as expected on wayland until we switch
    // to Qt 6.3 or newer. See https://codereview.qt-project.org/c/qt/qtdeclarative/+/361506
    if (effects->waylandDisplay()) {
        d->dummyWindow.reset(new QWindow());
        d->dummyWindow->setOpacity(0);
        d->dummyWindow->resize(1, 1);
        d->dummyWindow->setFlag(Qt::FramelessWindowHint);
        d->dummyWindow->setVisible(true);
        d->dummyWindow->requestActivate();
    }

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        addScreen(screen);
    }

    // Ensure one view has an active focus item
    activateView(activeView());

    connect(effects, &EffectsHandler::screenAdded, this, &QuickSceneEffect::handleScreenAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &QuickSceneEffect::handleScreenRemoved);

    effects->grabKeyboard(this);
    effects->startMouseInterception(this, Qt::ArrowCursor);
}

void QuickSceneEffect::stopInternal()
{
    disconnect(effects, &EffectsHandler::screenAdded, this, &QuickSceneEffect::handleScreenAdded);
    disconnect(effects, &EffectsHandler::screenRemoved, this, &QuickSceneEffect::handleScreenRemoved);

    d->incubators.clear();
    d->views.clear();
    d->dummyWindow.reset();
    d->running = false;
    qApp->removeEventFilter(this);
    effects->ungrabKeyboard();
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);
    effects->addRepaintFull();
}

void QuickSceneEffect::windowInputMouseEvent(QEvent *event)
{
    Qt::MouseButtons buttons;
    QPoint globalPosition;
    if (QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event)) {
        buttons = mouseEvent->buttons();
        globalPosition = mouseEvent->globalPos();
    } else if (QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent *>(event)) {
        buttons = wheelEvent->buttons();
        globalPosition = wheelEvent->globalPosition().toPoint();
    } else {
        return;
    }

    if (buttons) {
        if (!d->mouseImplicitGrab) {
            d->mouseImplicitGrab = viewAt(globalPosition);
        }
    }

    QuickSceneView *target = d->mouseImplicitGrab;
    if (!target) {
        target = viewAt(globalPosition);
    }

    if (!buttons) {
        d->mouseImplicitGrab = nullptr;
    }

    if (target) {
        if (buttons) {
            activateView(target);
        }
        target->forwardMouseEvent(event);
    }
}

void QuickSceneEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    auto *screenView = activeView();

    if (screenView) {
        // ActiveView may not have an activeFocusItem yet
        activateView(screenView);
        screenView->forwardKeyEvent(keyEvent);
    }
}

bool QuickSceneEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    for (const auto &[screen, screenView] : d->views) {
        if (screenView->geometry().contains(pos.toPoint())) {
            activateView(screenView.get());
            return screenView->forwardTouchDown(id, pos, time);
        }
    }
    return false;
}

bool QuickSceneEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    for (const auto &[screen, screenView] : d->views) {
        if (screenView->geometry().contains(pos.toPoint())) {
            return screenView->forwardTouchMotion(id, pos, time);
        }
    }
    return false;
}

bool QuickSceneEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    for (const auto &[screen, screenView] : d->views) {
        if (screenView->forwardTouchUp(id, time)) {
            return true;
        }
    }
    return false;
}

} // namespace KWin

#include <moc_kwinquickeffect.cpp>
