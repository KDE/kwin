/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QMatrix4x4>
#include <QObject>
#include <QPoint>

struct wl_client;
struct wl_resource;

namespace KWaylandServer
{
class AbstractDataSource;
class AbstractDropHandler;
class DragAndDropIcon;
class DataDeviceInterface;
class Display;
class KeyboardInterface;
class PointerInterface;
class SeatInterfacePrivate;
class SurfaceInterface;
class TextInputV1Interface;
class TextInputV2Interface;
class TextInputV3Interface;
class TouchInterface;

/**
 * Describes the source types for axis events. This indicates to the
 * client how an axis event was physically generated; a client may
 * adjust the user interface accordingly. For example, scroll events
 * from a "finger" source may be in a smooth coordinate space with
 * kinetic scrolling whereas a "wheel" source may be in discrete steps
 * of a number of lines.
 *
 * The "continuous" axis source is a device generating events in a
 * continuous coordinate space, but using something other than a
 * finger. One example for this source is button-based scrolling where
 * the vertical motion of a device is converted to scroll events while
 * a button is held down.
 *
 * The "wheel tilt" axis source indicates that the actual device is a
 * wheel but the scroll event is not caused by a rotation but a
 * (usually sideways) tilt of the wheel.
 */
enum class PointerAxisSource {
    Unknown,
    Wheel,
    Finger,
    Continuous,
    WheelTilt,
};

/**
 * This enum type is used to describe the state of a pointer button. It
 * is equivalent to the @c wl_pointer.button_state enum.
 */
enum class PointerButtonState : quint32 {
    Released = 0,
    Pressed = 1,
};

/**
 * This enum type is used to describe the state of a keyboard key. It is
 * equivalent to the @c wl_keyboard.key_state enum.
 */
enum class KeyboardKeyState : quint32 {
    Released = 0,
    Pressed = 1,
};

/**
 * @brief Represents a Seat on the Wayland Display.
 *
 * A Seat is a set of input devices (e.g. Keyboard, Pointer and Touch) the client can connect
 * to. The server needs to announce which input devices are supported and passes dedicated input
 * focus to a SurfaceInterface. Only the focused surface receives input events.
 *
 * The SeatInterface internally handles enter and release events when setting a focused surface.
 * Also it handles input translation from global to the local coordination, removing the need from
 * the user of the API to track the focused surfaces and can just interact with this class.
 *
 * To create a SeatInterface use @link Display::createSeat @endlink. Then one can set up what is
 * supported. Last but not least create needs to be called.
 *
 * @code
 * SeatInterface *seat = display->createSeat();
 * // set up
 * seat->setName(QStringLiteral("seat0"));
 * seat->setHasPointer(true);
 * seat->setHasKeyboard(true);
 * seat->setHasTouch(false);
 * // now fully create
 * seat->create();
 * @endcode
 *
 * To forward input events one needs to set the focused surface, update time stamp and then
 * forward the actual events:
 *
 * @code
 * // example for pointer
 * seat->setTimestamp(100);
 * seat->notifyPointerEnter(surface, QPointF(350, 210), QPointF(100, 200)); // surface at it's global position
 * seat->notifyPointerFrame();
 * seat->setTimestamp(110);
 * seat->notifyPointerButton(Qt::LeftButton, PointerButtonState::Pressed);
 * seat->notifyPointerFrame();
 * seat->setTimestamp(120);
 * seat->notifyPointerButton(Qt::LeftButton, PointerButtonState::Released);
 * seat->notifyPointerFrame();
 * @endcode
 *
 * @see KeyboardInterface
 * @see PointerInterface
 * @see TouchInterface
 * @see SurfaceInterface
 */
class KWIN_EXPORT SeatInterface : public QObject
{
    Q_OBJECT
public:
    explicit SeatInterface(Display *display, QObject *parent = nullptr);
    virtual ~SeatInterface();

    Display *display() const;
    QString name() const;
    bool hasPointer() const;
    bool hasKeyboard() const;
    bool hasTouch() const;

    void setName(const QString &name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

    void setTimestamp(std::chrono::microseconds time);
    std::chrono::milliseconds timestamp() const;

    /**
     * @name Drag'n'Drop related methods
     */
    ///@{
    /**
     * @returns whether there is currently a drag'n'drop going on.
     * @see isDragPointer
     * @see isDragTouch
     * @see dragStarted
     * @see dragEnded
     */
    bool isDrag() const;
    /**
     * @returns whether the drag'n'drop is operated through the pointer device
     * @see isDrag
     * @see isDragTouch
     */
    bool isDragPointer() const;
    /**
     * @returns whether the drag'n'drop is operated through the touch device
     * @see isDrag
     * @see isDragPointer
     */
    bool isDragTouch() const;
    /**
     * @returns The transformation applied to go from global to local coordinates for drag motion events.
     * @see dragSurfaceTransformation
     */
    QMatrix4x4 dragSurfaceTransformation() const;
    /**
     * @returns The currently focused Surface for drag motion events.
     * @see dragSurfaceTransformation
     * @see dragSurfaceChanged
     */
    SurfaceInterface *dragSurface() const;
    /**
     * @returns The DataDeviceInterface which started the drag and drop operation.
     * @see isDrag
     */
    KWaylandServer::AbstractDataSource *dragSource() const;
    /**
     * Sets the current drag target to @p surface.
     *
     * Sends a drag leave event to the current target and an enter event to @p surface.
     * The enter position is derived from @p globalPosition and transformed by @p inputTransformation.
     */
    void setDragTarget(AbstractDropHandler *dropTarget, SurfaceInterface *surface, const QPointF &globalPosition, const QMatrix4x4 &inputTransformation);
    /**
     * Sets the current drag target to @p surface.
     *
     * Sends a drag leave event to the current target and an enter event to @p surface.
     * The enter position is derived from current global position and transformed by @p inputTransformation.
     */
    void setDragTarget(AbstractDropHandler *dropTarget, SurfaceInterface *surface, const QMatrix4x4 &inputTransformation = QMatrix4x4());
    ///@}

    AbstractDropHandler *dropHandlerForSurface(SurfaceInterface *surface) const;

    /**
     * If there is a current drag in progress, force it to cancel
     */
    void cancelDrag();

    /**
     * @name Pointer related methods
     */
    ///@{
    /**
     * Updates the global pointer @p pos.
     *
     * Sends a pointer motion event to the focused pointer surface.
     */
    void notifyPointerMotion(const QPointF &pos);
    /**
     * @returns the global pointer position
     */
    QPointF pointerPos() const;
    /**
     * Sets the focused pointer @p surface.
     * All pointer events will be sent to the @p surface till a new focused pointer surface gets
     * installed. When the focus pointer surface changes a leave event is sent to the previous
     * focused surface.
     *
     * To unset the focused pointer surface pass @c nullptr as @p surface.
     *
     * Pointer motion events are adjusted to the local position based on the @p surfacePosition.
     * If the surface changes it's position in the global coordinate system
     * use setFocusedPointerSurfacePosition to update.
     * The surface position is used to create the base transformation matrix to go from global
     * to surface local coordinates. The default generated matrix is a translation with
     * negative @p surfacePosition.
     *
     * @param surface The surface which should become the new focused pointer surface.
     * @param surfacePosition The position of the surface in the global coordinate system
     *
     * @see setPointerPos
     * @see focucedPointerSurface
     * @see focusedPointer
     * @see setFocusedPointerSurfacePosition
     * @see focusedPointerSurfacePosition
     * @see setFocusedPointerSurfaceTransformation
     * @see focusedPointerSurfaceTransformation
     */
    void notifyPointerEnter(SurfaceInterface *surface, const QPointF &position, const QPointF &surfacePosition = QPointF());
    /**
     * Sets the focused pointer @p surface.
     * All pointer events will be sent to the @p surface till a new focused pointer surface gets
     * installed. When the focus pointer surface changes a leave event is sent to the previous
     * focused surface.
     *
     * To unset the focused pointer surface pass @c nullptr as @p surface.
     *
     * Pointer motion events are adjusted to the local position based on the @p transformation.
     * If the surface changes it's position in the global coordinate system
     * use setFocusedPointerSurfaceTransformation to update.
     *
     * @param surface The surface which should become the new focused pointer surface.
     * @param transformation The transformation to transform global into local coordinates
     *
     * @see setPointerPos
     * @see focucedPointerSurface
     * @see focusedPointer
     * @see setFocusedPointerSurfacePosition
     * @see focusedPointerSurfacePosition
     * @see setFocusedPointerSurfaceTransformation
     * @see focusedPointerSurfaceTransformation
     */
    void notifyPointerEnter(SurfaceInterface *surface, const QPointF &position, const QMatrix4x4 &transformation);
    void notifyPointerLeave();
    /**
     * @returns The currently focused pointer surface, that is the surface receiving pointer events.
     * @see notifyPointerEnter
     * @see notifyPointerLeave
     */
    SurfaceInterface *focusedPointerSurface() const;
    PointerInterface *pointer() const;
    /**
     * Updates the global position of the currently focused pointer surface.
     *
     * Updating the focused surface position also generates a new transformation matrix.
     * The default generated matrix is a translation with negative @p surfacePosition.
     * If a different transformation is required a dedicated call to
     * @link setFocusedPointerSurfaceTransformation is required.
     *
     * @param surfacePosition The new global position of the focused pointer surface
     * @see focusedPointerSurface
     * @see focusedPointerSurfaceTransformation
     * @see setFocusedPointerSurfaceTransformation
     */
    void setFocusedPointerSurfacePosition(const QPointF &surfacePosition);
    /**
     * @returns The position of the focused pointer surface in global coordinates.
     * @see setFocusedPointerSurfacePosition
     * @see focusedPointerSurfaceTransformation
     */
    QPointF focusedPointerSurfacePosition() const;
    /**
     * Sets the @p transformation for going from global to local coordinates.
     *
     * The default transformation gets generated from the surface position and reset whenever
     * the surface position changes.
     *
     * @see focusedPointerSurfaceTransformation
     * @see focusedPointerSurfacePosition
     * @see setFocusedPointerSurfacePosition
     */
    void setFocusedPointerSurfaceTransformation(const QMatrix4x4 &transformation);
    /**
     * @returns The transformation applied to pointer position to go from global to local coordinates.
     * @see setFocusedPointerSurfaceTransformation
     */
    QMatrix4x4 focusedPointerSurfaceTransformation() const;
    /**
     * Marks the specified @a button as pressed or released based on @a state.
     */
    void notifyPointerButton(quint32 button, PointerButtonState state);
    /**
     * @overload
     */
    void notifyPointerButton(Qt::MouseButton button, PointerButtonState state);
    void notifyPointerFrame();
    /**
     * @returns whether the @p button is pressed
     */
    bool isPointerButtonPressed(quint32 button) const;
    /**
     * @returns whether the @p button is pressed
     */
    bool isPointerButtonPressed(Qt::MouseButton button) const;
    /**
     * @returns the last serial for @p button.
     */
    quint32 pointerButtonSerial(quint32 button) const;
    /**
     * @returns the last serial for @p button.
     */
    quint32 pointerButtonSerial(Qt::MouseButton button) const;
    /**
     * Sends axis events to the currently focused pointer surface.
     *
     * @param orientation The scroll axis.
     * @param delta The length of a vector along the specified axis @p orientation.
     * @param deltaV120 The high-resolution scrolling axis value.
     * @param source Describes how the axis event was physically generated.
     */
    void notifyPointerAxis(Qt::Orientation orientation, qreal delta, qint32 deltaV120, PointerAxisSource source);
    /**
     * @returns true if there is a pressed button with the given @p serial
     */
    bool hasImplicitPointerGrab(quint32 serial) const;

    /**
     * A relative motion is in the same dimension as regular motion events,
     * except they do not represent an absolute position. For example,
     * moving a pointer from (x, y) to (x', y') would have the equivalent
     * relative motion (x' - x, y' - y). If a pointer motion caused the
     * absolute pointer position to be clipped by for example the edge of the
     * monitor, the relative motion is unaffected by the clipping and will
     * represent the unclipped motion.
     *
     * This method also contains non-accelerated motion deltas (@p deltaNonAccelerated).
     * The non-accelerated delta is, when applicable, the regular pointer motion
     * delta as it was before having applied motion acceleration and other
     * transformations such as normalization.
     *
     * Note that the non-accelerated delta does not represent 'raw' events as
     * they were read from some device. Pointer motion acceleration is device-
     * and configuration-specific and non-accelerated deltas and accelerated
     * deltas may have the same value on some devices.
     *
     * Relative motions are not coupled to wl_pointer.motion events (see {@link setPointerPos},
     * and can be sent in combination with such events, but also independently. There may
     * also be scenarios where wl_pointer.motion is sent, but there is no
     * relative motion. The order of an absolute and relative motion event
     * originating from the same physical motion is not guaranteed.
     *
     * Sending relative pointer events only makes sense if the RelativePointerManagerInterface
     * is created on the Display.
     *
     * @param delta Motion vector
     * @param deltaNonAccelerated non-accelerated motion vector
     * @param microseconds timestamp with microseconds granularity
     * @see setPointerPos
     */
    void relativePointerMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds timestamp);

    /**
     * Starts a multi-finger swipe gesture for the currently focused pointer surface.
     *
     * Such gestures are normally reported through dedicated input devices such as touchpads.
     *
     * The gesture is usually initiated by multiple fingers moving in the
     * same direction but once initiated the direction may change.
     * The precise conditions of when such a gesture is detected are
     * implementation-dependent.
     *
     * Only one gesture (either swipe or pinch or hold) can be active at a given time.
     *
     * @param fingerCount The number of fingers involved in this multi-finger touchpad gesture
     *
     * @see PointerGesturesInterface
     * @see focusedPointerSurface
     * @see updatePointerSwipeGesture
     * @see endPointerSwipeGesture
     * @see cancelPointerSwipeGesture
     * @see startPointerPinchGesture
     */
    void startPointerSwipeGesture(quint32 fingerCount);

    /**
     * The position of the logical center of the currently active multi-finger swipe gesture changes.
     *
     * @param delta coordinates are relative coordinates of the logical center of the gesture compared to the previous event.
     * @see startPointerSwipeGesture
     * @see endPointerSwipeGesture
     * @see cancelPointerSwipeGesture
     */
    void updatePointerSwipeGesture(const QPointF &delta);

    /**
     * The multi-finger swipe gesture ended. This may happen when one or more fingers are lifted.
     * @see startPointerSwipeGesture
     * @see updatePointerSwipeGesture
     * @see cancelPointerSwipeGesture
     * @see 5.29
     */
    void endPointerSwipeGesture();

    /**
     * The multi-finger swipe gestures ended and got cancelled by the Wayland compositor.
     * @see startPointerSwipeGesture
     * @see updatePointerSwipeGesture
     * @see endPointerSwipeGesture
     */
    void cancelPointerSwipeGesture();

    /**
     * Starts a multi-finch pinch gesture for the currently focused pointer surface.
     *
     * Such gestures are normally reported through dedicated input devices such as touchpads.
     *
     * The gesture is usually initiated by multiple fingers moving towards
     * each other or away from each other, or by two or more fingers rotating
     * around a logical center of gravity. The precise conditions of when
     * such a gesture is detected are implementation-dependent.
     *
     * Only one gesture (either swipe or pinch or hold) can be active at a given time.
     *
     * @param fingerCount The number of fingers involved in this multi-touch touchpad gesture
     *
     * @see PointerGesturesInterface
     * @see focusedPointerSurface
     * @see updatePointerPinchGesture
     * @see endPointerPinchGesture
     * @see cancelPointerPinchGesture
     * @see startPointerSwipeGesture
     */
    void startPointerPinchGesture(quint32 fingerCount);

    /**
     * The position of the logical center, the rotation or the relative scale of this
     * multi-finger pinch gesture changes.
     *
     * @param delta coordinates are relative coordinates of the logical center of the gesture compared to the previous event.
     * @param scale an absolute scale compared to the gesture start
     * @param rotation relative angle in degrees clockwise compared to the previous start of update
     * @see startPointerPinchGesture
     * @see endPointerPinchGesture
     * @see cancelPointerPinchGesture
     */
    void updatePointerPinchGesture(const QPointF &delta, qreal scale, qreal rotation);

    /**
     *
     * @see startPointerPinchGesture
     * @see updatePointerPinchGesture
     * @see cancelPointerPinchGesture
     */
    void endPointerPinchGesture();

    /**
     *
     * @see startPointerPinchGesture
     * @see updatePointerPinchGesture
     * @see endPointerPinchGesture
     */
    void cancelPointerPinchGesture();

    /**
     * Starts a multi-finger hold gesture for the currently focused pointer surface.
     *
     * Such gestures are normally reported through dedicated input devices such as touchpads.
     *
     * The gesture is usually initiated by multiple fingers being held down on the touchpad.
     * The precise conditions of when such a gesture is detected are
     * implementation-dependent.
     *
     * Only one gesture (either swipe or pinch or hold) can be active at a given time.
     *
     * @param fingerCount The number of fingers involved in this multi-finger touchpad gesture
     *
     * @see PointerGesturesInterface
     * @see focusedPointerSurface
     * @see endPointerHoldeGesture
     * @see cancelPointerHoldGesture
     */
    void startPointerHoldGesture(quint32 fingerCount);

    /**
     * The multi-finger hold gesture ended. This may happen when one or more fingers are lifted.
     * @see startPointerHoldGesture
     * @see cancelPointerHoldGesture
     */
    void endPointerHoldGesture();

    /**
     * The multi-finger swipe gestures ended and got cancelled by the Wayland compositor.
     * @see startPointerHoldGesture
     * @see endPointerHoldGesture
     */
    void cancelPointerHoldGesture();
    ///@}

    /**
     * Passes keyboard focus to @p surface.
     *
     * If the SeatInterface has the keyboard capability, also the focused
     * text input surface will be set to @p surface.
     *
     * @see focusedKeyboardSurface
     * @see hasKeyboard
     * @see setFocusedTextInputSurface
     */
    void setFocusedKeyboardSurface(SurfaceInterface *surface);
    SurfaceInterface *focusedKeyboardSurface() const;
    KeyboardInterface *keyboard() const;
    void notifyKeyboardKey(quint32 keyCode, KeyboardKeyState state);
    void notifyKeyboardModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group);
    ///@}

    /**
     * @name  touch related methods
     */
    ///@{
    void setFocusedTouchSurface(SurfaceInterface *surface, const QPointF &surfacePosition = QPointF());
    SurfaceInterface *focusedTouchSurface() const;
    TouchInterface *touch() const;
    void setFocusedTouchSurfacePosition(const QPointF &surfacePosition);
    QPointF focusedTouchSurfacePosition() const;
    void notifyTouchDown(qint32 id, const QPointF &globalPosition);
    void notifyTouchUp(qint32 id);
    void notifyTouchMotion(qint32 id, const QPointF &globalPosition);
    void notifyTouchFrame();
    void notifyTouchCancel();
    bool isTouchSequence() const;
    QPointF firstTouchPointPosition() const;
    /**
     * @returns true if there is a touch sequence going on associated with a touch
     * down of the given @p serial.
     */
    bool hasImplicitTouchGrab(quint32 serial) const;
    ///@}

    /**
     * @name Text input related methods.
     */
    ///@{
    /**
     * Passes text input focus to @p surface.
     *
     * If the SeatInterface has the keyboard capability this method will
     * be invoked automatically when setting the focused keyboard surface.
     *
     * In case there is a TextInputV2Interface for the @p surface, the enter
     * event will be triggered on the TextInputV2Interface for @p surface.
     * The focusedTextInput will be set to that TextInputV2Interface. If there
     * is no TextInputV2Interface for that @p surface, it might get updated later on.
     * In both cases the signal focusedTextInputChanged will be emitted.
     *
     * @see focusedTextInputSurface
     * @see focusedTextInput
     * @see focusedTextInputChanged
     * @see setFocusedKeyboardSurface
     */
    void setFocusedTextInputSurface(SurfaceInterface *surface);
    /**
     * @returns The SurfaceInterface which is currently focused for text input.
     * @see setFocusedTextInputSurface
     */
    SurfaceInterface *focusedTextInputSurface() const;

    TextInputV1Interface *textInputV1() const;

    /**
     * The currently focused text input, may be @c null even if there is a
     * focused text input surface set.
     *
     * The focused text input might not be enabled for the {@link focusedTextInputSurface}.
     * It is recommended to check the enabled state before interacting with the
     * TextInputV2Interface.
     *
     * @see focusedTextInputChanged
     * @see focusedTextInputSurface
     */
    TextInputV2Interface *textInputV2() const;

    TextInputV3Interface *textInputV3() const;
    ///@}

    /**
     * @returns The DataDeviceInterface holding the current clipboard selection.
     * @see selectionChanged
     * @see setSelection
     * This may be null
     */
    KWaylandServer::AbstractDataSource *selection() const;

    /**
     * This method allows to manually set the @p dataDevice for the current clipboard selection.
     * The clipboard selection is handled automatically in SeatInterface.
     * If a DataDeviceInterface belonging to the current focused KeyboardInterface
     * sets a selection, the current clipboard selection will be updated automatically.
     * With this method it's possible to override the automatic clipboard update for
     * e.g. the case of a clipboard manager.
     *
     * @param dataDevice Sets the current clipboard selection.
     * @see selection
     * @see selectionChanged
     */
    void setSelection(AbstractDataSource *selection);

    KWaylandServer::AbstractDataSource *primarySelection() const;
    void setPrimarySelection(AbstractDataSource *selection);

    void startDrag(AbstractDataSource *source, SurfaceInterface *sourceSurface, int dragSerial = -1, DragAndDropIcon *dragIcon = nullptr);

    /**
     * Returns the additional icon attached to the cursor during a drag-and-drop operation.
     * This function returns @c null if no drag-and-drop is active or no icon has been attached.
     */
    DragAndDropIcon *dragIcon() const;

    static SeatInterface *get(wl_resource *native);

Q_SIGNALS:
    void nameChanged(const QString &);
    void hasPointerChanged(bool);
    void hasKeyboardChanged(bool);
    void hasTouchChanged(bool);
    void pointerPosChanged(const QPointF &pos);
    void touchMoved(qint32 id, quint32 serial, const QPointF &globalPosition);
    void timestampChanged();

    /**
     * Emitted whenever the selection changes
     * @see selection
     * @see setSelection
     */
    void selectionChanged(KWaylandServer::AbstractDataSource *);

    /**
     * Emitted whenever the primary selection changes
     * @see primarySelection
     */
    void primarySelectionChanged(KWaylandServer::AbstractDataSource *);

    /**
     * Emitted when a drag'n'drop operation is started
     * @see dragEnded
     */
    void dragStarted();
    /**
     * Emitted when a drag'n'drop operation ended, either by dropping or canceling.
     * @see dragStarted
     */
    void dragEnded();
    /**
     * Emitted whenever the drag surface for motion events changed.
     * @see dragSurface
     */
    void dragSurfaceChanged();

    /** Emitted when a drop ocurrs in a drag'n'drop operation. This is emitted
     * before dragEnded
     */
    void dragDropped();
    /**
     * Emitted whenever the focused text input changed.
     * @see focusedTextInput
     */
    void focusedTextInputSurfaceChanged();
    /**
     * Emitted whenever the focused keyboard is about to change.
     * @see focusedKeyboardSurface
     */
    void focusedKeyboardSurfaceAboutToChange(SurfaceInterface *nextSurface);

private:
    std::unique_ptr<SeatInterfacePrivate> d;
    friend class SeatInterfacePrivate;
};

}

Q_DECLARE_METATYPE(KWaylandServer::SeatInterface *)
