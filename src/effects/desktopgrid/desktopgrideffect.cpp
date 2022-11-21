/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopgrideffect.h"
#include "desktopgridconfig.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>
#include <cmath>

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

DesktopGridEffect::DesktopGridEffect()
    : m_shutdownTimer(new QTimer(this))
    , m_toggleAction(new QAction(this))
    , m_realtimeToggleAction(new QAction(this))
{
    qmlRegisterUncreatableType<DesktopGridEffect>("org.kde.kwin.private.desktopgrid", 1, 0, "DesktopGridEffect", QStringLiteral("Cannot create elements of type DesktopGridEffect"));

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &DesktopGridEffect::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &DesktopGridEffect::realDeactivate);
    connect(effects, &EffectsHandler::desktopGridWidthChanged, this, &DesktopGridEffect::gridColumnsChanged);
    connect(effects, &EffectsHandler::desktopGridHeightChanged, this, &DesktopGridEffect::gridRowsChanged);

    m_toggleAction->setObjectName(QStringLiteral("ShowDesktopGrid"));
    m_toggleAction->setText(i18n("Show Desktop Grid"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, QList<QKeySequence>() << (Qt::META | Qt::Key_F8));
    KGlobalAccel::self()->setShortcut(m_toggleAction, QList<QKeySequence>() << (Qt::META | Qt::Key_F8));
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    connect(m_toggleAction, &QAction::triggered, this, [this]() {
        if (isRunning()) {
            deactivate(animationDuration());
        } else {
            activate();
        }
    });

    connect(m_realtimeToggleAction, &QAction::triggered, this, [this]() {
        if (m_status == Status::Deactivating) {
            if (m_partialActivationFactor < 0.5) {
                deactivate(animationDuration());
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

    effects->registerRealtimeTouchpadSwipeShortcut(SwipeDirection::Up, 4, m_realtimeToggleAction, [this](qreal progress) {
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
    });

    initConfig<DesktopGridConfig>();
    reconfigure(ReconfigureAll);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/qml/main.qml"))));
}

DesktopGridEffect::~DesktopGridEffect()
{
}

QVariantMap DesktopGridEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
    };
}

void DesktopGridEffect::reconfigure(ReconfigureFlags)
{
    DesktopGridConfig::self()->read();
    setLayout(DesktopGridConfig::layoutMode());
    setAnimationDuration(animationTime(300));

    for (const ElectricBorder &border : std::as_const(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    for (const ElectricBorder &border : std::as_const(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_toggleAction);
    }

    m_borderActivate.clear();
    m_touchBorderActivate.clear();

    const QList<int> activateBorders = DesktopGridConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    const QList<int> touchActivateBorders = DesktopGridConfig::touchBorderActivate();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivate.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, [this](ElectricBorder border, const QPointF &deltaProgress, const EffectScreen *screen) {
            if (m_status == Status::Active) {
                return;
            }
            const int rows = gridRows();
            const int columns = gridColumns();
            if (border == ElectricTop || border == ElectricBottom) {
                const int maxDelta = (screen->geometry().height() / rows) * (rows - (effects->currentDesktop() % rows));
                partialActivate(std::min(1.0, std::abs(deltaProgress.y()) / maxDelta));
            } else {
                const int maxDelta = (screen->geometry().width() / columns) * (columns - (effects->currentDesktop() % columns));
                partialActivate(std::min(1.0, std::abs(deltaProgress.x()) / maxDelta));
            }
        });
    }

    Q_EMIT showAddRemoveChanged();
    Q_EMIT desktopNameAlignmentChanged();
    Q_EMIT desktopLayoutModeChanged();
    Q_EMIT customLayoutRowsChanged();
}

int DesktopGridEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool DesktopGridEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        toggle();
        return true;
    }
    return false;
}

void DesktopGridEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() | keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

Qt::AlignmentFlag DesktopGridEffect::desktopNameAlignment() const
{
    return Qt::AlignmentFlag(DesktopGridConfig::desktopNameAlignment());
}

DesktopGridEffect::DesktopLayoutMode DesktopGridEffect::desktopLayoutMode() const
{
    return DesktopGridEffect::DesktopLayoutMode(DesktopGridConfig::desktopLayoutMode());
}

int DesktopGridEffect::customLayoutRows() const
{
    return DesktopGridConfig::customLayoutRows();
}

void DesktopGridEffect::addDesktop() const
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() + 1);
}

void DesktopGridEffect::removeDesktop() const
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() - 1);
}

void DesktopGridEffect::swapDesktops(int from, int to)
{
    QList<EffectWindow *> fromList;
    QList<EffectWindow *> toList;
    for (auto *w : effects->stackingOrder()) {
        if (!w->isNormalWindow() || !w->isOnCurrentActivity() ) {
            continue;
        }
        if (w->isOnDesktop(from)) {
            fromList << w;
        } else if (w->isOnDesktop(to)) {
            toList << w;
        }
    }
    for (auto *w : fromList) {
        effects->windowToDesktop(w, to);
    }
    for (auto *w : toList) {
        effects->windowToDesktop(w, from);
    }
}

int DesktopGridEffect::gridRows() const
{
    switch (desktopLayoutMode()) {
    case DesktopLayoutMode::LayoutAutomatic:
        return ceil(sqrt(effects->numberOfDesktops()));
    case DesktopLayoutMode::LayoutCustom:
        return std::clamp(customLayoutRows(), 1, effects->numberOfDesktops());
    case DesktopLayoutMode::LayoutPager:
    default:
        return effects->desktopGridSize().height();
    }
}

int DesktopGridEffect::gridColumns() const
{
    switch (desktopLayoutMode()) {
    case DesktopLayoutMode::LayoutAutomatic:
        return ceil(sqrt(effects->numberOfDesktops()));
    case DesktopLayoutMode::LayoutCustom:
        return std::max(1.0, ceil(qreal(effects->numberOfDesktops()) / customLayoutRows()));
    case DesktopLayoutMode::LayoutPager:
    default:
        return effects->desktopGridSize().width();
    }
}

int DesktopGridEffect::animationDuration() const
{
    return m_animationDuration;
}

void DesktopGridEffect::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

bool DesktopGridEffect::showAddRemove() const
{
    return DesktopGridConfig::showAddRemove();
}

int DesktopGridEffect::layout() const
{
    return m_layout;
}

void DesktopGridEffect::setLayout(int layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

qreal DesktopGridEffect::partialActivationFactor() const
{
    return m_partialActivationFactor;
}

void DesktopGridEffect::setPartialActivationFactor(qreal factor)
{
    if (m_partialActivationFactor != factor) {
        m_partialActivationFactor = factor;
        Q_EMIT partialActivationFactorChanged();
    }
}

bool DesktopGridEffect::gestureInProgress() const
{
    return m_gestureInProgress;
}

void DesktopGridEffect::setGestureInProgress(bool gesture)
{
    if (m_gestureInProgress != gesture) {
        m_gestureInProgress = gesture;
        Q_EMIT gestureInProgressChanged();
    }
}

void DesktopGridEffect::toggle()
{
    if (!isRunning() || m_partialActivationFactor > 0.5) {
        activate();
    } else {
        deactivate(animationDuration());
    }
}

void DesktopGridEffect::activate()
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_status = Status::Active;

    setGestureInProgress(false);
    setPartialActivationFactor(0.0);

    // This one should be the last.
    setRunning(true);
}

void DesktopGridEffect::partialActivate(qreal factor)
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_status = Status::Activating;

    setPartialActivationFactor(factor);
    setGestureInProgress(true);

    // This one should be the last.
    setRunning(true);
}

void DesktopGridEffect::cancelPartialActivate()
{
    deactivate(animationDuration());
}

void DesktopGridEffect::deactivate(int timeout)
{
    const auto screens = effects->screens();
    for (const auto screen : screens) {
        if (QuickSceneView *view = viewForScreen(screen)) {
            QMetaObject::invokeMethod(view->rootItem(), "stop");
        }
    }
    m_shutdownTimer->start(timeout);

    setGestureInProgress(false);
    setPartialActivationFactor(0.0);
}

void DesktopGridEffect::partialDeactivate(qreal factor)
{
    m_status = Status::Deactivating;

    setPartialActivationFactor(1 - factor);
    setGestureInProgress(true);
}

void DesktopGridEffect::cancelPartialDeactivate()
{
    activate();
}

void DesktopGridEffect::realDeactivate()
{
    setRunning(false);
    m_status = Status::Inactive;
}

} // namespace KWin
