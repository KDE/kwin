/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Eric Edlund <ericedlund@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// KWin
#include "globalshortcuts.h"
#include "kwinglobals.h"
#include "screenedge.h"
// Qt
#include <QVariantAnimation>
// C++
#include <memory>
#include <set>

namespace KWin
{
class Context;
class Action;
class Binding;
class GestureToAction;
class GestureToContextInstance;
class TouchborderToContext;

/*
 * A configurable instance of a Context.
 *
 * One Context can have multiple ContextInstances with different parameters/gesture bindings.
 * ContextInstances are chained together with different gestures allowing the
 * user to navigate between Contexts.
 * We can't guarantee a Context will be present, so we use
 * these for configuration and do calls on them instead of directly
 * on Contexts.
 */
class ContextInstance : public QObject, public std::enable_shared_from_this<ContextInstance>
{
    Q_OBJECT

public:
    /**
     * Create a configurable instance of the given @param baseContext.
     * @param name is for this ContextInstance. Used in config files.
     * @param parameters these are sent to the baseContext on activation.
     *
     * If the user requests to go to a context that doesn't have an instance,
     * a temporary context instance will be created.
     * Temporary Context Instances will be deleted once fully deactivated and
     * are only kept around by shared_ptrs
     */
    ContextInstance(const QString baseContext, const QString name = "Temporary Context", QHash<QString, QVariant> parameters = QHash<QString, QVariant>());
    void initActivate(); // No animation, immediately change the context to this.
    ~ContextInstance();

    /**
     * This will create a Binding object and add it to the ContextInstance.
     *
     * These bind operations are all just configurations. They can be done before or after the context/action they
     * refrence has been registered.
     *
     * @param targetContext Another context that will be switched to when the created binding is triggered.
     * @param actionName is the name of the action that will be triggered by the given bindings details.
     */
    void bindGestureContextInstance(const QString targetContext, GestureDeviceType device, GestureDirection direction, uint fingerCount);
    void bindGestureAction(const QString actionName, GestureDeviceType device, GestureDirection direction, uint fingerCount);

    /**
     * Per ContextInstance binding of touch borders isn't yet supported.
     */
    static void bindGlobalTouchBorder(const QString contextName, QList<int> *borders, const QHash<QString, QVariant> parameters);

    void setActivationParameters(QHash<QString, QVariant> &params);

    static std::shared_ptr<ContextInstance> s_switchingTo;
    static std::shared_ptr<ContextInstance> s_switchingFrom;
    static std::shared_ptr<ContextInstance> s_current;
    static QVariantAnimation s_animator;
    static AnimationDirection s_switchDirection;
    static bool s_transitionMustComplete;
    static qreal s_animationProgress;
    static qreal s_gestureProgress;
    static qreal transitionProgress()
    {
        return std::clamp(s_animationProgress + s_gestureProgress, 0.0, 1.0);
    }

    /**
     * Starts the Context transition animation from the current
     * active contextInstance to the given one.
     *
     * When the animation is done it triggers @see handleSwitchComplete()
     */
    static void switchTo(std::shared_ptr<ContextInstance> to);
    static void switchTo(Context *context);

    static void informContextsOfRegisteredContext(Context *context);
    static void informContextsOfUnregisteredContext(Context *context);

    /**
     * If an Action is registered/unregistered or a ContextInstance is created
     * or destroyed, refresh all the bindings so they don't try to send signals
     * to objects that don't exist and/or are aware of object that do exist.
     */
    static void refreshBindings();

    const QString baseContextName;
    const QString name;

private:
    friend Binding;
    friend GestureToAction;
    friend GestureToContextInstance;
    friend TouchborderToContext;

    std::set<std::unique_ptr<Binding>> m_bindings;
    static std::set<ContextInstance *> s_contextInstances; // List of all contexts

    /**
     * After the user ends the gesture and the animation
     * is completed, the actual context
     * switch happens.
     */
    static void handleSwitchComplete(std::shared_ptr<ContextInstance> to);
    /**
     * When a context is registered, every contextInstance is notified.
     */
    void tryAdd(Context *context);

    virtual bool isActivatable() const; // Is the baseContext defined?
    void deactivateWithAnimation(qreal initialProgress);
    std::unique_ptr<QVariantAnimation> m_deactivationAnimation;
    std::shared_ptr<ContextInstance> m_deactivationPointer;

    /**
     * If we need to open a Context but don't have a specific ContextInstace to use (like keyboard shortcuts)
     * then search the instances we have or create a new temporary instance.
     */
    static std::shared_ptr<ContextInstance> getClosestInstanceOf(const QString context, QHash<QString, QVariant> parameters = QHash<QString, QVariant>());

    // These passthrough functions guard against a null baseContext and enforce forced transitions
    virtual void activated();
    void activating(qreal, AnimationDirection);
    void deactivating(qreal, AnimationDirection);
    void deactivated();
    Context *baseContext;

    // Allow for 1 context to behave differently in different contexts
    //    Name     Value
    QHash<QString, QVariant> m_activationParameters;
}; // class Context

/**
 * Bindings are activated() and deactivated() by their context.
 *
 * Bindings are not aware of outside activity and must be
 * activated() again after relevant changes like when
 * an Action or Context is destroyed or registered
 *
 * Bindings can be for Gestures or Touchscreen borders
 */
class Binding : public QObject
{
    Q_OBJECT
public:
    virtual ~Binding() = default;

    void activate();
    void deactivate();

protected:
    /**
     * activateImpl() is never called twice without a deactivateImpl()
     */
    virtual void activateImpl() = 0;
    virtual void deactivateImpl() = 0;

    // Prevent concurrency
    static Binding *s_activeBinding;
};

class GestureToAction : public Binding
{
    Q_OBJECT
public:
    GestureToAction(Gesture *gesture, GestureDeviceType device, const QString actionName, bool);
    ~GestureToAction() override;

    void activateImpl() override;
    void deactivateImpl() override;

    std::unique_ptr<Gesture> gesture;
    GestureDeviceType device;
    const QString actionName;

private:
    Action *m_action;
};

class GestureToContextInstance : public Binding
{
    Q_OBJECT
public:
    GestureToContextInstance(Gesture *gesture, GestureDeviceType device, const QString targetContextInstanceName);
    ~GestureToContextInstance() override;

    void activateImpl() override;
    void deactivateImpl() override;

    const QString targetContextInstanceName;
    std::unique_ptr<Gesture> gesture;
    GestureDeviceType device;

private:
    std::shared_ptr<ContextInstance> m_to;
};

class TouchborderToContext : public Binding
{
    Q_OBJECT
public:
    TouchborderToContext(QList<int> *touchBordersRef, const QString contextName, const QHash<QString, QVariant> parameters);
    ~TouchborderToContext() override;

    void activateImpl() override;
    void deactivateImpl() override;

public Q_SLOTS:
    void touchscreenBorderOnUp();

public:
    QHash<QString, QVariant> activationParameters;
    const QString targetContextName;
    const QList<int> *touchBordersRef; // It is possible to have a touchborder binding with no touch borders
private:
    std::shared_ptr<ContextInstance> m_to;
    QList<ElectricBorder> m_currentlyBoundBorders;
    std::unique_ptr<QAction> m_touchscreenBorderOnUpAction;
};

/**
 * This solves the chicken vs egg problem of having
 * a context instance be activated before any
 * contexts are registered.
 */
class RootContextInstance : public ContextInstance
{
public:
    RootContextInstance()
        : ContextInstance(DEFAULT_CONTEXT, "Root Context"){};

    void activated() override{};
    bool isActivatable() const override
    {
        return true;
    }
};

} // namespace
