/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"
#include "expoarea.h"
#include "expolayout.h"
#include "overviewconfig.h"

#include <KDeclarative/QmlObjectSharedEngine>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QQmlComponent>
#include <QQuickItem>
#include <QTimer>
#include <QWindow>

namespace KWin
{

OverviewScreenView::OverviewScreenView(QQmlComponent *component, EffectScreen *screen, QWindow *renderWindow, OverviewEffect *effect)
    : OffscreenQuickView(effect, renderWindow)
{
    const QVariantMap initialProperties {
        { QStringLiteral("effect"), QVariant::fromValue(effect) },
        { QStringLiteral("targetScreen"), QVariant::fromValue(screen) },
    };

    setGeometry(screen->geometry());
    connect(screen, &EffectScreen::geometryChanged, this, [this, screen]() {
        setGeometry(screen->geometry());
    });

    m_rootItem = qobject_cast<QQuickItem *>(component->createWithInitialProperties(initialProperties));
    Q_ASSERT(m_rootItem);
    m_rootItem->setParentItem(contentItem());

    auto updateSize = [this]() { m_rootItem->setSize(contentItem()->size()); };
    updateSize();
    connect(contentItem(), &QQuickItem::widthChanged, m_rootItem, updateSize);
    connect(contentItem(), &QQuickItem::heightChanged, m_rootItem, updateSize);
}

OverviewScreenView::~OverviewScreenView()
{
    delete m_rootItem;
    m_rootItem = nullptr;
}

bool OverviewScreenView::isDirty() const
{
    return m_dirty;
}

void OverviewScreenView::markDirty()
{
    m_dirty = true;
}

void OverviewScreenView::resetDirty()
{
    m_dirty = false;
}

void OverviewScreenView::scheduleRepaint()
{
    markDirty();
    effects->addRepaint(geometry());
}

void OverviewScreenView::stop()
{
    QMetaObject::invokeMethod(m_rootItem, "stop");
}

OverviewEffect::OverviewEffect()
    : m_shutdownTimer(new QTimer(this))
{
    qmlRegisterType<ExpoArea>("org.kde.kwin.private.overview", 1, 0, "ExpoArea");
    qmlRegisterType<ExpoLayout>("org.kde.kwin.private.overview", 1, 0, "ExpoLayout");
    qmlRegisterType<ExpoCell>("org.kde.kwin.private.overview", 1, 0, "ExpoCell");

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &OverviewEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::CTRL + Qt::META + Qt::Key_D;
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &OverviewEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Overview"));
    m_toggleAction->setText(i18n("Toggle Overview"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    effects->registerGlobalShortcut({defaultToggleShortcut}, m_toggleAction);

    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        if (m_activated) {
            realDeactivate();
        }
    });

    initConfig<OverviewConfig>();
    reconfigure(ReconfigureAll);
}

OverviewEffect::~OverviewEffect()
{
}

bool OverviewEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void OverviewEffect::reconfigure(ReconfigureFlags)
{
    OverviewConfig::self()->read();
    setLayout(ExpoLayout::LayoutMode(OverviewConfig::layoutMode()));
    setAnimationDuration(animationTime(200));
    setBlurBackground(OverviewConfig::blurBackground());

    for (const ElectricBorder &border : qAsConst(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    for (const ElectricBorder &border : qAsConst(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_toggleAction);
    }

    m_borderActivate.clear();
    m_touchBorderActivate.clear();

    const QList<int> activateBorders = OverviewConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    const QList<int> touchActivateBorders = OverviewConfig::touchBorderActivate();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivate.append(ElectricBorder(border));
        effects->registerTouchBorder(ElectricBorder(border), m_toggleAction);
    }
}

int OverviewEffect::animationDuration() const
{
    return m_animationDuration;
}

void OverviewEffect::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

ExpoLayout::LayoutMode OverviewEffect::layout() const
{
    return m_layout;
}

void OverviewEffect::setLayout(ExpoLayout::LayoutMode layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

bool OverviewEffect::blurBackground() const
{
    return m_blurBackground;
}

void OverviewEffect::setBlurBackground(bool blur)
{
    if (m_blurBackground != blur) {
        m_blurBackground = blur;
        Q_EMIT blurBackgroundChanged();
    }
}

void OverviewEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    Q_UNUSED(mask)
    Q_UNUSED(region)

    m_paintedScreen = data.screen();

    if (effects->waylandDisplay()) {
        OffscreenQuickView *screenView = m_screenViews.value(data.screen());
        if (screenView) {
            effects->renderOffscreenQuickView(screenView);
        }
    } else {
        for (OffscreenQuickView *screenView : qAsConst(m_screenViews)) {
            effects->renderOffscreenQuickView(screenView);
        }
    }
}

void OverviewEffect::postPaintScreen()
{
    // Screen views are repainted after kwin performs its compositing cycle. Another alternative
    // is to update the views after receiving a vblank.
    if (effects->waylandDisplay()) {
        OverviewScreenView *screenView = m_screenViews.value(m_paintedScreen);
        if (screenView && screenView->isDirty()) {
            QMetaObject::invokeMethod(screenView, &OffscreenQuickView::update, Qt::QueuedConnection);
            screenView->resetDirty();
        }
    } else {
        for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
            if (screenView->isDirty()) {
                QMetaObject::invokeMethod(screenView, &OffscreenQuickView::update, Qt::QueuedConnection);
                screenView->resetDirty();
            }
        }
    }
    effects->postPaintScreen();
}

bool OverviewEffect::isActive() const
{
    return !m_screenViews.isEmpty() && !effects->isScreenLocked();
}

int OverviewEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool OverviewEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        toggle();
        return true;
    }
    return false;
}

void OverviewEffect::toggle()
{
    if (!m_activated) {
        activate();
    } else {
        deactivate();
    }
}

void OverviewEffect::activate()
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!m_qmlEngine) {
        m_qmlEngine = new KDeclarative::QmlObjectSharedEngine(this);
    }

    if (!m_qmlComponent) {
        m_qmlComponent = new QQmlComponent(m_qmlEngine->engine(), this);
        m_qmlComponent->loadUrl(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/overview/qml/ScreenView.qml"))));
        if (m_qmlComponent->isError()) {
            qWarning() << "Failed to load the Overview effect:" << m_qmlComponent->errors();
            delete m_qmlComponent;
            m_qmlComponent = nullptr;
            return;
        }
    }

    effects->setActiveFullScreenEffect(this);
    m_activated = true;

    // Install an event filter to monitor cursor shape changes.
    qApp->installEventFilter(this);

    // This is an ugly hack to make hidpi rendering work as expected on wayland until we switch
    // to Qt 6.3 or newer. See https://codereview.qt-project.org/c/qt/qtdeclarative/+/361506
    if (effects->waylandDisplay()) {
        m_dummyWindow.reset(new QWindow());
        m_dummyWindow->setOpacity(0);
        m_dummyWindow->resize(1, 1);
        m_dummyWindow->setFlag(Qt::FramelessWindowHint);
        m_dummyWindow->setVisible(true);
        m_dummyWindow->requestActivate();
    }

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        createScreenView(screen);
    }
    effects->grabKeyboard(this);
    effects->startMouseInterception(this, Qt::ArrowCursor);
}

void OverviewEffect::deactivate()
{
    for (OverviewScreenView *screenView : std::as_const(m_screenViews)) {
        screenView->stop();
    }
    m_shutdownTimer->start(animationDuration());
}

void OverviewEffect::quickDeactivate()
{
    m_shutdownTimer->start(0);
}

void OverviewEffect::realDeactivate()
{
    qDeleteAll(m_screenViews);
    m_screenViews.clear();
    m_dummyWindow.reset();
    m_activated = false;
    qApp->removeEventFilter(this);
    effects->ungrabKeyboard();
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);
    effects->addRepaintFull();
}

void OverviewEffect::handleScreenAdded(EffectScreen *screen)
{
    if (isActive()) {
        createScreenView(screen);
    }
}

void OverviewEffect::handleScreenRemoved(EffectScreen *screen)
{
    delete m_screenViews.take(screen);
}

bool OverviewEffect::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::CursorChange) {
        if (const QWindow *window = qobject_cast<QWindow *>(watched)) {
            effects->defineCursor(window->cursor().shape());
        }
    }
    return false;
}

void OverviewEffect::createScreenView(EffectScreen *screen)
{
    auto screenView = new OverviewScreenView(m_qmlComponent, screen, m_dummyWindow.data(), this);
    screenView->setAutomaticRepaint(false);

    connect(screenView, &OffscreenQuickView::repaintNeeded, this, [screenView]() {
        effects->addRepaint(screenView->geometry());
    });
    connect(screenView, &OffscreenQuickView::renderRequested, screenView, &OverviewScreenView::scheduleRepaint);
    connect(screenView, &OffscreenQuickView::sceneChanged, screenView, &OverviewScreenView::scheduleRepaint);

    screenView->scheduleRepaint();
    m_screenViews.insert(screen, screenView);
}

OverviewScreenView *OverviewEffect::viewAt(const QPoint &pos) const
{
    for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
        if (screenView->geometry().contains(pos)) {
            return screenView;
        }
    }
    return nullptr;
}

void OverviewEffect::windowInputMouseEvent(QEvent *event)
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
        if (!m_mouseImplicitGrab) {
            m_mouseImplicitGrab = viewAt(globalPosition);
        }
    }

    OverviewScreenView *target = m_mouseImplicitGrab;
    if (!target) {
        target = viewAt(globalPosition);
    }

    if (!buttons) {
        m_mouseImplicitGrab = nullptr;
    }

    if (target) {
        target->forwardMouseEvent(event);
    }

}

void OverviewEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() + keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    OverviewScreenView *screenView = m_screenViews.value(effects->activeScreen());
    if (screenView) {
        screenView->contentItem()->setFocus(true);
        screenView->forwardKeyEvent(keyEvent);
    }
}

bool OverviewEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
        if (screenView->geometry().contains(pos.toPoint())) {
            return screenView->forwardTouchDown(id, pos, time);
        }
    }
    return false;
}

bool OverviewEffect::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
        if (screenView->geometry().contains(pos.toPoint())) {
            return screenView->forwardTouchMotion(id, pos, time);
        }
    }
    return false;
}

bool OverviewEffect::touchUp(qint32 id, quint32 time)
{
    for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
        if (screenView->forwardTouchUp(id, time)) {
            return true;
        }
    }
    return false;
}

} // namespace KWin
