/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Eric Edlund <ericedlund@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "contexts.h"
#include "contexts_p.h"
// KWin
#include "input.h"
#include "main.h"
#include "screenedge.h"
#include "utils/common.h"
// Qt
#include <QDebug>
// C++
#include <cmath>
// KConfig
#include <KCoreConfigSkeleton>
#include <qglobal.h>

Q_LOGGING_CATEGORY(KWIN_CONTEXTS, "kwin_contexts", QtDebugMsg);

namespace KWin
{

static AnimationDirection animationDirection(GestureDirections d, qreal progress = 0, QSizeF delta = QSizeF(0, 0))
{
    //TODO: Make configuration option for natural scrolling with context switching
    // Fingers moving up, stuff on screen goes up, new contexnt comes from the bottom
    // It's like there's a piece of paper under your fingers
    const bool natural = true;

    typedef AnimationDirection Move;

    if (d.testFlag(GestureDirection::DirectionlessSwipe)) {
        if (std::abs(delta.height()) > std::abs(delta.width())) {
            if (delta.height() > 0) {
                return natural ? Move::Up : Move::Down;
            } else {
                return natural ? Move::Down : Move::Up;
            }
        } else {
            if (delta.width() > 0) {
                return natural ? Move::Right : Move::Left;
            } else {
                return natural ? Move::Left : Move::Right;
            }
        }
    } else if (d.testFlag(GestureDirection::VerticalAxis)) {
        if (progress > 0) {
            return natural ? Move::Up : Move::Down;
        } else {
            return natural ? Move::Down : Move::Up;
        }
    } else if (d.testFlag(GestureDirection::HorizontalAxis)) {
        if (progress > 0) {
            return natural ? Move::Right : Move::Left;
        } else {
            return natural ? Move::Left : Move::Right;
        }
    } else if (d.testFlag(GestureDirection::BiDirectionalPinch)) {
        if (progress > 0) {
            return Move::Expanding;
        } else {
            return Move::Contracting;
        }
    } else if (d & GestureDirection::Up) {
        return natural ? Move::Up : Move::Down;
    } else if (d & GestureDirection::Down) {
        return natural ? Move::Down : Move::Up;
    } else if (d & GestureDirection::Left) {
        return natural ? Move::Left : Move::Right;
    } else if (d & GestureDirection::Right) {
        return natural ? Move::Right : Move::Left;
    } else if (d & GestureDirection::Contracting) {
        return Move::Contracting;
    } else if (d & GestureDirection::Expanding) {
        return Move::Contracting;
    }

    return Move::None;
}

//******************************
// Action
//******************************

Action::Action(const QString humanReadableLabel, const QString actionName)
    : humanReadableLabel(humanReadableLabel)
    , name(actionName)
{
    ContextManager::s_self->registerAction(this);
}

Action::~Action()
{
    ContextManager::s_self->unregisterAction(this);
}

void Action::cancelledSlot()
{
    m_pastTriggerThreshold = false;
    Q_EMIT inProgress(false);
    Q_EMIT gestureReleased();
    Q_EMIT cancelled();
}

void Action::triggeredSlot()
{
    m_pastTriggerThreshold = false;
    Q_EMIT inProgress(false);
    Q_EMIT gestureReleased();
    Q_EMIT triggered();
}

void Action::semanticProgressSlot(qreal progress, GestureDirections d)
{
    Q_EMIT inProgress(true);
    if (progress > 0.2 && !m_pastTriggerThreshold) {
        m_pastTriggerThreshold = true;
        Q_EMIT crossTriggerThreshold(true);
    }
    if (progress < 0.2 && m_pastTriggerThreshold) {
        m_pastTriggerThreshold = false;
        Q_EMIT crossTriggerThreshold(false);
    }
    Q_EMIT semanticProgressUpdate(progress, d);
}

void Action::semanticAxisSlot(qreal progress, GestureDirections d)
{
    Q_EMIT semanticAxisUpdate(progress, d);
}

void Action::semanticDeltaSlot(const QSizeF progress, GestureDirections d)
{
    Q_EMIT semanticDeltaUpdate(progress, d);
}

void Action::pixelDeltaSlot(const QSizeF progress, GestureDirections d)
{
    Q_EMIT pixelDeltaUpdate(progress, d);
}

//*****************************
// Context
//*****************************

Context::Context(const QString label, const QString name)
    : humanReadableLabel(label)
    , name(name)
    , activationParameters(std::make_shared<std::vector<Parameter>>())
{
    // Because these connections are made first,
    // the state will be updated before any other things
    // connected to the signals
    connect(this, &Context::activated, [this]() {
        m_state = State::Active;
    });
    connect(this, &Context::activating, [this]() {
        m_state = State::Activating;
    });
    connect(this, &Context::deactivating, [this]() {
        m_state = State::Deactivating;
    });
    connect(this, &Context::deactivated, [this]() {
        m_state = State::Inactive;
    });

    contexts()->registerContext(this);
}

Context::~Context()
{
    contexts()->unregisterContext(this);
}

//********************************
// ContextInstance
//********************************

QVariantAnimation ContextInstance::s_animator = new QVariantAnimation();
std::set<ContextInstance *> ContextInstance::s_contextInstances = std::set<ContextInstance *>();

void ContextInstance::bindGestureContextInstance(const QString targetContextInstance,
                                                 GestureDeviceType device,
                                                 GestureDirection direction,
                                                 uint fingerCount)
{
    // If possible, better to alter the finger count on an existing gesture than create a new one
    for (auto &&binding : qAsConst(m_bindings)) {
        if (qobject_cast<GestureToContextInstance *>(binding.get())) {
            GestureToContextInstance *b = qobject_cast<GestureToContextInstance *>(binding.get());

            if (b->targetContextInstanceName == targetContextInstance && b->gesture->direction().testFlag(direction) && b->device == device) {
                b->gesture->addFingerCount(fingerCount);
                return;
            }
        }
    }

    Gesture *gesture;
    if (isSwipeDirection(direction)) {
        gesture = new SwipeGesture();
        qobject_cast<SwipeGesture *>(gesture)->setTriggerDelta(QSizeF(200, 200));
        gesture->setDirection(direction);
    } else if (isPinchDirection(direction)) {
        gesture = new PinchGesture();
        gesture->setDirection(direction);
    }
    gesture->addFingerCount(fingerCount);
    m_bindings.insert(std::make_unique<GestureToContextInstance>(gesture, device, targetContextInstance));

    refreshBindings();
}

void ContextInstance::bindGestureAction(const QString actionName, GestureDeviceType device, GestureDirection direction, uint fingerCount)
{
    // If possible, better to alter the finger count on an existing gesture than create a new one
    for (auto &&binding : m_bindings) {
        if (qobject_cast<GestureToAction *>(binding.get())) {
            GestureToAction *b = qobject_cast<GestureToAction *>(binding.get());

            if (b->actionName == actionName && b->gesture->direction().testFlag(direction) && b->device == device) {
                b->gesture->addFingerCount(fingerCount);
                refreshBindings();
                return;
            }
        }
    }

    Gesture *gesture;
    if (isSwipeDirection(direction)) {
        gesture = new SwipeGesture();
        qobject_cast<SwipeGesture *>(gesture)->setTriggerDelta(QSizeF(200, 200));
        gesture->setDirection(direction);
    } else if (isPinchDirection(direction)) {
        gesture = new PinchGesture();
        gesture->setDirection(direction);
    }
    gesture->addFingerCount(fingerCount);

    input()->shortcuts()->registerGesture(gesture, device);

    m_bindings.insert(std::make_unique<GestureToAction>(gesture, device, actionName, true));

    refreshBindings();
}

void ContextInstance::bindGlobalTouchBorder(const QString contextName, QList<int> *borders, const QHash<QString, QVariant> parameters)
{
    Binding *binding = new TouchborderToContext(borders, contextName, parameters);
    ContextManager::self()->globalBindings.insert(binding);
    binding->activate();
}

void ContextInstance::setActivationParameters(QHash<QString, QVariant> &params)
{
    for (QString param : params.keys()) {
        m_activationParameters[param] = params[param];
    }
}

std::shared_ptr<ContextInstance> ContextInstance::s_switchingTo;
std::shared_ptr<ContextInstance> ContextInstance::s_switchingFrom;
std::shared_ptr<ContextInstance> ContextInstance::s_current;
AnimationDirection ContextInstance::s_switchDirection = AnimationDirection::None;
bool ContextInstance::s_transitionMustComplete;
qreal ContextInstance::s_animationProgress;
qreal ContextInstance::s_gestureProgress;

void ContextInstance::switchTo(std::shared_ptr<ContextInstance> to)
{
    if (s_transitionMustComplete) {
        return;
    }
    if (!to->isActivatable()) {
        return;
    }
    s_animationProgress += s_gestureProgress;
    s_gestureProgress = 0;

    // This is complicated because there is no "transitioning" state.
    // We can switchTo() anywhere at any time.
    if (s_switchingTo) {
        if (to == s_current) {
            if (s_switchingTo == s_current) {
                return; // Nothing has changed
            } else {
                s_switchingTo->deactivateWithAnimation(transitionProgress());
                s_animationProgress = std::clamp(1 - s_animationProgress, 0.0, 1.0);
            }
        } else {
            if (s_switchingTo != s_current) {
                s_switchingTo->deactivateWithAnimation(transitionProgress());
            } else {
                s_animationProgress = std::clamp(1 - s_animationProgress, 0.0, 1.0);
            }
        }
    }
    s_switchingTo = to;

    s_animator.stop();
    disconnect(&s_animator, nullptr, nullptr, nullptr);

    // We're running the switch animation and sending out
    // progress updates to keep everything in sync
    s_animator.setStartValue(s_animationProgress);
    s_animator.setEndValue(1.0);
    s_animator.setDuration(CONTEXT_SWITCH_ANIMATION_DURATION * (1 - s_animationProgress));
    s_animator.setEasingCurve(QEasingCurve::InOutCubic);

    connect(&s_animator, &QVariantAnimation::valueChanged, [](const QVariant &value) {
        double progress = value.toDouble();
        s_animationProgress = progress;

        // It's important that we update leaving before entering.
        // If the old context is holding activeFullscreenEffect,
        // emitting leaving will make it release it allowing the new context
        // to start it's animation
        if (s_current != s_switchingTo) {
            s_current->deactivating(progress, s_switchDirection);
        }
        s_switchingTo->activating(progress, s_switchDirection);
    });
    connect(&s_animator, &QVariantAnimation::finished, []() {
        disconnect(&s_animator, nullptr, nullptr, nullptr);
        handleSwitchComplete(s_switchingTo);
    });
    s_animator.start();
}

void ContextInstance::switchTo(Context *context)
{
    switchTo(getClosestInstanceOf(context->name));
}

void ContextInstance::initActivate()
{
    handleSwitchComplete(shared_from_this());
}

void ContextInstance::handleSwitchComplete(std::shared_ptr<ContextInstance> to)
{
    Q_ASSERT(to->isActivatable());
    s_animationProgress = 0;
    s_animator.stop();
    s_switchingTo = nullptr;
    s_transitionMustComplete = false;

    if (to == s_current) {
        to->activated();
        return;
    }

    qCDebug(KWIN_CONTEXTS) << "Context Changed: Context=" + to->baseContextName + " ContextInstance=" + to->name;

    // Handle old context
    for (auto &&b : s_current->m_bindings) {
        b->deactivate();
    }
    s_current->deactivated();

    // Handle new context
    for (auto &&b : to->m_bindings) {
        b->activate();
    }
    to->activated();

    s_current = to;
    Q_EMIT contexts()->contextChanged(s_current->baseContextName);
}

void ContextInstance::deactivateWithAnimation(qreal activationProgress)
{
    m_deactivationPointer = shared_from_this();

    m_deactivationAnimation = std::make_unique<QVariantAnimation>();
    m_deactivationAnimation->setStartValue(1 - activationProgress);
    m_deactivationAnimation->setEndValue(1.0);
    m_deactivationAnimation->setDuration(activationProgress * CONTEXT_SWITCH_ANIMATION_DURATION);
    m_deactivationAnimation->setEasingCurve(QEasingCurve::InOutCubic);

    connect(m_deactivationAnimation.get(), &QVariantAnimation::valueChanged, [this](const QVariant &value) {
        deactivating(value.toDouble(), s_switchDirection);
    });
    connect(m_deactivationAnimation.get(), &QVariantAnimation::finished, [this]() {
        disconnect(m_deactivationAnimation.get(), nullptr, nullptr, nullptr);
        m_deactivationAnimation.reset();
        deactivated();
        m_deactivationPointer.reset();
    });
    m_deactivationAnimation->start();
}

std::shared_ptr<ContextInstance> ContextInstance::getClosestInstanceOf(const QString context, QHash<QString, QVariant> parameters)
{
    Q_ASSERT(contexts()->m_contexts.contains(context));

    for (ContextInstance *instance : qAsConst(s_contextInstances)) {
        if (instance->baseContextName == context && instance->m_activationParameters == parameters) {
            return instance->shared_from_this();
        }
    }

    // If there isn't already a contextInstance for that context
    std::shared_ptr<ContextInstance> instance = std::make_shared<ContextInstance>(context);
    instance->setActivationParameters(parameters);
    return instance;
}

void ContextInstance::activated()
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_switchingTo.get() != this) {
        return;
    }
    // If deactivating with animation, stop because we're being activated
    if (m_deactivationAnimation) {
        m_deactivationAnimation.reset();
    }

    if (baseContext->state() == Context::State::Inactive) {
        Q_EMIT baseContext->setActivationParameters(m_activationParameters);
    }

    Q_EMIT baseContext->activated();
}

void ContextInstance::activating(qreal progress, AnimationDirection direction)
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_switchingTo.get() != this) {
        return;
    }
    // If deactivating with animation, stop because we're being activated
    if (m_deactivationAnimation) {
        m_deactivationAnimation.reset();
    }

    if (baseContext->state() == Context::State::Inactive) {
        Q_EMIT baseContext->setActivationParameters(m_activationParameters);
    }

    Q_EMIT baseContext->activating(progress, direction);
    Q_EMIT contexts()->contextChanging(s_current->baseContextName, baseContextName, progress);
}

void ContextInstance::deactivating(qreal progress, AnimationDirection direction)
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_current.get() != this) {
        return;
    }

    Q_EMIT baseContext->deactivating(progress, direction);
}

void ContextInstance::deactivated()
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_current.get() != this) {
        return;
    }

    Q_EMIT baseContext->deactivated();
}

bool ContextInstance::isActivatable() const
{
    return (bool)baseContext;
}

void ContextInstance::informContextsOfRegisteredContext(Context *context)
{
    Q_ASSERT(context);
    for (ContextInstance *instance : qAsConst(s_contextInstances)) {
        instance->tryAdd(context);
    }
    refreshBindings();
}

void ContextInstance::informContextsOfUnregisteredContext(Context *context)
{
    // Remove refrences to this context from ContextInstances
    for (ContextInstance *instance : qAsConst(s_contextInstances)) {
        if (instance->baseContextName == context->name) {
            instance->baseContext = nullptr;
        }
    }

    if (s_current->baseContextName == context->name) {
        contexts()->m_rootContextInstance->initActivate();
    }
    if (s_switchingTo) {
        if (s_switchingTo->baseContextName == context->name) {
            contexts()->m_rootContextInstance->initActivate();
        }
    }

    refreshBindings();
}

void ContextInstance::tryAdd(Context *context)
{
    if (baseContextName != context->name) {
        return;
    }
    baseContext = context;

    // Load defaults
    for (Parameter p : *qAsConst(context->activationParameters)) {
        if (!m_activationParameters.contains(p.name)) {
            if (!p.defaultValue.isNull()) {
                m_activationParameters[p.name] = p.defaultValue;
            }
        }
    }
}

void ContextInstance::refreshBindings()
{
    for (auto &&b : s_current->m_bindings) {
        b->activate();
    }
    for (auto &&b : ContextManager::self()->globalBindings) {
        b->activate();
    }
}

ContextInstance::ContextInstance(const QString baseContextName, const QString name, QHash<QString, QVariant> parameters)
    : baseContextName(baseContextName)
    , name(name)
    , baseContext(contexts()->m_contexts.value(baseContextName))
    , m_activationParameters(parameters)
{
    s_contextInstances.insert(this);

    if (contexts()->m_contexts.contains(baseContextName)) {
        tryAdd(contexts()->m_contexts[baseContextName]);
    }
}

ContextInstance::~ContextInstance()
{
    s_contextInstances.erase(this);
    ContextManager::self()->m_contextInstances.remove(name);
    m_bindings.clear();
}

//****************************
// Binding
//****************************

Binding *Binding::s_activeBinding;

void Binding::activate()
{
    deactivateImpl(); // Make sure nothing is double bound
    activateImpl();
}

void Binding::deactivate()
{
    if (s_activeBinding == this) {
        s_activeBinding = nullptr;
    }
    deactivateImpl();
}

//************************************
// GestureToAction : public Binding
//************************************

GestureToAction::GestureToAction(Gesture *gesture, GestureDeviceType device, const QString actionName, bool)
    : gesture(gesture)
    , device(device)
    , actionName(actionName)
{
}

GestureToAction::~GestureToAction()
{
    deactivate();
};

void GestureToAction::activateImpl()
{
    m_action = ContextManager::self()->m_actions.value(actionName);
    if (!m_action) {
        return;
    }

    connect(gesture.get(), &Gesture::triggered, m_action, &Action::triggeredSlot, Qt::QueuedConnection);
    connect(gesture.get(), &Gesture::cancelled, m_action, &Action::cancelledSlot, Qt::QueuedConnection);

    connect(gesture.get(), &Gesture::semanticProgress, m_action, &Action::semanticProgressSlot);
    connect(gesture.get(), &Gesture::semanticProgressAxis, m_action, &Action::semanticAxisSlot);
    if (qobject_cast<SwipeGesture *>(gesture.get())) {
        SwipeGesture *swipeGesture = qobject_cast<SwipeGesture *>(gesture.get());
        connect(swipeGesture, &SwipeGesture::semanticDelta, m_action, &Action::semanticDeltaSlot);
        connect(swipeGesture, &SwipeGesture::pixelDelta, m_action, &Action::pixelDeltaSlot);
    }
    input()->shortcuts()->registerGesture(gesture.get(), device);
}

void GestureToAction::deactivateImpl()
{
    disconnect(gesture.get(), nullptr, nullptr, nullptr);
    input()->shortcuts()->unregisterGesture(gesture.get(), device);
}

//*******************************************
// GestureToContextInstance : public Binding
//*******************************************

GestureToContextInstance::GestureToContextInstance(Gesture *gesture, GestureDeviceType device, const QString targetContextInstanceName)
    : targetContextInstanceName(targetContextInstanceName)
    , gesture(gesture)
    , device(device)
    , m_to(ContextManager::self()->m_contextInstances.value(targetContextInstanceName))
{
    input()->shortcuts()->registerGesture(gesture, device);
}

GestureToContextInstance::~GestureToContextInstance()
{
    deactivate();
    input()->shortcuts()->unregisterGesture(gesture.get(), device);
}

void GestureToContextInstance::activateImpl()
{
    m_to = ContextManager::self()->m_contextInstances.value(targetContextInstanceName);
    if (!m_to) {
        return;
    }
    if (!m_to->isActivatable()) {
        return;
    }

    // Forward the gesture updates
    connect(gesture.get(), &Gesture::semanticProgress, [this](qreal progress, GestureDirections d) {
        if (s_activeBinding == this || s_activeBinding == nullptr) {
            s_activeBinding = this;

            // Stop the timer so the animation can follow the gesture
            ContextInstance::s_animator.stop();

            ContextInstance::s_switchingTo = m_to;
            ContextInstance::s_gestureProgress = progress;
            ContextInstance::s_switchDirection = animationDirection(d, progress);

            ContextInstance::s_current->deactivating(ContextInstance::transitionProgress(), ContextInstance::s_switchDirection);
            m_to->activating(ContextInstance::transitionProgress(), ContextInstance::s_switchDirection);
        }
    });
    connect(gesture.get(), &Gesture::triggered, [this]() {
        if (s_activeBinding == this) {
            s_activeBinding = nullptr;
            ContextInstance::switchTo(m_to);
        }
    });
    connect(gesture.get(), &Gesture::cancelled, [this]() {
        if (s_activeBinding == this) {
            s_activeBinding = nullptr;
            ContextInstance::switchTo(ContextInstance::s_current);
        }
    });
    input()->shortcuts()->registerGesture(gesture.get(), device);
}

void GestureToContextInstance::deactivateImpl()
{
    disconnect(gesture.get(), nullptr, nullptr, nullptr);
}

//*******************************************
// TouchborderToContext : public Binding
//*******************************************

TouchborderToContext::TouchborderToContext(QList<int> *touchBordersRef, const QString contextName, const QHash<QString, QVariant> parameters)
    : activationParameters(parameters)
    , targetContextName(contextName)
    , touchBordersRef(touchBordersRef)
    , m_touchscreenBorderOnUpAction(std::make_unique<QAction>())
{
}

TouchborderToContext::~TouchborderToContext()
{
    deactivate();
}

void TouchborderToContext::activateImpl()
{
    if (!ContextManager::self()->m_contexts.contains(targetContextName)) {
        return;
    }

    m_to = ContextInstance::getClosestInstanceOf(targetContextName, activationParameters);

    if (!m_to->isActivatable()) {
        return;
    }

    connect(m_touchscreenBorderOnUpAction.get(), &QAction::triggered, this, &TouchborderToContext::touchscreenBorderOnUp);

    auto touchMotionCallback = [this](ElectricBorder, const QSizeF &deltaProgress, Output *) {
        if (m_to->baseContext->state() == Context::State::Active) {
            return;
        }
        qreal progress = std::hypot(deltaProgress.width(), deltaProgress.height()) / 500;

        ContextInstance::s_switchingTo = m_to;
        ContextInstance::s_gestureProgress = progress;
        ContextInstance::s_switchDirection = AnimationDirection::None;
        //TODO: Interpret the delta given here to
        // a GestureDirection. Then use that to get
        // an AnimationDirection that conforms to whatever
        // settings we have for scroll direction in the future

        // Stop the timer so the animation can follow the gesture
        ContextInstance::s_animator.stop();

        ContextInstance::s_current->deactivating(progress, ContextInstance::s_switchDirection);
        m_to->activating(progress, ContextInstance::s_switchDirection);
    };

    for (int i : qAsConst(*touchBordersRef)) {
        ScreenEdges::self()->reserveTouch(ElectricBorder(i), m_touchscreenBorderOnUpAction.get(), touchMotionCallback);
        m_currentlyBoundBorders.push_back(ElectricBorder(i));
    }
}

void TouchborderToContext::deactivateImpl()
{
    for (ElectricBorder b : qAsConst(m_currentlyBoundBorders)) {
        ScreenEdges::self()->unreserveTouch(b, m_touchscreenBorderOnUpAction.get());
    }
    m_currentlyBoundBorders.clear();
    disconnect(m_touchscreenBorderOnUpAction.get(), &QAction::triggered, this, &TouchborderToContext::touchscreenBorderOnUp);
}

void TouchborderToContext::touchscreenBorderOnUp()
{
    ContextInstance::transitionProgress() > 0.5 ? ContextInstance::switchTo(m_to) : ContextInstance::switchTo(ContextInstance::s_current);
}

//************************************
// ContextManager
//************************************

KWIN_SINGLETON_FACTORY(ContextManager)

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
{
}

void ContextManager::init()
{
    m_rootContextInstance = std::make_shared<RootContextInstance>();
    m_contextInstances[m_rootContextInstance->name] = m_rootContextInstance;
    ContextInstance::s_current = m_rootContextInstance;
    m_rootContextInstance->initActivate();
    registerContext(new Context("System Default", DEFAULT_CONTEXT));

    initLoadConfig();
}

void ContextManager::registerContext(Context *context)
{
    if (m_contexts.contains(context->name)) {
        unregisterContext(context);
    }
    qCDebug(KWIN_CONTEXTS) << "Registered Context: " + context->name;

    m_contexts[context->name] = context;

    connect(context, &Context::grabActive, [context](bool forced) {
        ContextInstance::switchTo(context);
        if (!ContextInstance::s_transitionMustComplete) {
            ContextInstance::s_transitionMustComplete = forced;
        }
    });
    connect(context, &Context::ungrabActive, [this, context](bool forced) {
        if (ContextInstance::s_current->baseContextName == context->name) {
            ContextInstance::switchTo(m_rootContextInstance);
        } else if (ContextInstance::s_switchingTo) {
            if (ContextInstance::s_switchingTo->baseContextName == context->name) {
                ContextInstance::switchTo(ContextInstance::s_current);
            }
        }
        if (!ContextInstance::s_transitionMustComplete) {
            ContextInstance::s_transitionMustComplete = forced;
        }
    });
    ContextInstance::informContextsOfRegisteredContext(context);
}

void ContextManager::unregisterContext(Context *context)
{
    qCDebug(KWIN_CONTEXTS) << "Unregistered Context: " + context->name;
    m_contexts.remove(context->name);
    disconnect(context, &Context::grabActive, nullptr, nullptr);
    disconnect(context, &Context::ungrabActive, nullptr, nullptr);

    ContextInstance::informContextsOfUnregisteredContext(context);
}

void ContextManager::registerAction(Action *action)
{
    if (m_actions.contains(action->name)) {
        unregisterAction(action);
    }
    qCDebug(KWIN_CONTEXTS) << "Registered Action: " + action->name;

    m_actions[action->name] = action;

    ContextInstance::refreshBindings();
}

void ContextManager::unregisterAction(Action *action)
{
    qCDebug(KWIN_CONTEXTS) << "Unregistered Action: " + action->name;
    m_actions.remove(action->name);
    ContextInstance::refreshBindings();
}

void ContextManager::initLoadConfig()
{
    // List of hard coded bindings to substitute configuration
    // This stuff will end up in a beautiful json file later

    QString switchDesktop = "switchDesktop";
    m_rootContextInstance->bindGestureAction(switchDesktop, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 3);
    m_rootContextInstance->bindGestureAction(switchDesktop, GestureDeviceType::Touchpad, GestureDirection::VerticalAxis, 3);
    m_rootContextInstance->bindGestureAction(switchDesktop, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 4);
    m_rootContextInstance->bindGestureAction(switchDesktop, GestureDeviceType::Touchscreen, GestureDirection::HorizontalAxis, 3);

    // Overview
    auto overview1 = std::make_shared<ContextInstance>("overview", "Overview 1");
    m_contextInstances[overview1->name] = overview1;
    m_rootContextInstance->bindGestureContextInstance(overview1->name, GestureDeviceType::Touchpad, GestureDirection::Contracting, 4);
    m_rootContextInstance->bindGestureContextInstance(overview1->name, GestureDeviceType::Touchscreen, GestureDirection::VerticalAxis, 3);
    overview1->bindGestureContextInstance(m_rootContextInstance->name, GestureDeviceType::Touchpad, GestureDirection::Expanding, 4);
    overview1->bindGestureContextInstance(m_rootContextInstance->name, GestureDeviceType::Touchscreen, GestureDirection::VerticalAxis, 3);

    /* TODO: When uncommented, these lines allow desktop switching while in the overview context.
    * The switching will respect overview's display of all the desktops in a line.
    * To implement, make overview respond to desktopChanging() and desktopChanged() in effects.
    * Without doing that, it doesn't repaint and there's no changing animation.
    *
    QString switchDesktopOrdered = "virtualdesktopsmanager";
    overview1->bindGestureAction(switchDesktopOrdered, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 3);
    overview1->bindGestureAction(switchDesktopOrdered, GestureDeviceType::Touchscreen, GestureDirection::HorizontalAxis, 3);
    overview1->bindGestureAction(switchDesktopOrdered, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 4);
    */

    // Window View
    auto windowview1 = std::make_shared<ContextInstance>("windowview", "Window View 1");
    m_contextInstances[windowview1->name] = windowview1;
    m_rootContextInstance->bindGestureContextInstance(windowview1->name, GestureDeviceType::Touchpad, GestureDirection::Down, 4);
    windowview1->bindGestureContextInstance(m_rootContextInstance->name, GestureDeviceType::Touchpad, GestureDirection::Up, 4);

    // Desktop Grid
    auto desktopgrid1 = std::make_shared<ContextInstance>("desktopgrid", "Desktop Grid 1");
    m_contextInstances[desktopgrid1->name] = desktopgrid1;
    m_rootContextInstance->bindGestureContextInstance(desktopgrid1->name, GestureDeviceType::Touchpad, GestureDirection::Up, 4);
    desktopgrid1->bindGestureContextInstance(m_rootContextInstance->name, GestureDeviceType::Touchpad, GestureDirection::Down, 4);

    // Read and activate touch border bindings from the effects that provide them.
    // This is a temorary solution to avoid regression in configuration of touchborders.
    // Later, touch border configs will be centrally managed by this system
    // and effects wont know how they are bound.
    configReader = std::make_unique<KCoreConfigSkeleton>(kwinApp()->config(), this);
    configReader->setCurrentGroup("Effect-windowview");
    QList<int> *windowView_activateCurrent = new QList<int>();
    QList<int> *windowView_activateAll = new QList<int>();
    QList<int> *windowView_activateClass = new QList<int>();
    configReader->addItemIntList("TouchBorderActivate", *windowView_activateCurrent);
    configReader->addItemIntList("TouchBorderActivateAll", *windowView_activateAll);
    configReader->addItemIntList("TouchBorderActivateClass", *windowView_activateClass);

    configReader->setCurrentGroup("Effect-desktopgrid");
    QList<int> *desktopGrid_activate = new QList<int>();
    configReader->addItemIntList("TouchBorderActivate", *desktopGrid_activate);

    configReader->setCurrentGroup("Effect-overview");
    QList<int> *overview_activate = new QList<int>();
    configReader->addItemIntList("TouchBorderActivate", *overview_activate);

    typedef QHash<QString, QVariant> Params;

    Params winViewCurrentP = Params();
    winViewCurrentP["windows"] = "ModeCurrentDesktop";
    ContextInstance::bindGlobalTouchBorder("windowview", windowView_activateCurrent, winViewCurrentP);

    Params winViewAllP = Params();
    winViewCurrentP["windows"] = "ModeAllDesktops";
    ContextInstance::bindGlobalTouchBorder("windowview", windowView_activateAll, winViewAllP);

    Params winViewClassP = Params();
    winViewClassP["windows"] = "ModeWindowClass";
    ContextInstance::bindGlobalTouchBorder("windowview", windowView_activateClass, winViewClassP);

    ContextInstance::bindGlobalTouchBorder("desktopgrid", desktopGrid_activate, Params());
    ContextInstance::bindGlobalTouchBorder("overview", overview_activate, Params());

    ContextInstance::refreshBindings();
}

void ContextManager::reconfigure()
{
    configReader->read();
    ContextInstance::refreshBindings();
}

void ContextManager::setContext(const QString context)
{
    if (!m_contexts.contains(context)) {
        return;
    }
    ContextInstance::switchTo(m_contexts.value(context));
}

QString ContextManager::currentContext() const
{
    if (!ContextInstance::s_current) {
        return "";
    }
    return ContextInstance::s_current->baseContextName;
}

uint ContextManager::contextSwitchAnimationDuration() const
{
    return CONTEXT_SWITCH_ANIMATION_DURATION;
}

} // namespace
