/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"
#include "configurablegesture.h"
#include "effect/effecthandler.h"
#include "overviewconfig.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QQuickItem>
#include <QTimer>

using namespace std::chrono_literals;

namespace KWin
{

OverviewEffect::OverviewEffect()
    // manages the transition between inactive -> overview
    : m_overviewState(new EffectTogglableState(this))
    // manages the transition between overview -> grid
    , m_transitionState(new EffectTogglableState(this))
    // manages the transition betwee inactive -> overview
    , m_gridState(new EffectTogglableState(this))
    , m_border(new EffectTogglableTouchBorder(m_overviewState))
    , m_gridBorder(new EffectTogglableTouchBorder(m_gridState))
    , m_gesture(effects->registerGesture("builtin_overview", "overview effect"))
    , m_shutdownTimer(new QTimer(this))
{
    m_overviewState->addGesture(m_gesture.get());
    m_transitionState->addGesture(m_gesture.get());
    m_gridState->addInverseGesture(m_gesture.get());
    m_transitionState->stop();

    connect(m_overviewState, &EffectTogglableState::inProgressChanged, this, &OverviewEffect::overviewGestureInProgressChanged);
    connect(m_overviewState, &EffectTogglableState::partialActivationFactorChanged, this, &OverviewEffect::overviewPartialActivationFactorChanged);

    connect(m_overviewState, &EffectTogglableState::statusChanged, this, [this](EffectTogglableState::Status status) {
        if (status == EffectTogglableState::Status::Activating || status == EffectTogglableState::Status::Active) {
            m_searchText = QString();
            setRunning(true);
            m_gridState->stop();
        }
        if (status == EffectTogglableState::Status::Active) {
            m_transitionState->deactivate();
        }
        if (status == EffectTogglableState::Status::Inactive || status == EffectTogglableState::Status::Deactivating) {
            m_transitionState->stop();
        }
        if (status == EffectTogglableState::Status::Inactive) {
            m_gridState->deactivate();
            deactivate();
        }
    });

    connect(m_transitionState, &EffectTogglableState::statusChanged, this, [this](EffectTogglableState::Status status) {
        if (status == EffectTogglableState::Status::Activating || status == EffectTogglableState::Status::Active) {
            m_overviewState->stop();
        }
        if (status == EffectTogglableState::Status::Inactive) {
            m_overviewState->activate();
        }
        if (status == EffectTogglableState::Status::Active) {
            m_gridState->activate();
        }
        if (status == EffectTogglableState::Status::Inactive || status == EffectTogglableState::Status::Deactivating) {
            m_gridState->stop();
        }
    });

    connect(m_gridState, &EffectTogglableState::statusChanged, this, [this](EffectTogglableState::Status status) {
        if (status == EffectTogglableState::Status::Activating || status == EffectTogglableState::Status::Active) {
            m_searchText = QString();
            setRunning(true);
            m_overviewState->stop();
        }
        if (status == EffectTogglableState::Status::Inactive) {
            m_overviewState->deactivate();
            deactivate();
        }
        if (status == EffectTogglableState::Status::Active) {
            m_transitionState->activate();
        }
        if (status == EffectTogglableState::Status::Inactive || status == EffectTogglableState::Status::Deactivating) {
            m_transitionState->stop();
        }
    });

    connect(m_transitionState, &EffectTogglableState::inProgressChanged, this, &OverviewEffect::transitionGestureInProgressChanged);
    connect(m_transitionState, &EffectTogglableState::partialActivationFactorChanged, this, &OverviewEffect::transitionPartialActivationFactorChanged);

    connect(m_gridState, &EffectTogglableState::inProgressChanged, this, &OverviewEffect::gridGestureInProgressChanged);
    connect(m_gridState, &EffectTogglableState::partialActivationFactorChanged, this, &OverviewEffect::gridPartialActivationFactorChanged);

    connect(effects, &EffectsHandler::desktopChanging, this, [this](VirtualDesktop *old, QPointF desktopOffset, EffectWindow *with) {
        m_desktopOffset = desktopOffset;
        Q_EMIT desktopOffsetChanged();
    });
    connect(effects, &EffectsHandler::desktopChanged, this, [this](VirtualDesktop *old, VirtualDesktop *current, EffectWindow *with) {
        m_desktopOffset = QPointF(0, 0);
        Q_EMIT desktopOffsetChanged();
    });
    connect(effects, &EffectsHandler::desktopChangingCancelled, this, [this]() {
        m_desktopOffset = QPointF(0, 0);
        Q_EMIT desktopOffsetChanged();
    });

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &OverviewEffect::realDeactivate);

    auto cycleAction = new QAction(this);
    connect(cycleAction, &QAction::triggered, this, &OverviewEffect::cycle);
    cycleAction->setObjectName(QStringLiteral("Cycle Overview"));
    cycleAction->setText(i18nc("@action Grid View and Overview are the name of KWin effects", "Cycle through Overview and Grid View"));
    KGlobalAccel::self()->setDefaultShortcut(cycleAction, {});
    KGlobalAccel::self()->setShortcut(cycleAction, {});
    m_cycleShortcut = KGlobalAccel::self()->shortcut(cycleAction);

    auto reverseCycleAction = new QAction(this);
    connect(reverseCycleAction, &QAction::triggered, this, &OverviewEffect::reverseCycle);
    reverseCycleAction->setObjectName(QStringLiteral("Cycle Overview Opposite"));
    reverseCycleAction->setText(i18nc("@action Grid View and Overview are the name of KWin effects", "Cycle through Grid View and Overview"));
    KGlobalAccel::self()->setDefaultShortcut(reverseCycleAction, {});
    KGlobalAccel::self()->setShortcut(reverseCycleAction, {});
    m_reverseCycleShortcut = KGlobalAccel::self()->shortcut(reverseCycleAction);

    const QKeySequence defaultOverviewShortcut = Qt::META | Qt::Key_W;
    auto overviewAction = m_overviewState->toggleAction();
    overviewAction->setObjectName(QStringLiteral("Overview"));
    overviewAction->setText(i18nc("@action Overview is the name of a Kwin effect", "Toggle Overview"));
    overviewAction->setAutoRepeat(false);
    KGlobalAccel::self()->setDefaultShortcut(overviewAction, {defaultOverviewShortcut});
    KGlobalAccel::self()->setShortcut(overviewAction, {defaultOverviewShortcut});
    m_overviewShortcut = KGlobalAccel::self()->shortcut(overviewAction);

    const QKeySequence defaultGridShortcut = Qt::META | Qt::Key_G;
    auto gridAction = m_gridState->toggleAction();
    gridAction->setObjectName(QStringLiteral("Grid View"));
    gridAction->setText(i18nc("@action Grid view is the name of a Kwin effect", "Toggle Grid View"));
    gridAction->setAutoRepeat(false);
    KGlobalAccel::self()->setDefaultShortcut(gridAction, {defaultGridShortcut});
    KGlobalAccel::self()->setShortcut(gridAction, {defaultGridShortcut});
    m_gridShortcut = KGlobalAccel::self()->shortcut(gridAction);

    connect(effects, &EffectsHandler::screenAboutToLock, this, &OverviewEffect::realDeactivate);

    OverviewConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    auto delegate = new QQmlComponent(effects->qmlEngine(), this);
    connect(delegate, &QQmlComponent::statusChanged, this, [delegate]() {
        if (delegate->isError()) {
            qWarning() << "Failed to load overview:" << delegate->errorString();
        }
    });
    delegate->loadFromModule(QStringLiteral("org.kde.kwin.overview"), QStringLiteral("Main"), QQmlComponent::Asynchronous);
    setDelegate(delegate);
}

OverviewEffect::~OverviewEffect()
{
}

void OverviewEffect::reconfigure(ReconfigureFlags)
{
    OverviewConfig::self()->read();
    setAnimationDuration(animationTime(300ms));
    setFilterWindows(OverviewConfig::filterWindows());

    for (const ElectricBorder &border : std::as_const(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }
    for (const ElectricBorder &border : std::as_const(m_gridBorderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    m_borderActivate.clear();
    m_gridBorderActivate.clear();

    const QList<int> activateBorders = OverviewConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    const QList<int> gridActivateBorders = OverviewConfig::gridBorderActivate();
    for (const int &border : gridActivateBorders) {
        m_gridBorderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    m_border->setBorders(OverviewConfig::touchBorderActivate());
    m_gridBorder->setBorders(OverviewConfig::gridTouchBorderActivate());
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

bool OverviewEffect::filterWindows() const
{
    return m_filterWindows;
}

void OverviewEffect::setFilterWindows(bool filterWindows)
{
    if (m_filterWindows != filterWindows) {
        m_filterWindows = filterWindows;
        Q_EMIT filterWindowsChanged();
    }
}

qreal OverviewEffect::overviewPartialActivationFactor() const
{
    return m_overviewState->partialActivationFactor();
}

bool OverviewEffect::overviewGestureInProgress() const
{
    return m_overviewState->inProgress();
}

qreal OverviewEffect::transitionPartialActivationFactor() const
{
    return m_transitionState->partialActivationFactor();
}

bool OverviewEffect::transitionGestureInProgress() const
{
    return m_transitionState->inProgress();
}

qreal OverviewEffect::gridPartialActivationFactor() const
{
    return m_gridState->partialActivationFactor();
}

bool OverviewEffect::gridGestureInProgress() const
{
    return m_gridState->inProgress();
}

QPointF OverviewEffect::desktopOffset() const
{
    return m_desktopOffset;
}

bool OverviewEffect::ignoreMinimized() const
{
    return OverviewConfig::ignoreMinimized();
}

bool OverviewEffect::organizedGrid() const
{
    return OverviewConfig::organizedGrid();
}

int OverviewEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool OverviewEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        m_overviewState->toggle();
        return true;
    } else if (m_gridBorderActivate.contains(border)) {
        m_gridState->toggle();
        return true;
    }
    return false;
}

void OverviewEffect::activate()
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_overviewState->activate();
}

void OverviewEffect::deactivate()
{
    const auto screens = effects->screens();
    m_shutdownTimer->start(animationDuration());
    m_overviewState->deactivate();
}

void OverviewEffect::realDeactivate()
{
    if (m_overviewState->status() == EffectTogglableState::Status::Inactive) {
        setRunning(false);
    }
}

void OverviewEffect::cycle()
{
    if (m_overviewState->status() == EffectTogglableState::Status::Inactive) {
        m_overviewState->activate();
    } else if (m_transitionState->status() == EffectTogglableState::Status::Inactive) {
        m_transitionState->activate();
    } else if (m_gridState->status() == EffectTogglableState::Status::Active) {
        m_overviewState->deactivate();
    }
}

void OverviewEffect::reverseCycle()
{
    if (m_overviewState->status() == EffectTogglableState::Status::Active) {
        m_overviewState->deactivate();
    } else if (m_transitionState->status() == EffectTogglableState::Status::Active) {
        m_transitionState->deactivate();
    } else if (m_gridState->status() == EffectTogglableState::Status::Inactive) {
        m_gridState->activate();
    }
}

} // namespace KWin

#include "moc_overvieweffect.cpp"
