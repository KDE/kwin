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

    m_realtimeToggleAction = new QAction(this);
    connect(m_realtimeToggleAction, &QAction::triggered, this, [this]() {
        if (isRunning() && m_partialActivationFactor > 0.5) {
            activate();
        } else {
            deactivate();
        }
        m_partialActivationFactor = 0;
        Q_EMIT gestureInProgressChanged();
        Q_EMIT partialActivationFactorChanged();
    });

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_W;
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &OverviewEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Overview"));
    m_toggleAction->setText(i18n("Toggle Overview"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    effects->registerGlobalShortcut({defaultToggleShortcut}, m_toggleAction);

    auto progressCallback = [this](qreal progress) {
        if (m_status == Status::Active) {
            return;
        }
        const bool wasInProgress = m_partialActivationFactor > 0;
        m_partialActivationFactor = progress;
        Q_EMIT partialActivationFactorChanged();
        if (!wasInProgress) {
            Q_EMIT gestureInProgressChanged();
        }
        if (!isRunning()) {
            partialActivate();
        }
    };

    effects->registerRealtimeTouchpadSwipeShortcut(SwipeDirection::Up, 4, m_realtimeToggleAction, progressCallback);
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
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, [this](ElectricBorder border, const QSizeF &deltaProgress, const EffectScreen *screen) {
            Q_UNUSED(screen)
            if (m_status == Status::Active) {
                return;
            }
            const bool wasInProgress = m_partialActivationFactor > 0;
            const int maxDelta = 500; // Arbitrary logical pixels value seems to behave better than scaledScreenSize
            if (border == ElectricTop || border == ElectricBottom) {
                m_partialActivationFactor = std::min(1.0, qAbs(deltaProgress.height()) / maxDelta);
            } else {
                m_partialActivationFactor = std::min(1.0, qAbs(deltaProgress.width()) / maxDelta);
            }
            Q_EMIT partialActivationFactorChanged();
            if (!wasInProgress) {
                Q_EMIT gestureInProgressChanged();
            }
            if (!isRunning()) {
                partialActivate();
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

bool OverviewEffect::gestureInProgress() const
{
    return m_partialActivationFactor > 0;
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
    m_partialActivationFactor = 0;
    Q_EMIT gestureInProgressChanged();
    Q_EMIT partialActivationFactorChanged();
}

void OverviewEffect::activate()
{
    if (effects->isScreenLocked()) {
        return;
    }
    m_status = Status::Active;
    setRunning(true);
}

void OverviewEffect::partialActivate()
{
    if (effects->isScreenLocked()) {
        return;
    }
    m_status = Status::Activating;
    setRunning(true);
}

void OverviewEffect::deactivate()
{
    const auto screenViews = views();
    for (QuickSceneView *view : screenViews) {
        QMetaObject::invokeMethod(view->rootItem(), "stop");
    }
    m_shutdownTimer->start(animationDuration());
    m_status = Status::Inactive;
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
