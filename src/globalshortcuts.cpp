/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "globalshortcuts.h"
// config
#include <config-kwin.h>
// kwin
#include "gestures.h"
#include "kwinglobals.h"
#include "main.h"
#include "options.h"
#include "utils/common.h"
// KDE
#include <KGlobalAccel/private/kglobalaccel_interface.h>
#include <KGlobalAccel/private/kglobalacceld.h>
// Qt
#include <QAction>
// system
#include <signal.h>
#include <variant>

#include <KConfigWatcher>
#include <iostream>

Q_LOGGING_CATEGORY(KWIN_CONTEXTS, "kwin_contexts", QtDebugMsg)

namespace KWin
{

qreal GlobalShortcutsManager::ContextInstance::s_animationProgress = 0;
qreal GlobalShortcutsManager::ContextInstance::s_gestureProgress = 0;
GlobalShortcutsManager::ContextInstance::Binding *GlobalShortcutsManager::ContextInstance::Binding::activeBinding = nullptr;
std::shared_ptr<GlobalShortcutsManager::ContextInstance> GlobalShortcutsManager::ContextInstance::s_switchingTo = nullptr;
QVector<GlobalShortcutsManager::ContextInstance *> GlobalShortcutsManager::ContextInstance::s_contextInstances = QVector<ContextInstance *>();
static QVariantAnimation s_animator = new QVariantAnimation();
QSet<GlobalShortcutsManager::ContextInstance::Binding *> GlobalShortcutsManager::ContextInstance::s_globalBindings = QSet<GlobalShortcutsManager::ContextInstance::Binding *>();
std::shared_ptr<GlobalShortcutsManager::ContextInstance> GlobalShortcutsManager::ContextInstance::s_current = nullptr;
GlobalShortcutsManager *GlobalShortcutsManager::ContextInstance::s_manager = nullptr;
AnimationDirection GlobalShortcutsManager::ContextInstance::s_switchDirection = AnimationDirection::None;
bool GlobalShortcutsManager::ContextInstance::s_transitionMustComplete = false;

KCoreConfigSkeleton *configReader;

GestureDirection oppositeOf(GestureDirection d)
{
    switch (d) {
    case GestureDirection::Left:
        return GestureDirection::Right;
    case GestureDirection::Right:
        return GestureDirection::Left;
    case GestureDirection::Up:
        return GestureDirection::Down;
    case GestureDirection::Down:
        return GestureDirection::Up;
    case GestureDirection::Expanding:
        return GestureDirection::Contracting;
    case GestureDirection::Contracting:
        return GestureDirection::Expanding;
    default:
        return d;
    }
}

static QString string(GestureDirection direction)
{
    switch (direction) {
    case GestureDirection::Up:
        return "Up";
    case GestureDirection::Down:
        return "Down";
    case GestureDirection::Left:
        return "Left";
    case GestureDirection::Right:
        return "Right";
    case GestureDirection::HorizontalAxis:
        return "HorizontalAxis";
    case GestureDirection::VerticalAxis:
        return "VerticalAxis";
    case GestureDirection::DirectionlessSwipe:
        return "DirectionlessSwipe";
    case GestureDirection::Contracting:
        return "Contracting";
    case GestureDirection::Expanding:
        return "Expanding";
    case GestureDirection::BiDirectionalPinch:
        return "BiDirectionalPinch";
    default:
        Q_UNREACHABLE();
    }
}

GlobalShortcut::GlobalShortcut(Shortcut &&sc, QAction *action)
    : m_shortcut(sc)
    , m_action(action)
{
}

GlobalShortcut::~GlobalShortcut()
{
}

QAction *GlobalShortcut::action() const
{
    return m_action;
}

void GlobalShortcut::invoke() const
{
    QMetaObject::invokeMethod(m_action, "trigger", Qt::QueuedConnection);
}

const Shortcut &GlobalShortcut::shortcut() const
{
    return m_shortcut;
}

GlobalShortcutsManager::GlobalShortcutsManager(QObject *parent)
    : QObject(parent)
    , m_touchpadGestureRecognizer(new GestureRecognizer(this))
    , m_touchscreenGestureRecognizer(new GestureRecognizer(this))
{
}

GlobalShortcutsManager::~GlobalShortcutsManager()
{
}

void GlobalShortcutsManager::init()
{
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        qputenv("KGLOBALACCELD_PLATFORM", QByteArrayLiteral("org.kde.kwin"));
        m_kglobalAccel = new KGlobalAccelD(this);
        if (!m_kglobalAccel->init()) {
            qCDebug(KWIN_CORE) << "Init of kglobalaccel failed";
            delete m_kglobalAccel;
            m_kglobalAccel = nullptr;
        } else {
            qCDebug(KWIN_CORE) << "KGlobalAcceld inited";
        }
    }

    ContextInstance::s_manager = this;
    m_rootContextInstance = std::make_unique<ContextInstance>(DEFAULT_CONTEXT, "Root Context");
    m_rootContextInstance->initActivate();
    registerContext(new Context("System Default", DEFAULT_CONTEXT));

    loadGestureConfig();
}

void GlobalShortcutsManager::objectDeleted(QObject *object)
{
    auto it = m_shortcuts.begin();
    while (it != m_shortcuts.end()) {
        if (it->action() == object) {
            it = m_shortcuts.erase(it);
        } else {
            ++it;
        }
    }
}

bool GlobalShortcutsManager::addIfNotExists(GlobalShortcut sc)
{
    for (const auto &cs : qAsConst(m_shortcuts)) {
        if (sc.shortcut() == cs.shortcut()) {
            return false;
        }
    }

    connect(sc.action(), &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
    m_shortcuts.push_back(std::move(sc));
    return true;
}

void GlobalShortcutsManager::registerPointerShortcut(QAction *action, Qt::KeyboardModifiers modifiers, Qt::MouseButtons pointerButtons)
{
    addIfNotExists(GlobalShortcut(PointerButtonShortcut{modifiers, pointerButtons}, action));
}

void GlobalShortcutsManager::registerAxisShortcut(QAction *action, Qt::KeyboardModifiers modifiers, PointerAxisDirection axis)
{
    addIfNotExists(GlobalShortcut(PointerAxisShortcut{modifiers, axis}, action));
}

void GlobalShortcutsManager::loadGestureConfig()
{
    // List of hard coded bindings to substitute configuration
    // This stuff will end up in a beautiful XML file later

    QString switchDesktop = "switchDesktop";
    m_rootContextInstance->bindGesture(switchDesktop, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 3);
    m_rootContextInstance->bindGesture(switchDesktop, GestureDeviceType::Touchpad, GestureDirection::VerticalAxis, 3);
    m_rootContextInstance->bindGesture(switchDesktop, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 4);
    m_rootContextInstance->bindGesture(switchDesktop, GestureDeviceType::Touchscreen, GestureDirection::HorizontalAxis, 3);

    // Overview
    auto upOverview = std::make_shared<ContextInstance>("overview", "Up Overview");
    m_rootContextInstance->bindGesture(upOverview, GestureDeviceType::Touchpad, GestureDirection::Contracting, 4);
    m_rootContextInstance->bindGesture(upOverview, GestureDeviceType::Touchscreen, GestureDirection::VerticalAxis, 3);
    upOverview->bindGesture(m_rootContextInstance, GestureDeviceType::Touchpad, GestureDirection::Expanding, 4);
    upOverview->bindGesture(m_rootContextInstance, GestureDeviceType::Touchscreen, GestureDirection::VerticalAxis, 3);

    /* TODO: When uncommented, these lines allow desktop switching while in the overview context.
    * The switching will respect overview's display of all the desktops in a line.
    * To implement, make overview respond to desktopChanging() and desktopChanged() in effects.
    * Without doing that, it doesn't repaint and there's no changing animation.
    *
    QString switchDesktopOrdered = "virtualdesktopsmanager";
    upOverview->bind(switchDesktopOrdered, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 3);
    upOverview->bind(switchDesktopOrdered, GestureDeviceType::Touchscreen, GestureDirection::HorizontalAxis, 3);
    upOverview->bind(switchDesktopOrdered, GestureDeviceType::Touchpad, GestureDirection::HorizontalAxis, 4);
    */

    // Window View
    auto downWindowView = std::make_shared<ContextInstance>("windowview", "Down Present Windows");
    m_rootContextInstance->bindGesture(downWindowView, GestureDeviceType::Touchpad, GestureDirection::Down, 4);
    downWindowView->bindGesture(m_rootContextInstance, GestureDeviceType::Touchpad, GestureDirection::Up, 4);

    // Desktop Grid
    auto contractingDesktopGrid = std::make_shared<ContextInstance>("desktopgrid", "Contracting Desktop Grid");
    m_rootContextInstance->bindGesture(contractingDesktopGrid, GestureDeviceType::Touchpad, GestureDirection::Up, 4);
    contractingDesktopGrid->bindGesture(m_rootContextInstance, GestureDeviceType::Touchpad, GestureDirection::Down, 4);

    // Read and activate touch border bindings from the effects that provide them.
    // This is a temorary solution to avoid regression in configuration of touchborders.
    // Later, touch border configs will be centrally managed by this system
    // and effects wont know how they are bound.
    configReader = new KCoreConfigSkeleton(kwinApp()->config(), this);
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

void GlobalShortcutsManager::registerContext(Context *context)
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
    connect(context, &QObject::destroyed, [this, context]() {
        unregisterContext(context);
    });
    ContextInstance::informContextsOfRegisteredContext(context);
}

void GlobalShortcutsManager::unregisterContext(Context *context)
{
    qCDebug(KWIN_CONTEXTS) << "Unregistered Context: " + context->name;
    m_contexts.remove(context->name);
    disconnect(context, &Context::grabActive, nullptr, nullptr);
    disconnect(context, &Context::ungrabActive, nullptr, nullptr);

    ContextInstance::informContextsOfUnregisteredContext(context);
}

void GlobalShortcutsManager::registerAction(Action *action)
{
    if (m_actions.contains(action->name)) {
        unregisterAction(action);
    }
    qCDebug(KWIN_CONTEXTS) << "Registered Action: " + action->name;

    m_actions[action->name] = action;

    // Memory managment
    connect(action, &QObject::destroyed, [this, action]() {
        unregisterAction(action);
    });

    ContextInstance::refreshBindings();
}

void GlobalShortcutsManager::unregisterAction(Action *action)
{
    qCDebug(KWIN_CONTEXTS) << "Unregistered Action: " + action->name;
    m_actions.remove(action->name);
    ContextInstance::refreshBindings();
}

void GlobalShortcutsManager::ContextInstance::bindGesture(std::shared_ptr<ContextInstance> targetContext,
                                                          GestureDeviceType device,
                                                          GestureDirection direction,
                                                          uint fingerCount)
{
    // If possible, better to alter the finger count on an existing gesture than create a new one
    for (Binding *b : qAsConst(m_bindings)) {
        if (b->to == targetContext && b->gesture->direction == direction && b->gesture->device == device) {
            b->gesture->gesture()->addFingerCount(fingerCount);
            return;
        }
    }

    GestureShortcut *gesture = new GestureShortcut{device, direction};
    const auto &recognizer = device == GestureDeviceType::Touchpad ? s_manager->m_touchpadGestureRecognizer : s_manager->m_touchscreenGestureRecognizer;
    if (SWIPE_DIRECTIONS.contains(gesture->direction)) {
        gesture->swipeGesture.reset(new SwipeGesture());
        gesture->swipeGesture->setMinimumDelta(QSizeF(200, 200));
        gesture->swipeGesture->setDirection(gesture->direction);
        recognizer->registerSwipeGesture(gesture->swipeGesture.get());
    } else if (PINCH_DIRECTIONS.contains(gesture->direction)) {
        gesture->pinchGesture.reset(new PinchGesture());
        gesture->pinchGesture->setDirection(gesture->direction);
        recognizer->registerPinchGesture(gesture->pinchGesture.get());
    }
    gesture->gesture()->addFingerCount(fingerCount);

    Binding *binding = new Binding(gesture, targetContext);
    m_bindings.insert(binding);

    refreshBindings();
}

void GlobalShortcutsManager::ContextInstance::bindGesture(const QString slotName, GestureDeviceType device, GestureDirection direction, uint fingerCount)
{
    // If possible, better to alter the finger count on an existing gesture than create a new one
    for (Binding *b : qAsConst(m_bindings)) {
        if (b->type() != Binding::Type::Gesture_Action) {
            continue;
        }
        if (b->actionName == slotName && b->gesture->direction == direction && b->gesture->device == device) {
            b->gesture->gesture()->addFingerCount(fingerCount);
            refreshBindings();
            return;
        }
    }

    GestureShortcut *gesture = new GestureShortcut{device, direction};
    const auto &recognizer = device == GestureDeviceType::Touchpad ? s_manager->m_touchpadGestureRecognizer : s_manager->m_touchscreenGestureRecognizer;
    if (SWIPE_DIRECTIONS.contains(gesture->direction)) {
        gesture->swipeGesture.reset(new SwipeGesture());
        gesture->swipeGesture->setMinimumDelta(QSizeF(200, 200));
        gesture->swipeGesture->setDirection(gesture->direction);
        recognizer->registerSwipeGesture(gesture->swipeGesture.get());
    } else if (PINCH_DIRECTIONS.contains(gesture->direction)) {
        gesture->pinchGesture.reset(new PinchGesture());
        gesture->pinchGesture->setDirection(gesture->direction);
        recognizer->registerPinchGesture(gesture->pinchGesture.get());
    }
    gesture->gesture()->addFingerCount(fingerCount);

    Binding *binding = new Binding(gesture, slotName);
    m_bindings.insert(binding);

    refreshBindings();
}

void GlobalShortcutsManager::ContextInstance::bindGlobalTouchBorder(const QString contextName, QList<int> *borders, const QHash<QString, QVariant> parameters)
{
    Binding *binding = new Binding(borders, contextName, parameters);
    s_globalBindings.insert(binding);
    binding->activate();
}

void GlobalShortcutsManager::ContextInstance::setActivationParameters(QHash<QString, QVariant> &params)
{
    for (QString param : params.keys()) {
        m_activationParameters[param] = params[param];
    }
}

bool overlapping(GestureDirection d, GestureDirection d1)
{
    if (d == d1) {
        return true;
    }

    if (PINCH_DIRECTIONS.contains(d) && PINCH_DIRECTIONS.contains(d1)) {
        if (d == GestureDirection::BiDirectionalPinch || d1 == GestureDirection::BiDirectionalPinch) {
            return true;
        }
    } else if (SWIPE_DIRECTIONS.contains(d) && SWIPE_DIRECTIONS.contains(d1)) {
        if (d == GestureDirection::DirectionlessSwipe || d1 == GestureDirection::DirectionlessSwipe) {
            return true;
        }

        switch (d) {
        case GestureDirection::VerticalAxis:
            if (d1 == GestureDirection::Up || d1 == GestureDirection::Down) {
                return true;
            }
            break;
        case GestureDirection::HorizontalAxis:
            if (d1 == GestureDirection::Left || d1 == GestureDirection::Right) {
                return true;
            }
            break;
        default:
            Q_UNREACHABLE();
        }

        switch (d1) {
        case GestureDirection::VerticalAxis:
            if (d == GestureDirection::Up || d == GestureDirection::Down) {
                return true;
            }
            break;
        case GestureDirection::HorizontalAxis:
            if (d == GestureDirection::Left || d == GestureDirection::Right) {
                return true;
            }
            break;
        default:
            Q_UNREACHABLE();
        }
    }
    return false;
}

void GlobalShortcutsManager::setContext(const QString context)
{
    if (!m_contexts.contains(context)) {
        return;
    }
    ContextInstance::switchTo(m_contexts.value(context));
}

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, int keyQt)
{
    if (m_kglobalAccelInterface) {
        if (!keyQt && !mods) {
            return false;
        }
        auto check = [this](Qt::KeyboardModifiers mods, int keyQt) {
            bool retVal = false;
            QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                      "checkKeyPressed",
                                      Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, retVal),
                                      Q_ARG(int, int(mods) | keyQt));
            return retVal;
        };
        if (check(mods, keyQt)) {
            return true;
        } else if (keyQt == Qt::Key_Backtab) {
            // KGlobalAccel on X11 has some workaround for Backtab
            // see kglobalaccel/src/runtime/plugins/xcb/kglobalccel_x11.cpp method x11KeyPress
            // Apparently KKeySequenceWidget captures Shift+Tab instead of Backtab
            // thus if the key is backtab we should adjust to add shift again and use tab
            // in addition KWin registers the shortcut incorrectly as Alt+Shift+Backtab
            // this should be changed to either Alt+Backtab or Alt+Shift+Tab to match KKeySequenceWidget
            // trying the variants
            if (check(mods | Qt::ShiftModifier, keyQt)) {
                return true;
            }
            if (check(mods | Qt::ShiftModifier, Qt::Key_Tab)) {
                return true;
            }
        }
    }
    return false;
}

bool GlobalShortcutsManager::processKeyRelease(Qt::KeyboardModifiers mods, int keyQt)
{
    if (m_kglobalAccelInterface) {
        QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                  "checkKeyReleased",
                                  Qt::DirectConnection,
                                  Q_ARG(int, int(mods) | keyQt));
    }
    return false;
}

template<typename ShortcutKind, typename... Args>
bool match(QVector<GlobalShortcut> &shortcuts, Args... args)
{
    for (auto &sc : shortcuts) {
        if (std::holds_alternative<ShortcutKind>(sc.shortcut())) {
            if (std::get<ShortcutKind>(sc.shortcut()) == ShortcutKind{args...}) {
                sc.invoke();
                return true;
            }
        }
    }
    return false;
}

// TODO(C++20): use ranges for a nicer way of filtering by shortcut type
bool GlobalShortcutsManager::processPointerPressed(Qt::KeyboardModifiers mods, Qt::MouseButtons pointerButtons)
{
    return match<PointerButtonShortcut>(m_shortcuts, mods, pointerButtons);
}

bool GlobalShortcutsManager::processAxis(Qt::KeyboardModifiers mods, PointerAxisDirection axis)
{
    return match<PointerAxisShortcut>(m_shortcuts, mods, axis);
}

void GlobalShortcutsManager::processSwipeStart(GestureDeviceType device, uint fingerCount)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->startSwipeGesture(fingerCount);
    } else {
        m_touchscreenGestureRecognizer->startSwipeGesture(fingerCount);
    }
}

void GlobalShortcutsManager::processSwipeUpdate(GestureDeviceType device, const QSizeF &delta)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->updateSwipeGesture(delta);
    } else {
        m_touchscreenGestureRecognizer->updateSwipeGesture(delta);
    }
}

void GlobalShortcutsManager::processSwipeCancel(GestureDeviceType device)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->cancelSwipeGesture();
    } else {
        m_touchscreenGestureRecognizer->cancelSwipeGesture();
    }
}

void GlobalShortcutsManager::processSwipeEnd(GestureDeviceType device)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->endSwipeGesture();
    } else {
        m_touchscreenGestureRecognizer->endSwipeGesture();
    }
    // TODO: cancel on Wayland Seat if one triggered
}

void GlobalShortcutsManager::processPinchStart(uint fingerCount)
{
    m_touchpadGestureRecognizer->startPinchGesture(fingerCount);
}

void GlobalShortcutsManager::processPinchUpdate(qreal scale, qreal angleDelta, const QSizeF &delta)
{
    m_touchpadGestureRecognizer->updatePinchGesture(scale, angleDelta, delta);
}

void GlobalShortcutsManager::processPinchCancel()
{
    m_touchpadGestureRecognizer->cancelPinchGesture();
}

void GlobalShortcutsManager::processPinchEnd()
{
    m_touchpadGestureRecognizer->endPinchGesture();
}

Gesture *GestureShortcut::gesture() const
{
    if (swipeGesture != nullptr) {
        return swipeGesture.get();
    } else {
        return pinchGesture.get();
    }
}

Action::Action(const QString humanReadableLabel, const QString actionName)
    : humanReadableLabel(humanReadableLabel)
    , name(actionName)
    , supportedGestureDirections(new QSet<GestureDirection>(ALL_DIRECTIONS))
{
}

static AnimationDirection animationDirection(GestureDirection d, qreal progress = 0, QSizeF delta = QSizeF(0, 0))
{
    //TODO: Make configuration option for this
    // Fingers moving up, stuff on screen goes up, new contexnt comes from the bottom
    // It's like there's a piece of paper under your fingers
    const bool natural = true;

    typedef AnimationDirection Move;

    switch (d) {
    case GestureDirection::Up:
        return natural ? Move::Up : Move::Down;
    case GestureDirection::Down:
        return natural ? Move::Down : Move::Up;
    case GestureDirection::Left:
        return natural ? Move::Left : Move::Right;
    case GestureDirection::Right:
        return natural ? Move::Right : Move::Left;
    case GestureDirection::VerticalAxis:
        if (progress > 0) {
            return natural ? Move::Up : Move::Down;
        } else {
            return natural ? Move::Down : Move::Up;
        }
    case GestureDirection::HorizontalAxis:
        if (progress > 0) {
            return natural ? Move::Right : Move::Left;
        } else {
            return natural ? Move::Left : Move::Right;
        }
    case GestureDirection::DirectionlessSwipe:
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
    case GestureDirection::Contracting:
        return Move::Contracting;
    case GestureDirection::Expanding:
        return Move::Expanding;
    case GestureDirection::BiDirectionalPinch:
        if (progress > 0) {
            return Move::Expanding;
        } else {
            return Move::Contracting;
        }
    }
    return Move::None;
}

GlobalShortcutsManager::ContextInstance::Binding::Binding(GestureShortcut *gesture, const QString slotName)
    : actionName(slotName)
    , gesture(gesture)
{
}

GlobalShortcutsManager::ContextInstance::Binding::Binding(GestureShortcut *gesture, std::shared_ptr<ContextInstance> to)
    : gesture(std::unique_ptr<GestureShortcut>(gesture))
    , to(to)
{
}

GlobalShortcutsManager::ContextInstance::Binding::Binding(QList<int> *touchBordersRef, const QString contextName, const QHash<QString, QVariant> parameters)
    : targetContextName(contextName)
    , activationParameters(parameters)
    , touchBordersRef(touchBordersRef)
    , m_touchscreenOnUpAction(std::make_unique<QAction>())
{
}

GlobalShortcutsManager::ContextInstance::Binding::~Binding()
{
    deactivate();
}

void GlobalShortcutsManager::ContextInstance::Binding::activate()
{
    deactivate(); // Make sure we don't double connect anything

    switch (type()) {
    case Type::Gesture_Action:
        action = s_manager->m_actions.value(actionName);

        connect(gesture->gesture(), &Gesture::triggered, action, &Action::triggeredSlot, Qt::QueuedConnection);
        connect(gesture->gesture(), &Gesture::cancelled, action, &Action::cancelledSlot, Qt::QueuedConnection);

        connect(gesture->gesture(), &Gesture::semanticProgress, action, &Action::semanticProgressSlot);
        connect(gesture->gesture(), &Gesture::semanticProgressAxis, action, &Action::semanticAxisSlot);
        if (gesture->swipeGesture != nullptr) {
            connect(gesture->swipeGesture.get(), &SwipeGesture::semanticDelta, action, &Action::semanticDeltaSlot);
            connect(gesture->swipeGesture.get(), &SwipeGesture::pixelDelta, action, &Action::pixelDeltaSlot);
        }

        // Protect against pointer invalidation
        //connect(action, &Action::destroyed, [this]() {
        //deactivate();
        //});

        return;
    case Type::Gesture_ContextInstanceSwitcher:
        if (!to->isActivatable()) {
            return;
        }

        // Forward the gesture updates
        connect(gesture->gesture(), &Gesture::semanticProgress, [this](qreal progress, GestureDirection d) {
            if (activeBinding == this || activeBinding == nullptr) {
                activeBinding = this;

                // Stop the timer so the animation can follow the gesture
                s_animator.stop();

                s_switchingTo = to;
                s_gestureProgress = progress;
                s_switchDirection = animationDirection(d, progress);

                s_current->deactivating(totalTransitionProgress(), s_switchDirection);
                to->activating(totalTransitionProgress(), s_switchDirection);
            }
        });
        connect(gesture->gesture(), &Gesture::triggered, [this]() {
            if (activeBinding == this) {
                activeBinding = nullptr;
                switchTo(to);
            }
        });
        connect(gesture->gesture(), &Gesture::cancelled, [this]() {
            if (activeBinding == this) {
                activeBinding = nullptr;
                switchTo(s_current);
            }
        });

        // Protect against pointer invalidation
        //connect(to->baseContext, &Context::destroyed, [this]() {
        //    deactivate();
        //});

        return;
    case Type::TouchscreenBorder_ContextSwitcher:
        if (!s_manager->m_contexts.contains(targetContextName)) {
            return;
        }

        to = getClosestInstanceOf(targetContextName, activationParameters);

        if (!to->isActivatable()) {
            return;
        }

        connect(m_touchscreenOnUpAction.get(), &QAction::triggered, [this]() {
            totalTransitionProgress() > 0.5 ? switchTo(to) : switchTo(s_current);
        });

        auto touchMotionCallback = [this](ElectricBorder, const QSizeF &deltaProgress, Output *) {
            if (to->baseContext->state() == Context::State::Active) {
                return;
            }
            qreal progress = std::hypot(deltaProgress.width(), deltaProgress.height()) / 500;

            s_switchingTo = to;
            s_gestureProgress = progress;
            s_switchDirection = AnimationDirection::None;
            //TODO: Interpret the delta given here to
            // a GestureDirection. Then use that to get
            // an AnimationDirection that conforms to whatever
            // settings we have for scroll direction in the future

            // Stop the timer so the animation can follow the gesture
            s_animator.stop();

            s_current->deactivating(progress, s_switchDirection);
            to->activating(progress, s_switchDirection);
        };

        m_currentlyBoundBorders.clear();
        for (int i : qAsConst(*touchBordersRef)) {
            m_currentlyBoundBorders.push_back(ElectricBorder(i));
        }
        for (ElectricBorder b : qAsConst(m_currentlyBoundBorders)) {
            ScreenEdges::self()->reserveTouch(b, m_touchscreenOnUpAction.get(), touchMotionCallback);
        }

        // Protect against pointer invalidation
        //connect(to->baseContext, &Context::destroyed, [this]() {
        //deactivate();
        //});

        return;
    }
}

void GlobalShortcutsManager::ContextInstance::Binding::deactivate()
{
    switch (type()) {
    case Gesture_Action:
    case Gesture_ContextInstanceSwitcher:
        disconnect(gesture->gesture(), nullptr, nullptr, nullptr);
        break;
    case TouchscreenBorder_ContextSwitcher:
        for (ElectricBorder b : qAsConst(m_currentlyBoundBorders)) {
            ScreenEdges::self()->unreserveTouch(b, m_touchscreenOnUpAction.get());
        }
        disconnect(m_touchscreenOnUpAction.get(), nullptr, nullptr, nullptr);
        break;
    }

    if (activeBinding == this) {
        activeBinding = nullptr;
    }
}

GlobalShortcutsManager::ContextInstance::Binding::Type GlobalShortcutsManager::ContextInstance::Binding::type() const
{
    if (to || !targetContextName.isEmpty()) {
        if (gesture) {
            return Type::Gesture_ContextInstanceSwitcher;
        } else {
            return Type::TouchscreenBorder_ContextSwitcher;
        }
    } else if (!actionName.isEmpty()) {
        if (gesture) {
            return Type::Gesture_Action;
        }
    }
    // This should be impossible
    Q_ASSERT(false);
    return Type::Gesture_ContextInstanceSwitcher;
}

void GlobalShortcutsManager::forceRegisterTouchscreenSwipe(QAction *action, std::function<void(qreal)> progressCallback, GestureDirection direction, uint fingerCount)
{
    SwipeGesture *gesture = new SwipeGesture(action);
    gesture->addFingerCount(fingerCount);
    gesture->setDirection(direction);
    m_touchscreenGestureRecognizer->registerSwipeGesture(gesture);

    connect(gesture, &SwipeGesture::triggered, [action]() {
        action->trigger();
    });
    connect(gesture, &SwipeGesture::semanticProgress, progressCallback);
}

bool GlobalShortcutsManager::ContextInstance::gestureAvailable(GestureShortcut *gesture) const
{
    // Check all the gesture bindings in the context
    for (Binding *b : qAsConst(m_bindings)) {
        if (overlapping(b->gesture.get()->direction, gesture->direction) // Matches the direction
            && b->gesture->gesture()->acceptableFingerCounts().intersects(gesture->gesture()->acceptableFingerCounts()) // Matches finger count
            && b->gesture.get()->device == gesture->device) { // Matches device
            return false;
        }
    }
    return true;
}

QString GlobalShortcutsManager::currentContext() const
{
    if (!ContextInstance::s_current) {
        return "";
    }
    return ContextInstance::s_current->baseContextName;
}

void GlobalShortcutsManager::configChanged()
{
    configReader->read();
    ContextInstance::refreshBindings();
}

uint GlobalShortcutsManager::contextSwitchAnimationDuration() const
{
    return CONTEXT_SWITCH_ANIMATION_DURATION;
}

QString GestureShortcut::toString() const
{
    QString str = "";
    str.append(string(direction) + " ");

    switch (device) {
    case GestureDeviceType::Touchpad:
        str.append("Touchpad ");
        break;
    case GestureDeviceType::Touchscreen:
        str.append("Touchscreen ");
        break;
    }

    return str;
}

Context::Context(const QString label, const QString name)
    : humanReadableLabel(label)
    , name(name)
    , activationParameters(new QList<Parameter>())
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
}

void GlobalShortcutsManager::ContextInstance::switchTo(std::shared_ptr<ContextInstance> to)
{
    if (s_transitionMustComplete) {
        return;
    }
    if (!to->isActivatable()) {
        std::cout << "Trying to Switch To Invalid Context" << std::endl;
        return;
    }
    s_animationProgress += s_gestureProgress;
    s_gestureProgress = 0;

    // This is complicated because there is no "transitioning" state.
    // We can switchTo() anywhere at any time.
    if (s_switchingTo) {
        if (to == s_current) {
            if (s_switchingTo == s_current) {
                return;
            } else if (s_switchingTo != s_current) {
                s_switchingTo->deactivateWithAnimation(totalTransitionProgress());
                s_animationProgress = std::clamp(1 - s_animationProgress, 0.0, 1.0);
            }
        } else if (to != s_current) {
            if (s_switchingTo != s_current) {
                s_switchingTo->deactivateWithAnimation(totalTransitionProgress());
            } else if (s_switchingTo == s_current) {
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

void GlobalShortcutsManager::ContextInstance::switchTo(Context *context)
{
    switchTo(getClosestInstanceOf(context->name));
}

void GlobalShortcutsManager::ContextInstance::initActivate()
{
    s_current = shared_from_this();
    handleSwitchComplete(shared_from_this());
}

void GlobalShortcutsManager::ContextInstance::handleSwitchComplete(std::shared_ptr<ContextInstance> to)
{
    Q_ASSERT(to->baseContext);
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
    for (Binding *b : qAsConst(s_current->m_bindings)) {
        b->deactivate();
    }
    s_current->deactivated();

    // Handle new context
    for (Binding *b : qAsConst(to->m_bindings)) {
        b->activate();
    }
    to->activated();

    s_current = to->shared_from_this();
    Q_EMIT s_manager->contextChanged(s_current->baseContextName);
}

void GlobalShortcutsManager::ContextInstance::deactivateWithAnimation(qreal activationProgress)
{
    QVariantAnimation *fadeout = new QVariantAnimation();
    fadeout->setStartValue(1 - activationProgress);
    fadeout->setEndValue(1.0);
    fadeout->setDuration(activationProgress * CONTEXT_SWITCH_ANIMATION_DURATION);
    fadeout->setEasingCurve(QEasingCurve::InOutCubic);

    connect(fadeout, &QVariantAnimation::valueChanged, [this, fadeout](const QVariant &value) {
        // Cancel if this context is being activated
        if (s_switchingTo.get() == this) {
            fadeout->stop();
            disconnect(fadeout, nullptr, nullptr, nullptr);
            delete fadeout;
            return;
        }
        deactivating(value.toDouble(), s_switchDirection);
    });
    connect(fadeout, &QVariantAnimation::finished, [this, fadeout]() {
        disconnect(fadeout, nullptr, nullptr, nullptr);
        delete fadeout;
        deactivated();
    });
    fadeout->start();
}

std::shared_ptr<GlobalShortcutsManager::ContextInstance> GlobalShortcutsManager::ContextInstance::getClosestInstanceOf(const QString context, QHash<QString, QVariant> parameters)
{
    Q_ASSERT(s_manager->m_contexts.contains(context));

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

void GlobalShortcutsManager::ContextInstance::activated()
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_switchingTo.get() != this) {
        return;
    }

    if (baseContext->state() == Context::State::Inactive) {
        Q_EMIT baseContext->setActivationParameters(m_activationParameters);
    }

    Q_EMIT baseContext->activated();
}

void GlobalShortcutsManager::ContextInstance::activating(qreal progress, AnimationDirection direction)
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_switchingTo.get() != this) {
        return;
    }

    if (baseContext->state() == Context::State::Inactive) {
        Q_EMIT baseContext->setActivationParameters(m_activationParameters);
    }

    Q_EMIT baseContext->activating(progress, direction);
}

void GlobalShortcutsManager::ContextInstance::deactivating(qreal progress, AnimationDirection direction)
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_current.get() != this) {
        return;
    }

    Q_EMIT baseContext->deactivating(progress, direction);
}

void GlobalShortcutsManager::ContextInstance::deactivated()
{
    if (!isActivatable()) {
        return;
    }
    if (s_transitionMustComplete && s_current.get() != this) {
        return;
    }

    Q_EMIT baseContext->deactivated();
}

bool GlobalShortcutsManager::ContextInstance::isActivatable() const
{
    return baseContext != nullptr;
}

void GlobalShortcutsManager::ContextInstance::informContextsOfRegisteredContext(Context *context)
{
    for (ContextInstance *instance : qAsConst(s_contextInstances)) {
        instance->tryAdd(context);
    }
    refreshBindings();
}

void GlobalShortcutsManager::ContextInstance::informContextsOfUnregisteredContext(Context *context)
{
    // Remove refrences to this context from ContextInstances
    for (ContextInstance *instance : qAsConst(s_contextInstances)) {
        if (instance->baseContextName == context->name) {
            instance->baseContext = nullptr;
        }
    }

    if (s_current->baseContextName == context->name) {
        s_manager->m_rootContextInstance->initActivate();
    }
    if (s_switchingTo) {
        if (s_switchingTo->baseContextName == context->name) {
            s_manager->m_rootContextInstance->initActivate();
        }
    }

    refreshBindings();
}

void GlobalShortcutsManager::ContextInstance::tryAdd(Context *context)
{
    Q_ASSERT(context);
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
void GlobalShortcutsManager::ContextInstance::refreshBindings()
{
    for (Binding *b : qAsConst(s_current->m_bindings)) {
        b->activate();
    }
    for (Binding *b : qAsConst(s_globalBindings)) {
        b->activate();
    }
}

GlobalShortcutsManager::ContextInstance::ContextInstance(const QString baseContextName, const QString name, QHash<QString, QVariant> parameters)
    : baseContextName(baseContextName)
    , name(name)
    , m_bindings(QSet<Binding *>())
    , baseContext(s_manager->m_contexts.value(baseContextName))
    , m_activationParameters(parameters)
{
    s_contextInstances.append(this);

    if (s_manager->m_contexts.contains(baseContextName)) {
        tryAdd(s_manager->m_contexts[baseContextName]);
        //refreshBindings();
    }
}

GlobalShortcutsManager::ContextInstance::~ContextInstance()
{
    s_contextInstances.removeAll(this);
    qDeleteAll(m_bindings);
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

void Action::semanticProgressSlot(qreal progress, GestureDirection d)
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

void Action::semanticAxisSlot(qreal progress, GestureDirection d)
{
    Q_EMIT semanticAxisUpdate(progress, d);
}

void Action::semanticDeltaSlot(const QSizeF progress, GestureDirection d)
{
    Q_EMIT semanticDeltaUpdate(progress, d);
}

void Action::pixelDeltaSlot(const QSizeF progress, GestureDirection d)
{
    Q_EMIT pixelDeltaUpdate(progress, d);
}
} // namespace
