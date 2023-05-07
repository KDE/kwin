/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"
#include "overviewconfig.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QQuickItem>
#include <QTimer>

namespace KWin
{

TogglableState::TogglableState(QObject *effect)
    : QObject(effect)
{
    m_activateAction = new QAction(this);
    connect(m_activateAction, &QAction::triggered, this, [this]() {
        if (m_status == Status::Activating) {
            if (m_partialActivationFactor > 0.5) {
                activate();
                Q_EMIT activated();
            } else {
                deactivate();
                Q_EMIT deactivated();
            }
        }
    });
    m_deactivateAction = new QAction(this);
    connect(m_deactivateAction, &QAction::triggered, this, [this]() {
        if (m_status == Status::Deactivating) {
            if (m_partialActivationFactor < 0.5) {
                deactivate();
                Q_EMIT deactivated();
            } else {
                activate();
                Q_EMIT activated();
            }
        }
    });
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &TogglableState::toggle);
}

void TogglableState::activate()
{
    setStatus(Status::Active);
    setInProgress(false);
    setPartialActivationFactor(0.0);
}

void TogglableState::setPartialActivationFactor(qreal factor)
{
    if (m_partialActivationFactor != factor) {
        m_partialActivationFactor = factor;
        Q_EMIT partialActivationFactorChanged();
    }
}

void TogglableState::deactivate()
{
    setInProgress(false);
    setPartialActivationFactor(0.0);
}

bool TogglableState::inProgress() const
{
    return m_inProgress;
}

void TogglableState::setInProgress(bool gesture)
{
    if (m_inProgress != gesture) {
        m_inProgress = gesture;
        Q_EMIT inProgressChanged();
    }
}

void TogglableState::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        Q_EMIT statusChanged(status);
    }
}

void TogglableState::partialActivate(qreal factor)
{
    if (effects->isScreenLocked()) {
        return;
    }

    setStatus(Status::Activating);
    setPartialActivationFactor(factor);
    setInProgress(true);
}

void TogglableState::partialDeactivate(qreal factor)
{
    setStatus(Status::Deactivating);
    setPartialActivationFactor(1.0 - factor);
    setInProgress(true);
}

void TogglableState::toggle()
{
    if (m_status == Status::Inactive || m_partialActivationFactor > 0.5) {
        activate();
        Q_EMIT activated();
    } else {
        deactivate();
        Q_EMIT deactivated();
    }
}

void TogglableState::setProgress(qreal progress)
{
    if (!effects->hasActiveFullScreenEffect() || effects->activeFullScreenEffect() == parent()) {
        switch (m_status) {
        case Status::Inactive:
        case Status::Activating:
            partialActivate(progress);
            break;
        }
    }
}

void TogglableState::setRegress(qreal regress)
{
    if (!effects->hasActiveFullScreenEffect() || effects->activeFullScreenEffect() == parent()) {
        switch (m_status) {
        case Status::Active:
        case Status::Deactivating:
            partialDeactivate(regress);
            break;
        }
    }
}

TogglableGesture::TogglableGesture(TogglableState *state)
    : QObject(state)
    , m_state(state)
{
}

static PinchDirection opposite(PinchDirection direction)
{
    switch (direction) {
    case PinchDirection::Contracting:
        return PinchDirection::Expanding;
    case PinchDirection::Expanding:
        return PinchDirection::Contracting;
    }
}

void TogglableGesture::addTouchpadGesture(PinchDirection direction, uint fingerCount)
{
    auto progressCallback = [this](qreal progress) {
        m_state->setProgress(progress);
    };
    auto progressCallbackInv = [this](qreal progress) {
        m_state->setRegress(progress);
    };

    effects->registerTouchpadPinchShortcut(direction, 4, m_state->activateAction(), progressCallback);
    effects->registerTouchpadPinchShortcut(opposite(direction), 4, m_state->deactivateAction(), progressCallbackInv);
}

static SwipeDirection opposite(SwipeDirection direction)
{
    switch (direction) {
    case SwipeDirection::Invalid:
        return SwipeDirection::Invalid;
    case SwipeDirection::Down:
        return SwipeDirection::Up;
    case SwipeDirection::Up:
        return SwipeDirection::Down;
    case SwipeDirection::Left:
        return SwipeDirection::Right;
    case SwipeDirection::Right:
        return SwipeDirection::Left;
    }
}

void TogglableGesture::addTouchscreenSwipeGesture(SwipeDirection direction, uint fingerCount)
{
    auto progressCallback = [this](qreal progress) {
        m_state->setProgress(progress);
    };
    auto progressCallbackInv = [this](qreal progress) {
        m_state->setRegress(progress);
    };

    effects->registerTouchscreenSwipeShortcut(direction, 3, m_state->activateAction(), progressCallback);
    effects->registerTouchscreenSwipeShortcut(opposite(direction), 3, m_state->deactivateAction(), progressCallbackInv);
}

TogglableTouchBorder::TogglableTouchBorder(TogglableState *state)
    : QObject(state)
    , m_state(state)
{
}

TogglableTouchBorder::~TogglableTouchBorder()
{
    for (const ElectricBorder &border : std::as_const(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_state->toggleAction());
    }
}

void TogglableTouchBorder::setBorders(const QList<int> &touchActivateBorders)
{
    for (const ElectricBorder &border : std::as_const(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_state->toggleAction());
    }
    m_touchBorderActivate.clear();

    for (const int &border : touchActivateBorders) {
        m_touchBorderActivate.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_state->toggleAction(), [this](ElectricBorder border, const QPointF &deltaProgress, const EffectScreen *screen) {
            if (m_state->status() == TogglableState::Status::Active) {
                return;
            }
            const int maxDelta = 500; // Arbitrary logical pixels value seems to behave better than scaledScreenSize
            qreal progress = 0;
            if (border == ElectricTop || border == ElectricBottom) {
                progress = std::min(1.0, std::abs(deltaProgress.y()) / maxDelta);
            } else {
                progress = std::min(1.0, std::abs(deltaProgress.x()) / maxDelta);
            }
            m_state->setProgress(progress);
        });
    }
}

OverviewEffect::OverviewEffect()
    : m_state(new TogglableState(this))
    , m_border(new TogglableTouchBorder(m_state))
    , m_shutdownTimer(new QTimer(this))
{
    auto gesture = new TogglableGesture(m_state);
    gesture->addTouchpadGesture(PinchDirection::Contracting, 4);
    gesture->addTouchscreenSwipeGesture(SwipeDirection::Up, 3);

    connect(m_state, &TogglableState::activated, this, &OverviewEffect::activate);
    connect(m_state, &TogglableState::deactivated, this, &OverviewEffect::deactivate);
    connect(m_state, &TogglableState::inProgressChanged, this, &OverviewEffect::gestureInProgressChanged);
    connect(m_state, &TogglableState::partialActivationFactorChanged, this, &OverviewEffect::partialActivationFactorChanged);
    connect(m_state, &TogglableState::statusChanged, this, [this](TogglableState::Status status) {
        if (status == TogglableState::Status::Activating) {
            m_searchText = QString();
        }
        setRunning(status != TogglableState::Status::Inactive);
    });

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &OverviewEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_W;
    auto toggleAction = m_state->toggleAction();
    connect(toggleAction, &QAction::triggered, m_state, &TogglableState::toggle);
    toggleAction->setObjectName(QStringLiteral("Overview"));
    toggleAction->setText(i18n("Toggle Overview"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(toggleAction);

    connect(effects, &EffectsHandler::screenAboutToLock, this, &OverviewEffect::realDeactivate);

    initConfig<OverviewConfig>();
    reconfigure(ReconfigureAll);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/overview/qml/main.qml"))));
}

OverviewEffect::~OverviewEffect()
{
}

void OverviewEffect::reconfigure(ReconfigureFlags)
{
    OverviewConfig::self()->read();
    setLayout(OverviewConfig::layoutMode());
    setAnimationDuration(animationTime(300));

    for (const ElectricBorder &border : std::as_const(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    m_borderActivate.clear();

    const QList<int> activateBorders = OverviewConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    m_border->setBorders(OverviewConfig::touchBorderActivate());
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

int OverviewEffect::layout() const
{
    return m_layout;
}

bool OverviewEffect::ignoreMinimized() const
{
    return OverviewConfig::ignoreMinimized();
}

void OverviewEffect::setLayout(int layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

int OverviewEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool OverviewEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        m_state->toggle();
        return true;
    }
    return false;
}

void OverviewEffect::activate()
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_state->activate();
}

void OverviewEffect::deactivate()
{
    const auto screens = effects->screens();
    for (const auto screen : screens) {
        if (QuickSceneView *view = viewForScreen(screen)) {
            QMetaObject::invokeMethod(view->rootItem(), "stop");
        }
    }
    m_shutdownTimer->start(animationDuration());

    m_state->deactivate();
}

void OverviewEffect::realDeactivate()
{
    m_state->setStatus(TogglableState::Status::Inactive);
}

void OverviewEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() | keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            m_state->toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

} // namespace KWin
