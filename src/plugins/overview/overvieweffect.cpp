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

OverviewEffect::OverviewEffect()
    : m_state(new EffectTogglableState(this))
    , m_border(new EffectTogglableTouchBorder(m_state))
    , m_shutdownTimer(new QTimer(this))
{
    auto gesture = new EffectTogglableGesture(m_state);
    gesture->addTouchpadPinchGesture(PinchDirection::Contracting, 4);
    gesture->addTouchscreenSwipeGesture(SwipeDirection::Up, 3);

    connect(m_state, &EffectTogglableState::activated, this, &OverviewEffect::activate);
    connect(m_state, &EffectTogglableState::deactivated, this, &OverviewEffect::deactivate);
    connect(m_state, &EffectTogglableState::inProgressChanged, this, &OverviewEffect::gestureInProgressChanged);
    connect(m_state, &EffectTogglableState::partialActivationFactorChanged, this, &OverviewEffect::partialActivationFactorChanged);
    connect(m_state, &EffectTogglableState::statusChanged, this, [this](EffectTogglableState::Status status) {
        if (status == EffectTogglableState::Status::Activating) {
            m_searchText = QString();
        }
        setRunning(status != EffectTogglableState::Status::Inactive);
    });

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &OverviewEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_W;
    auto toggleAction = m_state->toggleAction();
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
    m_state->setStatus(EffectTogglableState::Status::Inactive);
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
