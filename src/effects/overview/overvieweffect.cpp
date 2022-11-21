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
    : m_shutdownTimer(new QTimer(this))
{
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &OverviewEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_W;
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &OverviewEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Overview"));
    m_toggleAction->setText(i18n("Toggle Overview"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);

    m_realtimeToggleAction = new QAction(this);
    connect(m_realtimeToggleAction, &QAction::triggered, this, [this]() {
        if (m_status == Status::Deactivating) {
            if (m_partialActivationFactor < 0.5) {
                deactivate();
            } else {
                cancelPartialDeactivate();
            }
        } else if (m_status == Status::Activating) {
            if (m_partialActivationFactor > 0.5) {
                activate();
            } else {
                cancelPartialActivate();
            }
        }
    });

    auto progressCallback = [this](qreal progress) {
        if (!effects->hasActiveFullScreenEffect() || effects->activeFullScreenEffect() == this) {
            switch (m_status) {
            case Status::Inactive:
            case Status::Activating:
                partialActivate(progress);
                break;
            case Status::Active:
            case Status::Deactivating:
                partialDeactivate(progress);
                break;
            }
        }
    };

    effects->registerRealtimeTouchpadPinchShortcut(PinchDirection::Contracting, 4, m_realtimeToggleAction, progressCallback);
    effects->registerTouchscreenSwipeShortcut(SwipeDirection::Up, 3, m_realtimeToggleAction, progressCallback);

    connect(effects, &EffectsHandler::screenAboutToLock, this, &OverviewEffect::realDeactivate);

    initConfig<OverviewConfig>();
    reconfigure(ReconfigureAll);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/overview/qml/ScreenView.qml"))));
}

OverviewEffect::~OverviewEffect()
{
}

QVariantMap OverviewEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
    };
}

void OverviewEffect::reconfigure(ReconfigureFlags)
{
    OverviewConfig::self()->read();
    setLayout(OverviewConfig::layoutMode());
    setAnimationDuration(animationTime(300));
    setBlurBackground(OverviewConfig::blurBackground());

    for (const ElectricBorder &border : std::as_const(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    for (const ElectricBorder &border : std::as_const(m_touchBorderActivate)) {
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
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, [this](ElectricBorder border, const QPointF &deltaProgress, const EffectScreen *screen) {
            if (m_status == Status::Active) {
                return;
            }
            const int maxDelta = 500; // Arbitrary logical pixels value seems to behave better than scaledScreenSize
            if (border == ElectricTop || border == ElectricBottom) {
                partialActivate(std::min(1.0, std::abs(deltaProgress.y()) / maxDelta));
            } else {
                partialActivate(std::min(1.0, std::abs(deltaProgress.x()) / maxDelta));
            }
        });
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

qreal OverviewEffect::partialActivationFactor() const
{
    return m_partialActivationFactor;
}

void OverviewEffect::setPartialActivationFactor(qreal factor)
{
    if (m_partialActivationFactor != factor) {
        m_partialActivationFactor = factor;
        Q_EMIT partialActivationFactorChanged();
    }
}

bool OverviewEffect::gestureInProgress() const
{
    return m_gestureInProgress;
}

void OverviewEffect::setGestureInProgress(bool gesture)
{
    if (m_gestureInProgress != gesture) {
        m_gestureInProgress = gesture;
        Q_EMIT gestureInProgressChanged();
    }
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
    if (!isRunning() || m_partialActivationFactor > 0.5) {
        activate();
    } else {
        deactivate();
    }
}

void OverviewEffect::activate()
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_status = Status::Active;

    setGestureInProgress(false);
    setPartialActivationFactor(0.0);

    // This one should be the last.
    m_searchText = QString();
    setRunning(true);
}

void OverviewEffect::partialActivate(qreal factor)
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_status = Status::Activating;

    setPartialActivationFactor(factor);
    setGestureInProgress(true);

    // This one should be the last.
    m_searchText = QString();
    setRunning(true);
}

void OverviewEffect::cancelPartialActivate()
{
    deactivate();
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

    setGestureInProgress(false);
    setPartialActivationFactor(0.0);
}

void OverviewEffect::partialDeactivate(qreal factor)
{
    m_status = Status::Deactivating;

    setPartialActivationFactor(1.0 - factor);
    setGestureInProgress(true);
}

void OverviewEffect::cancelPartialDeactivate()
{
    activate();
}

void OverviewEffect::realDeactivate()
{
    setRunning(false);
    m_status = Status::Inactive;
}

void OverviewEffect::quickDeactivate()
{
    m_shutdownTimer->start(0);
}

void OverviewEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() | keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

} // namespace KWin
