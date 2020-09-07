/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_INPUT_H
#define KWIN_INPUT_H
#include <kwinglobals.h>
#include <QAction>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <config-kwin.h>

#include <KSharedConfig>
#include <QSet>

#include <functional>

class KGlobalAccelInterface;
class QKeySequence;
class QMouseEvent;
class QKeyEvent;
class QWheelEvent;

namespace KWin
{
class GlobalShortcutsManager;
class Toplevel;
class InputEventFilter;
class InputEventSpy;
class KeyboardInputRedirection;
class PointerInputRedirection;
class TabletInputRedirection;
class TouchInputRedirection;
class WindowSelectorFilter;
class SwitchEvent;
class TabletEvent;
class TabletInputFilter;

namespace Decoration
{
class DecoratedClientImpl;
}

namespace LibInput
{
    class Connection;
    class Device;
}

/**
 * @brief This class is responsible for redirecting incoming input to the surface which currently
 * has input or send enter/leave events.
 *
 * In addition input is intercepted before passed to the surfaces to have KWin internal areas
 * getting input first (e.g. screen edges) and filter the input event out if we currently have
 * a full input grab.
 */
class KWIN_EXPORT InputRedirection : public QObject
{
    Q_OBJECT
public:
    enum PointerButtonState {
        PointerButtonReleased,
        PointerButtonPressed
    };
    enum PointerAxis {
        PointerAxisVertical,
        PointerAxisHorizontal
    };
    enum PointerAxisSource {
        PointerAxisSourceUnknown,
        PointerAxisSourceWheel,
        PointerAxisSourceFinger,
        PointerAxisSourceContinuous,
        PointerAxisSourceWheelTilt
    };
    enum KeyboardKeyState {
        KeyboardKeyReleased,
        KeyboardKeyPressed,
        KeyboardKeyAutoRepeat
    };
    enum TabletEventType {
        Axis,
        Proximity,
        Tip
    };
    enum TabletToolType {
        Pen,
        Eraser,
        Brush,
        Pencil,
        Airbrush,
        Finger,
        Mouse,
        Lens,
        Totem,
    };
    enum Capability {
        Tilt,
        Pressure,
        Distance,
        Rotation,
        Slider,
        Wheel,
    };

    ~InputRedirection() override;
    void init();

    /**
     * @return const QPointF& The current global pointer position
     */
    QPointF globalPointer() const;
    Qt::MouseButtons qtButtonStates() const;
    Qt::KeyboardModifiers keyboardModifiers() const;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const;

    void registerShortcut(const QKeySequence &shortcut, QAction *action);
    /**
     * @overload
     *
     * Like registerShortcut, but also connects QAction::triggered to the @p slot on @p receiver.
     * It's recommended to use this method as it ensures that the X11 timestamp is updated prior
     * to the @p slot being invoked. If not using this overload it's required to ensure that
     * registerShortcut is called before connecting to QAction's triggered signal.
     */
    template <typename T, typename Slot>
    void registerShortcut(const QKeySequence &shortcut, QAction *action, T *receiver, Slot slot);
    void registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action);
    void registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action);
    void registerTouchpadSwipeShortcut(SwipeDirection direction, QAction *action);
    void registerGlobalAccel(KGlobalAccelInterface *interface);

    /**
     * @internal
     */
    void processPointerMotion(const QPointF &pos, uint32_t time);
    /**
     * @internal
     */
    void processPointerButton(uint32_t button, PointerButtonState state, uint32_t time);
    /**
     * @internal
     */
    void processPointerAxis(PointerAxis axis, qreal delta, qint32 discreteDelta, PointerAxisSource source, uint32_t time);
    /**
     * @internal
     */
    void processKeyboardKey(uint32_t key, KeyboardKeyState state, uint32_t time);
    /**
     * @internal
     */
    void processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    /**
     * @internal
     */
    void processKeymapChange(int fd, uint32_t size);
    void processTouchDown(qint32 id, const QPointF &pos, quint32 time);
    void processTouchUp(qint32 id, quint32 time);
    void processTouchMotion(qint32 id, const QPointF &pos, quint32 time);
    void cancelTouch();
    void touchFrame();

    bool supportsPointerWarping() const;
    void warpPointer(const QPointF &pos);

    /**
     * Adds the @p filter to the list of event filters and makes it the first
     * event filter in processing.
     *
     * Note: the event filter will get events before the lock screen can get them, thus
     * this is a security relevant method.
     */
    void prependInputEventFilter(InputEventFilter *filter);
    void uninstallInputEventFilter(InputEventFilter *filter);

    /**
     * Installs the @p spy for spying on events.
     */
    void installInputEventSpy(InputEventSpy *spy);

    /**
     * Uninstalls the @p spy. This happens automatically when deleting an InputEventSpy.
     */
    void uninstallInputEventSpy(InputEventSpy *spy);

    Toplevel *findToplevel(const QPoint &pos);
    Toplevel *findManagedToplevel(const QPoint &pos);
    GlobalShortcutsManager *shortcuts() const {
        return m_shortcuts;
    }

    /**
     * Sends an event through all InputFilters.
     * The method @p function is invoked on each input filter. Processing is stopped if
     * a filter returns @c true for @p function.
     *
     * The UnaryPredicate is defined like the UnaryPredicate of std::any_of.
     * The signature of the function should be equivalent to the following:
     * @code
     * bool function(const InputEventFilter *spy);
     * @endcode
     *
     * The intended usage is to std::bind the method to invoke on the filter with all arguments
     * bind.
     */
    template <class UnaryPredicate>
    void processFilters(UnaryPredicate function) {
        std::any_of(m_filters.constBegin(), m_filters.constEnd(), function);
    }

    /**
     * Sends an event through all input event spies.
     * The @p function is invoked on each InputEventSpy.
     *
     * The UnaryFunction is defined like the UnaryFunction of std::for_each.
     * The signature of the function should be equivalent to the following:
     * @code
     * void function(const InputEventSpy *spy);
     * @endcode
     *
     * The intended usage is to std::bind the method to invoke on the spies with all arguments
     * bind.
     */
    template <class UnaryFunction>
    void processSpies(UnaryFunction function) {
        std::for_each(m_spies.constBegin(), m_spies.constEnd(), function);
    }

    KeyboardInputRedirection *keyboard() const {
        return m_keyboard;
    }
    PointerInputRedirection *pointer() const {
        return m_pointer;
    }
    TabletInputRedirection *tablet() const {
        return m_tablet;
    }
    TouchInputRedirection *touch() const {
        return m_touch;
    }

    bool hasAlphaNumericKeyboard();
    bool hasTabletModeSwitch();

    void startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName);
    void startInteractivePositionSelection(std::function<void(const QPoint &)> callback);
    bool isSelectingWindow() const;

Q_SIGNALS:
    /**
     * @brief Emitted when the global pointer position changed
     *
     * @param pos The new global pointer position.
     */
    void globalPointerChanged(const QPointF &pos);
    /**
     * @brief Emitted when the state of a pointer button changed.
     *
     * @param button The button which changed
     * @param state The new button state
     */
    void pointerButtonStateChanged(uint32_t button, InputRedirection::PointerButtonState state);
    /**
     * @brief Emitted when a pointer axis changed
     *
     * @param axis The axis on which the even occurred
     * @param delta The delta of the event.
     */
    void pointerAxisChanged(InputRedirection::PointerAxis axis, qreal delta);
    /**
     * @brief Emitted when the modifiers changes.
     *
     * Only emitted for the mask which is provided by Qt::KeyboardModifiers, if other modifiers
     * change signal is not emitted
     *
     * @param newMods The new modifiers state
     * @param oldMods The previous modifiers state
     */
    void keyboardModifiersChanged(Qt::KeyboardModifiers newMods, Qt::KeyboardModifiers oldMods);
    /**
     * @brief Emitted when the state of a key changed.
     *
     * @param keyCode The keycode of the key which changed
     * @param state The new key state
     */
    void keyStateChanged(quint32 keyCode, InputRedirection::KeyboardKeyState state);

    void hasAlphaNumericKeyboardChanged(bool set);
    void hasTabletModeSwitchChanged(bool set);

private:
    void setupLibInput();
    void setupTouchpadShortcuts();
    void setupLibInputWithScreens();
    void setupWorkspace();
    void reconfigure();
    void setupInputFilters();
    void installInputEventFilter(InputEventFilter *filter);
    KeyboardInputRedirection *m_keyboard;
    PointerInputRedirection *m_pointer;
    TabletInputRedirection *m_tablet;
    TouchInputRedirection *m_touch;
    TabletInputFilter *m_tabletSupport = nullptr;

    GlobalShortcutsManager *m_shortcuts;

    LibInput::Connection *m_libInput = nullptr;

    WindowSelectorFilter *m_windowSelector = nullptr;

    QVector<InputEventFilter*> m_filters;
    QVector<InputEventSpy*> m_spies;

    KWIN_SINGLETON(InputRedirection)
    friend InputRedirection *input();
    friend class DecorationEventFilter;
    friend class InternalWindowEventFilter;
    friend class ForwardInputFilter;
};

/**
 * Base class for filtering input events inside InputRedirection.
 *
 * The idea behind the InputEventFilter is to have task oriented
 * filters. E.g. there is one filter taking care of a locked screen,
 * one to take care of interacting with window decorations, etc.
 *
 * A concrete subclass can reimplement the virtual methods and decide
 * whether an event should be filtered out or not by returning either
 * @c true or @c false. E.g. the lock screen filter can easily ensure
 * that all events are filtered out.
 *
 * As soon as a filter returns @c true the processing is stopped. If
 * a filter returns @c false the next one is invoked. This means a filter
 * installed early gets to see more events than a filter installed later on.
 *
 * Deleting an instance of InputEventFilter automatically uninstalls it from
 * InputRedirection.
 */
class KWIN_EXPORT InputEventFilter
{
public:
    InputEventFilter();
    virtual ~InputEventFilter();

    /**
     * Event filter for pointer events which can be described by a QMouseEvent.
     *
     * Please note that the button translation in QMouseEvent cannot cover all
     * possible buttons. Because of that also the @p nativeButton code is passed
     * through the filter. For internal areas it's fine to use @p event, but for
     * passing to client windows the @p nativeButton should be used.
     *
     * @param event The event information about the move or button press/release
     * @param nativeButton The native key code of the button, for move events 0
     * @return @c true to stop further event processing, @c false to pass to next filter
     */
    virtual bool pointerEvent(QMouseEvent *event, quint32 nativeButton);
    /**
     * Event filter for pointer axis events.
     *
     * @param event The event information about the axis event
     * @return @c true to stop further event processing, @c false to pass to next filter
     */
    virtual bool wheelEvent(QWheelEvent *event);
    /**
     * Event filter for keyboard events.
     *
     * @param event The event information about the key event
     * @return @c tru to stop further event processing, @c false to pass to next filter.
     */
    virtual bool keyEvent(QKeyEvent *event);
    virtual bool touchDown(qint32 id, const QPointF &pos, quint32 time);
    virtual bool touchMotion(qint32 id, const QPointF &pos, quint32 time);
    virtual bool touchUp(qint32 id, quint32 time);

    virtual bool pinchGestureBegin(int fingerCount, quint32 time);
    virtual bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time);
    virtual bool pinchGestureEnd(quint32 time);
    virtual bool pinchGestureCancelled(quint32 time);

    virtual bool swipeGestureBegin(int fingerCount, quint32 time);
    virtual bool swipeGestureUpdate(const QSizeF &delta, quint32 time);
    virtual bool swipeGestureEnd(quint32 time);
    virtual bool swipeGestureCancelled(quint32 time);

    virtual bool switchEvent(SwitchEvent *event);

    virtual bool tabletToolEvent(TabletEvent *event);
    virtual bool tabletToolButtonEvent(const QSet<uint> &buttons);
    virtual bool tabletPadButtonEvent(const QSet<uint> &buttons);
    virtual bool tabletPadStripEvent(int number, int position, bool isFinger);
    virtual bool tabletPadRingEvent(int number, int position, bool isFinger);

protected:
    void passToWaylandServer(QKeyEvent *event);
};

class KWIN_EXPORT InputDeviceHandler : public QObject
{
    Q_OBJECT
public:
    ~InputDeviceHandler() override;
    virtual void init();

    void update();

    /**
     * @brief First Toplevel currently at the position of the input device
     * according to the stacking order.
     * @return Toplevel* at device position.
     *
     * This will be null if no toplevel is at the position
     */
    Toplevel *at() const;
    /**
     * @brief Toplevel currently having pointer input focus (this might
     * be different from the Toplevel at the position of the pointer).
     * @return Toplevel* with pointer focus.
     *
     * This will be null if no toplevel has focus
     */
    Toplevel *focus() const;

    /**
     * @brief The Decoration currently receiving events.
     * @return decoration with pointer focus.
     */
    QPointer<Decoration::DecoratedClientImpl> decoration() const {
        return m_focus.decoration;
    }
    /**
     * @brief The internal window currently receiving events.
     * @return QWindow with pointer focus.
     */
    QPointer<QWindow> internalWindow() const {
        return m_focus.internalWindow;
    }

    virtual QPointF position() const = 0;

    void setFocus(Toplevel *toplevel);
    void setDecoration(QPointer<Decoration::DecoratedClientImpl> decoration);
    void setInternalWindow(QWindow *window);

Q_SIGNALS:
    void atChanged(Toplevel *old, Toplevel *now);
    void decorationChanged();

protected:
    explicit InputDeviceHandler(InputRedirection *parent);

    virtual void cleanupInternalWindow(QWindow *old, QWindow *now) = 0;
    virtual void cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now) = 0;

    virtual void focusUpdate(Toplevel *old, Toplevel *now) = 0;

    /**
     * Certain input devices can be in a state of having no valid
     * position. An example are touch screens when no finger/pen
     * is resting on the surface (no touch point).
     */
    virtual bool positionValid() const {
        return false;
    }
    virtual bool focusUpdatesBlocked() {
        return false;
    }

    inline bool inited() const {
        return m_inited;
    }
    inline void setInited(bool set) {
        m_inited = set;
    }

private:
    bool setAt(Toplevel *toplevel);
    void updateFocus();
    bool updateDecoration();
    void updateInternalWindow(QWindow *window);

    QWindow* findInternalWindow(const QPoint &pos) const;

    struct {
        QPointer<Toplevel> at;
        QMetaObject::Connection surfaceCreatedConnection;
    } m_at;

    struct {
        QPointer<Toplevel> focus;
        QPointer<Decoration::DecoratedClientImpl> decoration;
        QPointer<QWindow> internalWindow;
    } m_focus;

    bool m_inited = false;
};

inline
InputRedirection *input()
{
    return InputRedirection::s_self;
}

template <typename T, typename Slot>
inline
void InputRedirection::registerShortcut(const QKeySequence &shortcut, QAction *action, T *receiver, Slot slot) {
    registerShortcut(shortcut, action);
    connect(action, &QAction::triggered, receiver, slot);
}

} // namespace KWin

Q_DECLARE_METATYPE(KWin::InputRedirection::KeyboardKeyState)
Q_DECLARE_METATYPE(KWin::InputRedirection::PointerButtonState)
Q_DECLARE_METATYPE(KWin::InputRedirection::PointerAxis)
Q_DECLARE_METATYPE(KWin::InputRedirection::PointerAxisSource)

#endif // KWIN_INPUT_H
