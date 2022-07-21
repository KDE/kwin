/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Eric Edlund <ericedlund@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// KWin
#include "globalshortcuts.h"
#include <kwinglobals.h>
// Qt
#include <QList>
#include <QObject>
#include <QSet>
// C++
#include <memory>

namespace KWin
{
const uint CONTEXT_SWITCH_ANIMATION_DURATION = 300;

class ContextInstance;
class Binding;
class GestureToAction;
class GestureToContextInstance;
class TouchborderToContext;
struct Parameter;

/**
 * Anything that wants to respond to gestures registers an Action.
 * Action contains no information about what gesture it's bound
 * to, allowing it to be configurable.
 *
 * Every slot mush have a unique persistent name that can be used for config.
 * Example: VirtualDesktopManager submits one of these used for switching the desktop.
 */
class Action : public QObject
{
    Q_OBJECT
public:
    Action(const QString label, const QString name);
    ~Action();
    const QString humanReadableLabel;
    const QString name;

Q_SIGNALS:
    void gestureReleased();
    void triggered();
    void cancelled();
    void crossTriggerThreshold(bool triggered);

    void semanticProgressUpdate(qreal, GestureDirections);
    void semanticAxisUpdate(qreal, GestureDirections);
    void semanticDeltaUpdate(const QSizeF, GestureDirections);
    void pixelDeltaUpdate(const QSizeF, GestureDirections);

    bool inProgress(bool);

public Q_SLOTS:
    // Used by GlobalShortcuts only; don't touch
    void cancelledSlot();
    void triggeredSlot();

    void semanticProgressSlot(qreal, GestureDirections);
    void semanticAxisSlot(qreal, GestureDirections);
    void semanticDeltaSlot(const QSizeF, GestureDirections);
    void pixelDeltaSlot(const QSizeF, GestureDirections);

private:
    bool m_pastTriggerThreshold = false;
};

/**
 * A place that the user can be.
 * Examples: Overview effect, desktop grid
 *
 * Effects that offer their own interface will provide one of these.
 * Only 1 Context can be State::Active at once
 */
class Context : public QObject
{
    Q_OBJECT
public:
    Context(const QString label, const QString name);
    ~Context();
    const QString humanReadableLabel;
    const QString name;

    enum State {
        Active,
        Activating,
        Deactivating,
        Inactive,
    };

    State state()
    {
        return m_state;
    }
    /**
     * Some Contexts take parameters.
     * @example WindowView takes the "Windows" parameter
     * and supplies the possibleValues:
     *    "Show All Windows" "ModeAllDesktops"
     *    "Show Windows from Current Desktop" "ModeCurrentDesktop"
     *    "Show All Windows of the Active Application" "ModeWindowClass"
     *
     * Contexts are guaranteed to recieve all their parameters
     * with setActivationParameters() before they are asked to
     * start activating.
     */
    std::shared_ptr<std::vector<Parameter>> activationParameters;

Q_SIGNALS:
    // Emitted by globalshortcuts; recieved by object holder:
    void activated(); // Emitted after the switch
    void activating(qreal progress, AnimationDirection); // [0, 1+] Semantic scale
    void deactivating(qreal progress, AnimationDirection); // [0, 1+] Semantic scale
    void deactivated(); // Emitted before a switch
    /**
     * Gives parameters in the same order as they are in parameters list.
     */
    //                                Parameter Name  Value
    void setActivationParameters(const QHash<QString, QVariant> &params); // Emit before a switch starts

    // Recieved by globalshortcuts; sent by object holder:
    void grabActive(bool mustComplete = false);
    void ungrabActive(bool mustComplete = false);

private:
    State m_state = State::Inactive;
};

/**
 * This singleton object manages the Context system.
 *
 * It can be used to track or set the current context.
 * It handles reading configuration for contexts internally.
 */
class ContextManager : public QObject
{
    Q_OBJECT
public:
    void init();
    uint contextSwitchAnimationDuration() const;

public Q_SLOTS:
    void reconfigure();

    void setContext(const QString context);
    QString currentContext() const;

Q_SIGNALS:
    /**
     * @param progress given in range [0, 1]
     */
    void contextChanging(const QString from, const QString to, qreal progress);
    void contextChanged(const QString currentContext);

private:
    void initLoadConfig();
    friend ContextInstance;
    friend Binding;
    friend GestureToAction;
    friend GestureToContextInstance;
    friend TouchborderToContext;

    // Called by Constructor/Destructor of object
    friend Action;
    void registerAction(Action *action);
    void unregisterAction(Action *action);
    friend Context;
    void registerContext(Context *context);
    void unregisterContext(Context *context);

    //    Name     Pointer
    QHash<QString, Context *> m_contexts;
    QHash<QString, std::shared_ptr<ContextInstance>> m_contextInstances; // Doesn't include temporary contexts
    QHash<QString, Action *> m_actions;

    /**
     * These bindings are always active regardless of context.
     */
    QSet<Binding *> globalBindings = QSet<Binding *>();

    std::shared_ptr<ContextInstance> m_rootContextInstance;

    KWIN_SINGLETON(ContextManager)
    friend ContextManager *contexts();

    std::unique_ptr<KCoreConfigSkeleton> configReader;
};

inline ContextManager *contexts()
{
    return ContextManager::self();
}

} // namespace
