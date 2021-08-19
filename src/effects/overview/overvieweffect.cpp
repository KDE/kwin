/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"
#include "expoarea.h"
#include "expolayout.h"
#include "overviewconfig.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QQmlContext>
#include <QQuickItem>
#include <QTimer>
#include <QWindow>

namespace KWin
{

OverviewScreenView::OverviewScreenView(EffectScreen *screen, QWindow *renderWindow, OverviewEffect *effect)
    : EffectQuickScene(effect, renderWindow)
{
    rootContext()->setContextProperty("effect", effect);
    rootContext()->setContextProperty("targetScreen", screen);

    setGeometry(screen->geometry());
    setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/overview/qml/ScreenView.qml"))));

    connect(screen, &EffectScreen::geometryChanged, this, [this, screen]() {
        setGeometry(screen->geometry());
    });
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
    QMetaObject::invokeMethod(rootItem(), "stop");
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

    connect(effects, &EffectsHandler::screenAboutToLock, this, &OverviewEffect::realDeactivate);

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
    setAnimationDuration(animationTime(300));

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

void OverviewEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    Q_UNUSED(mask)
    Q_UNUSED(region)

    m_paintedScreen = data.screen();

    if (effects->waylandDisplay()) {
        EffectQuickView *screenView = m_screenViews.value(data.screen());
        if (screenView) {
            effects->renderEffectQuickView(screenView);
        }
    } else {
        for (EffectQuickView *screenView : qAsConst(m_screenViews)) {
            effects->renderEffectQuickView(screenView);
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
            QMetaObject::invokeMethod(screenView, &EffectQuickView::update, Qt::QueuedConnection);
            screenView->resetDirty();
        }
    } else {
        for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
            if (screenView->isDirty()) {
                QMetaObject::invokeMethod(screenView, &EffectQuickView::update, Qt::QueuedConnection);
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

    effects->setActiveFullScreenEffect(this);
    m_activated = true;

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
    for (OverviewScreenView *screenView : m_screenViews) {
        screenView->stop();
    }
    m_shutdownTimer->start(animationDuration());
}

void OverviewEffect::realDeactivate()
{
    qDeleteAll(m_screenViews);
    m_screenViews.clear();
    m_dummyWindow.reset();
    m_activated = false;
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

void OverviewEffect::createScreenView(EffectScreen *screen)
{
    auto screenView = new OverviewScreenView(screen, m_dummyWindow.data(), this);
    screenView->setAutomaticRepaint(false);

    connect(screenView, &EffectQuickView::repaintNeeded, this, [screenView]() {
        effects->addRepaint(screenView->geometry());
    });
    connect(screenView, &EffectQuickView::renderRequested, screenView, &OverviewScreenView::scheduleRepaint);
    connect(screenView, &EffectQuickView::sceneChanged, screenView, &OverviewScreenView::scheduleRepaint);

    screenView->scheduleRepaint();
    m_screenViews.insert(screen, screenView);
}

void OverviewEffect::windowInputMouseEvent(QEvent *event)
{
    QPoint globalPosition;
    if (QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event)) {
        globalPosition = mouseEvent->globalPos();
    } else if (QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent *>(event)) {
        globalPosition = wheelEvent->globalPosition().toPoint();
    } else {
        return;
    }
    for (OverviewScreenView *screenView : qAsConst(m_screenViews)) {
        if (screenView->geometry().contains(globalPosition)) {
            screenView->forwardMouseEvent(event);
            break;
        }
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
    EffectScreen *activeScreen = effects->findScreen(effects->activeScreen());
    OverviewScreenView *screenView = m_screenViews.value(activeScreen);
    if (screenView) {
        screenView->contentItem()->setFocus(true);
        screenView->forwardKeyEvent(keyEvent);
    }
}

} // namespace KWin
