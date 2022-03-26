/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_GLOBALSHORTCUTS_H
#define KWIN_GLOBALSHORTCUTS_H
// KWin
#include "gestures.h"
#include "screenedge.h"
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QKeySequence>
#include <memory>
#include <QSharedPointer>
#include <QTimer>

#include <KCoreConfigSkeleton>
#include <iostream>

class QAction;
class KGlobalAccelD;
class KGlobalAccelInterface;

namespace KWin
{
class GlobalShortcut;
class SwipeGesture;
class PinchGesture;
class GestureRecognizer;
class Action;
class GlobalShortcutsManager;

const uint CONTEXT_SWITCH_ANIMATION_DURATION = 300;

struct GestureShortcut
{
    GestureDeviceType device;
    GestureDirection direction;

    Gesture *gesture() const;
    QSharedPointer<SwipeGesture> swipeGesture = nullptr;
    QSharedPointer<PinchGesture> pinchGesture = nullptr;

    QString toString() const;
};

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
    const QString humanReadableLabel;
    const QString name;

    std::shared_ptr<QSet<GestureDirection>> supportedGestureDirections;

Q_SIGNALS:
    void gestureReleased();
    void triggered();
    void cancelled();
    void crossTriggerThreshold(bool triggered);

    void semanticProgressUpdate(qreal, GestureDirection);
    void semanticAxisUpdate(qreal, GestureDirection);
    void semanticDeltaUpdate(const QSizeF, GestureDirection);
    void pixelDeltaUpdate(const QSizeF, GestureDirection);

    bool inProgress(bool);

public Q_SLOTS:
    // Used by GlobalShortcuts only; don't touch
    void cancelledSlot();
    void triggeredSlot();

    void semanticProgressSlot(qreal, GestureDirection);
    void semanticAxisSlot(qreal, GestureDirection);
    void semanticDeltaSlot(const QSizeF, GestureDirection);
    void pixelDeltaSlot(const QSizeF, GestureDirection);

private:
    bool m_pastTriggerThreshold = false;
};

/**
 * A place that the user can be.
 * Examples: Overview effect, desktop grid
 *
 * Effects that offer their own interface will provide one of these.
 */
class Context : public QObject
{
    Q_OBJECT
public:
    Context(const QString label, const QString name);
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
    std::shared_ptr<QList<Parameter>> activationParameters;

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
 * @brief Manager for the global shortcut system inside KWin.
 *
 * This class is responsible for holding all the global shortcuts and to process a key press event.
 * That is trigger a shortcut if there is a match.
 *
 * For internal shortcut handling (those which are delivered inside KWin) QActions are used and
 * triggered if the shortcut matches. For external shortcut handling a DBus interface is used.
 */
class GlobalShortcutsManager : public QObject
{
    Q_OBJECT
public:
    explicit GlobalShortcutsManager(QObject *parent = nullptr);
    ~GlobalShortcutsManager() override;
    void init();

    /**
     * @brief Registers an internal global pointer shortcut
     *
     * @param action The action to trigger if the shortcut is pressed
     * @param modifiers The modifiers which need to be hold to trigger the action
     * @param pointerButtons The pointer button which needs to be pressed
     */
    void registerPointerShortcut(QAction *action, Qt::KeyboardModifiers modifiers, Qt::MouseButtons pointerButtons);
    /**
     * @brief Registers an internal global axis shortcut
     *
     * @param action The action to trigger if the shortcut is triggered
     * @param modifiers The modifiers which need to be hold to trigger the action
     * @param axis The pointer axis
     */
    void registerAxisShortcut(QAction *action, Qt::KeyboardModifiers modifiers, PointerAxisDirection axis);

    void forceRegisterTouchscreenSwipe(QAction *action, std::function<void(qreal)> progressCallback, GestureDirection direction, uint fingerCount);

    void registerAction(Action *action);
    void unregisterAction(Action *action);
    void registerContext(Context *context);
    void unregisterContext(Context *context); // Never call, Qt will do it.

    void setContext(const QString contextName);
    QString currentContext() const;

    void configChanged();

    uint contextSwitchAnimationDuration() const;

    /**
     * @brief Processes a key event to decide whether a shortcut needs to be triggered.
     *
     * If a shortcut triggered this method returns @c true to indicate to the caller that the event
     * should not be further processed. If there is no shortcut which triggered for the key, then
     * @c false is returned.
     *
     * @param modifiers The current hold modifiers
     * @param keyQt The Qt::Key which got pressed
     * @return @c true if a shortcut triggered, @c false otherwise
     */
    bool processKey(Qt::KeyboardModifiers modifiers, int keyQt);
    bool processKeyRelease(Qt::KeyboardModifiers modifiers, int keyQt);
    bool processPointerPressed(Qt::KeyboardModifiers modifiers, Qt::MouseButtons pointerButtons);
    /**
     * @brief Processes a pointer axis event to decide whether a shortcut needs to be triggered.
     *
     * If a shortcut triggered this method returns @c true to indicate to the caller that the event
     * should not be further processed. If there is no shortcut which triggered for the key, then
     * @c false is returned.
     *
     * @param modifiers The current hold modifiers
     * @param axis The axis direction which has triggered this event
     * @return @c true if a shortcut triggered, @c false otherwise
     */
    bool processAxis(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis);

    void processSwipeStart(GestureDeviceType device, uint fingerCount);
    void processSwipeUpdate(GestureDeviceType device, const QSizeF &delta);
    void processSwipeCancel(GestureDeviceType device);
    void processSwipeEnd(GestureDeviceType device);

    void processPinchStart(uint fingerCount);
    void processPinchUpdate(qreal scale, qreal angleDelta, const QSizeF &delta);
    void processPinchCancel();
    void processPinchEnd();

    void setKGlobalAccelInterface(KGlobalAccelInterface *interface)
    {
        m_kglobalAccelInterface = interface;
    }

Q_SIGNALS:
    void contextChanged(const QString currentContext);

private:
    /**
     * A configurable instance of a Context.
     * This class is responsible for switching and running contexts.
     * ContextInstances are chained together with different gestures allowing the
     * user to navigate between Contexts and have different bindings
     * in each.
     * We can't guarantee a Context will be present, so we use
     * these for configuration and do calls on them instead of directly
     * on Contexts.
     */
    class ContextInstance : public std::enable_shared_from_this<ContextInstance>
    {
    public:
        /**
         * Create a configurable, bindable instance of the given @param baseContext.
         * @param name is for this ContextInstance. Used in config files.
         * @param parameters these are sent to the baseContext on activation.
         *
         * Temporary Context Instances will be deleted once they're off screen.
         */
        ContextInstance(const QString baseContext, const QString name = "Temporary Context", QHash<QString, QVariant> parameters = QHash<QString, QVariant>());
        void initActivate(); // No animation, immediately change the context to this.
        ~ContextInstance();

        /**
         * These bind operations are all just configurations. They can be done before or after the context/action they
         * refrence has been registered. All the parameters are guaranteed to exist.
         *
         * @param targetContext Is another context that will be switched to when the given binding details are triggered.
         * @param actionName is the name of the action that will be triggered by the given bindings details.
         */
        void bindGesture(std::shared_ptr<ContextInstance> targetContext, GestureDeviceType device, GestureDirection direction, uint fingerCount);
        void bindGesture(const QString actionName, GestureDeviceType device, GestureDirection direction, uint fingerCount);
        void setActivationParameters(QHash<QString, QVariant> &params);

        static void bindGlobalTouchBorder(const QString contextName, QList<int> *borders, const QHash<QString, QVariant> parameters);

        /**
         * Checks the gestures bound in this contextInstance to see if
         * @param gesture could be added without collision.
         */
        bool gestureAvailable(GestureShortcut *gesture) const;

        /**
         * Starts the Context transition animation from the current
         * active contextInstance to the given one.
         *
         * When the animation is done it triggers @see handleSwitchComplete()
         *
         * Will do nothing if target Context is null.
         */
        static void switchTo(std::shared_ptr<ContextInstance> to);
        static void switchTo(Context *context);
        static void informContextsOfRegisteredContext(Context *context);
        static void informContextsOfUnregisteredContext(Context *context);
        static void refreshBindings();

        const QString baseContextName; // Guaranteed to exist, used in config
        const QString name; // Name of Context, used in config

        static std::shared_ptr<ContextInstance> s_current;
        static std::shared_ptr<ContextInstance> s_switchingTo;
        static bool s_transitionMustComplete;

        static GlobalShortcutsManager *s_manager;

    private:
        /**
         * Bindings are activated() and deactivated() by their context.
         *
         * Bindings are not aware of outside activity and must be
         * activated() again after relevant changes like when
         * an Action or Context is destroyed or registered
         */
        struct Binding {
            Binding(GestureShortcut *gesture, const QString actionName);
            Binding(GestureShortcut *gesture, std::shared_ptr<ContextInstance> to);
            Binding(QList<int> *touchBordersRef, QString toContext, const QHash<QString, QVariant> parameters); // For touch borders
            ~Binding();

            // Activate can be called several times without deactivate.
            void activate();
            void deactivate();

            enum Type {
                TouchscreenBorder_ContextSwitcher,
                Gesture_ContextInstanceSwitcher,
                Gesture_Action,
            };
            Type type() const;

            // Prevent concurrency
            static Binding *activeBinding;

            Action *action = nullptr;
            const QString actionName;
            const QString targetContextName;
            QHash<QString, QVariant> activationParameters;
            std::unique_ptr<GestureShortcut> gesture;
            std::shared_ptr<ContextInstance> to = nullptr;
            const QList<int> *touchBordersRef; // It is possible to have a touchborder binding with no touch borders

        private:
            // Keep separately so that deactivate() doesn't
            // try to unbind a border it doesn't have
            QList<ElectricBorder> m_currentlyBoundBorders;
            std::unique_ptr<QAction> m_touchscreenOnUpAction;
        };

        QSet<Binding *> m_bindings;
        static QVector<ContextInstance *> s_contextInstances; // List of all contexts

        /**
         * These bindings are always active regardless of context.
         */
        static QSet<Binding *> s_globalBindings;

        /**
         * After the switching animation is complete and
         * user finishes any gestures, the actual context
         * switch happens.
         */
        static void handleSwitchComplete(std::shared_ptr<ContextInstance> to);
        /**
         * When a context is registered, every contextInstance is notified.
         */
        void tryAdd(Context *context);

        bool isActivatable() const; // Is the baseContext defined?
        void deactivateWithAnimation(qreal initialProgress);

        static AnimationDirection s_switchDirection;
        static qreal s_animationProgress;
        static qreal s_gestureProgress;
        static inline qreal totalTransitionProgress()
        {
            return std::clamp(s_animationProgress + s_gestureProgress, 0.0, 1.0);
        }

        static std::shared_ptr<ContextInstance> getClosestInstanceOf(const QString context, QHash<QString, QVariant> parameters = QHash<QString, QVariant>());
        // These passthrough functions guard against a null baseContext and enforce transition locking
        void activated();
        void activating(qreal, AnimationDirection);
        void deactivating(qreal, AnimationDirection);
        void deactivated();
        Context *baseContext;

        // Allow for 1 context to behave differently in different contexts
        //    Name     Value
        QHash<QString, QVariant> m_activationParameters;
    }; // class Context

    void objectDeleted(QObject *object);
    bool addIfNotExists(GlobalShortcut sc);
    void loadGestureConfig();

    QVector<GlobalShortcut> m_shortcuts;

    //    Name     Pointer
    QHash<QString, Context *> m_contexts;
    QHash<QString, Action *> m_actions;

    std::shared_ptr<ContextInstance> m_rootContextInstance;

    KGlobalAccelD *m_kglobalAccel = nullptr;
    KGlobalAccelInterface *m_kglobalAccelInterface = nullptr;
    QScopedPointer<GestureRecognizer> m_touchpadGestureRecognizer;
    QScopedPointer<GestureRecognizer> m_touchscreenGestureRecognizer;
};

struct KeyboardShortcut
{
    QKeySequence sequence;
    bool operator==(const KeyboardShortcut &rhs) const
    {
        return sequence == rhs.sequence;
    }
};
struct PointerButtonShortcut
{
    Qt::KeyboardModifiers pointerModifiers;
    Qt::MouseButtons pointerButtons;
    bool operator==(const PointerButtonShortcut &rhs) const
    {
        return pointerModifiers == rhs.pointerModifiers && pointerButtons == rhs.pointerButtons;
    }
};
struct PointerAxisShortcut
{
    Qt::KeyboardModifiers axisModifiers;
    PointerAxisDirection axisDirection;
    bool operator==(const PointerAxisShortcut &rhs) const
    {
        return axisModifiers == rhs.axisModifiers && axisDirection == rhs.axisDirection;
    }
};

using Shortcut = std::variant<KeyboardShortcut, PointerButtonShortcut, PointerAxisShortcut>;

class GlobalShortcut
{
public:
    GlobalShortcut(Shortcut &&shortcut, QAction *action);
    ~GlobalShortcut();

    void invoke() const;
    QAction *action() const;
    const Shortcut &shortcut() const;

private:
    Shortcut m_shortcut = {};
    QAction *m_action = nullptr;
};

} // namespace

#endif
