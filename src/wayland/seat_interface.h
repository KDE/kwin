/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_SEAT_INTERFACE_H
#define WAYLAND_SERVER_SEAT_INTERFACE_H

#include <QObject>
#include <QPoint>
#include <QMatrix4x4>

#include <KWaylandServer/kwaylandserver_export.h>
#include "global.h"
#include "keyboard_interface.h"
#include "pointer_interface.h"
#include "touch_interface.h"

struct wl_client;
struct wl_resource;

namespace KWaylandServer
{

class DataDeviceInterface;
class AbstractDataSource;
class Display;
class SurfaceInterface;
class TextInputV2Interface;
class TextInputV3Interface;

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
 *
 * @since 5.59
 **/
enum class PointerAxisSource {
    Unknown,
    Wheel,
    Finger,
    Continuous,
    WheelTilt
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
 * seat->setFocusedPointerSurface(surface, QPointF(100, 200)); // surface at it's global position
 * seat->setTimestamp(100);
 * seat->setPointerPos(QPointF(350, 210)); // global pos, local pos in surface: 250,10
 * seat->setTimestamp(110);
 * seat->pointerButtonPressed(Qt::LeftButton);
 * seat->setTimestamp(120);
 * seat->pointerButtonReleased(Qt::LeftButton);
 * @endcode
 *
 * @see KeyboardInterface
 * @see PointerInterface
 * @see TouchInterface
 * @see SurfaceInterface
 **/
class KWAYLANDSERVER_EXPORT SeatInterface : public Global
{
    Q_OBJECT
    /**
     * The name of the Seat
     **/
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    /**
     * Whether the SeatInterface supports a pointer device.
     **/
    Q_PROPERTY(bool pointer READ hasPointer WRITE setHasPointer NOTIFY hasPointerChanged)
    /**
     * Whether the SeatInterface supports a keyboard device.
     **/
    Q_PROPERTY(bool keyboard READ hasKeyboard WRITE setHasKeyboard NOTIFY hasKeyboardChanged)
    /**
     * Whether the SeatInterface supports a touch device.
     * @deprecated Since 5.5, use touch
     **/
    Q_PROPERTY(bool tourch READ hasTouch WRITE setHasTouch NOTIFY hasTouchChanged)
    /**
     * Whether the SeatInterface supports a touch device.
     **/
    Q_PROPERTY(bool touch READ hasTouch WRITE setHasTouch NOTIFY hasTouchChanged)
    /**
     * The global pointer position.
     **/
    Q_PROPERTY(QPointF pointerPos READ pointerPos WRITE setPointerPos NOTIFY pointerPosChanged)
    /**
     * The current timestamp passed to the input events.
     **/
    Q_PROPERTY(quint32 timestamp READ timestamp WRITE setTimestamp NOTIFY timestampChanged)
public:
    explicit SeatInterface(Display *display, QObject *parent = nullptr);
    virtual ~SeatInterface();

    QString name() const;
    bool hasPointer() const;
    bool hasKeyboard() const;
    bool hasTouch() const;

    void setName(const QString &name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

    void setTimestamp(quint32 time);
    quint32 timestamp() const;

    /**
     * @name Drag'n'Drop related methods
     **/
    ///@{
    /**
     * @returns whether there is currently a drag'n'drop going on.
     * @since 5.6
     * @see isDragPointer
     * @see isDragTouch
     * @see dragStarted
     * @see dragEnded
     **/
    bool isDrag() const;
    /**
     * @returns whether the drag'n'drop is operated through the pointer device
     * @since 5.6
     * @see isDrag
     * @see isDragTouch
     **/
    bool isDragPointer() const;
    /**
     * @returns whether the drag'n'drop is operated through the touch device
     * @since 5.6
     * @see isDrag
     * @see isDragPointer
     **/
    bool isDragTouch() const;
    /**
     * @returns The transformation applied to go from global to local coordinates for drag motion events.
     * @see dragSurfaceTransformation
     * @since 5.6
     **/
    QMatrix4x4 dragSurfaceTransformation() const;
    /**
     * @returns The currently focused Surface for drag motion events.
     * @since 5.6
     * @see dragSurfaceTransformation
     * @see dragSurfaceChanged
     **/
    SurfaceInterface *dragSurface() const;
    /**
     * @returns The PointerInterface which triggered the drag operation
     * @since 5.6
     * @see isDragPointer
     **/
    PointerInterface *dragPointer() const;
    /**
     * @returns The DataDeviceInterface which started the drag and drop operation.
     * @see isDrag
     * @since 5.6
     **/
    DataDeviceInterface *dragSource() const;
    /**
     * Sets the current drag target to @p surface.
     *
     * Sends a drag leave event to the current target and an enter event to @p surface.
     * The enter position is derived from @p globalPosition and transformed by @p inputTransformation.
     * @since 5.6
     **/
    void setDragTarget(SurfaceInterface *surface, const QPointF &globalPosition, const QMatrix4x4 &inputTransformation);
    /**
     * Sets the current drag target to @p surface.
     *
     * Sends a drag leave event to the current target and an enter event to @p surface.
     * The enter position is derived from current global position and transformed by @p inputTransformation.
     * @since 5.6
     **/
    void setDragTarget(SurfaceInterface *surface, const QMatrix4x4 &inputTransformation = QMatrix4x4());
    ///@}

    /**
     * @name Pointer related methods
     **/
    ///@{
    /**
     * Updates the global pointer @p pos.
     *
     * Sends a pointer motion event to the focused pointer surface.
     **/
    void setPointerPos(const QPointF &pos);
    /**
     * @returns the global pointer position
     **/
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
     **/
    void setFocusedPointerSurface(SurfaceInterface *surface, const QPointF &surfacePosition = QPoint());
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
     * @since 5.6
     **/
    void setFocusedPointerSurface(SurfaceInterface *surface, const QMatrix4x4 &transformation);
    /**
     * @returns The currently focused pointer surface, that is the surface receiving pointer events.
     * @see setFocusedPointerSurface
     **/
    SurfaceInterface *focusedPointerSurface() const;
    /**
     * @returns The PointerInterface belonging to the focused pointer surface, if any.
     * @see setFocusedPointerSurface
     **/
    PointerInterface *focusedPointer() const;
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
     * @see setFocusedPointerSurface
     * @see focusedPointerSurfaceTransformation
     * @see setFocusedPointerSurfaceTransformation
     **/
    void setFocusedPointerSurfacePosition(const QPointF &surfacePosition);
    /**
     * @returns The position of the focused pointer surface in global coordinates.
     * @see setFocusedPointerSurfacePosition
     * @see setFocusedPointerSurface
     * @see focusedPointerSurfaceTransformation
     **/
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
     * @since 5.6
     **/
    void setFocusedPointerSurfaceTransformation(const QMatrix4x4 &transformation);
    /**
     * @returns The transformation applied to pointer position to go from global to local coordinates.
     * @see setFocusedPointerSurfaceTransformation
     * @since 5.6
     **/
    QMatrix4x4 focusedPointerSurfaceTransformation() const;
    /**
     * Marks the @p button as pressed.
     *
     * If there is a focused pointer surface a button pressed event is sent to it.
     *
     * @param button The Linux button code
     **/
    void pointerButtonPressed(quint32 button);
    /**
     * @overload
     **/
    void pointerButtonPressed(Qt::MouseButton button);
    /**
     * Marks the @p button as released.
     *
     * If there is a focused pointer surface a button release event is sent to it.
     *
     * @param button The Linux button code
     **/
    void pointerButtonReleased(quint32 button);
    /**
     * @overload
     **/
    void pointerButtonReleased(Qt::MouseButton button);
    /**
     * @returns whether the @p button is pressed
     **/
    bool isPointerButtonPressed(quint32 button) const;
    /**
     * @returns whether the @p button is pressed
     **/
    bool isPointerButtonPressed(Qt::MouseButton button) const;
    /**
     * @returns the last serial for @p button.
     **/
    quint32 pointerButtonSerial(quint32 button) const;
    /**
     * @returns the last serial for @p button.
     **/
    quint32 pointerButtonSerial(Qt::MouseButton button) const;
    /**
     * Sends axis events to the currently focused pointer surface.
     *
     * @param orientation The scroll axis.
     * @param delta The length of a vector along the specified axis @p orientation.
     * @param discreteDelta The number of discrete steps, e.g. mouse wheel clicks.
     * @param source Describes how the axis event was physically generated.
     * @since 5.59
     * @todo Drop V5 suffix with KF6.
     **/
    void pointerAxisV5(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source);
    /**
     * @see pointerAxisV5
     **/
    void pointerAxis(Qt::Orientation orientation, quint32 delta);
    /**
     * @returns true if there is a pressed button with the given @p serial
     * @since 5.6
     **/
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
     * @since 5.28
     **/
    void relativePointerMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds);

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
     * Only one gesture (either swipe or pinch) can be active at a given time.
     *
     * @param fingerCount The number of fingers involved in this multi-finger touchpad gesture
     *
     * @see PointerGesturesInterface
     * @see focusedPointerSurface
     * @see updatePointerSwipeGesture
     * @see endPointerSwipeGesture
     * @see cancelPointerSwipeGesture
     * @see startPointerPinchGesture
     * @since 5.29
     **/
    void startPointerSwipeGesture(quint32 fingerCount);

    /**
     * The position of the logical center of the currently active multi-finger swipe gesture changes.
     *
     * @param delta coordinates are relative coordinates of the logical center of the gesture compared to the previous event.
     * @see startPointerSwipeGesture
     * @see endPointerSwipeGesture
     * @see cancelPointerSwipeGesture
     * @since 5.29
     **/
    void updatePointerSwipeGesture(const QSizeF &delta);

    /**
     * The multi-finger swipe gesture ended. This may happen when one or more fingers are lifted.
     * @see startPointerSwipeGesture
     * @see updatePointerSwipeGesture
     * @see cancelPointerSwipeGesture
     * @see 5.29
     **/
    void endPointerSwipeGesture();

    /**
     * The multi-finger swipe gestures ended and got cancelled by the Wayland compositor.
     * @see startPointerSwipeGesture
     * @see updatePointerSwipeGesture
     * @see endPointerSwipeGesture
     * @since 5.29
     **/
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
     * Only one gesture (either swipe or pinch) can be active at a given time.
     *
     * @param fingerCount The number of fingers involved in this multi-touch touchpad gesture
     *
     * @see PointerGesturesInterface
     * @see focusedPointerSurface
     * @see updatePointerPinchGesture
     * @see endPointerPinchGesture
     * @see cancelPointerPinchGesture
     * @see startPointerSwipeGesture
     * @since 5.29
     **/
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
     * @since 5.29
     **/
    void updatePointerPinchGesture(const QSizeF &delta, qreal scale, qreal rotation);

    /**
     *
     * @see startPointerPinchGesture
     * @see updatePointerPinchGesture
     * @see cancelPointerPinchGesture
     * @since 5.29
     **/
    void endPointerPinchGesture();

    /**
     *
     * @see startPointerPinchGesture
     * @see updatePointerPinchGesture
     * @see endPointerPinchGesture
     * @since 5.29
     **/
    void cancelPointerPinchGesture();
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
     **/
    void setFocusedKeyboardSurface(SurfaceInterface *surface);
    SurfaceInterface *focusedKeyboardSurface() const;
    KeyboardInterface *keyboard() const;
    ///@}

    /**
     * @name  touch related methods
     **/
    ///@{
    void setFocusedTouchSurface(SurfaceInterface *surface, const QPointF &surfacePosition = QPointF());
    SurfaceInterface *focusedTouchSurface() const;
    TouchInterface *focusedTouch() const;
    void setFocusedTouchSurfacePosition(const QPointF &surfacePosition);
    QPointF focusedTouchSurfacePosition() const;
    qint32 touchDown(const QPointF &globalPosition);
    void touchUp(qint32 id);
    void touchMove(qint32 id, const QPointF &globalPosition);
    void touchFrame();
    void cancelTouchSequence();
    bool isTouchSequence() const;
    /**
     * @returns true if there is a touch sequence going on associated with a touch
     * down of the given @p serial.
     * @since 5.XX
     **/
    bool hasImplicitTouchGrab(quint32 serial) const;
    ///@}

    /**
     * @name Text input related methods.
     **/
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
     * @since 5.23
     **/
    void setFocusedTextInputSurface(SurfaceInterface *surface);
    /**
     * @returns The SurfaceInterface which is currently focused for text input.
     * @see setFocusedTextInputSurface
     * @since 5.23
     **/
    SurfaceInterface *focusedTextInputSurface() const;
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
     * @since 5.23
     **/
    TextInputV2Interface *textInputV2() const;
    
    TextInputV3Interface *textInputV3() const;
    ///@}

    /**
     * @returns The DataDeviceInterface holding the current clipboard selection.
     * @since 5.24
     * @see selectionChanged
     * @see setSelection
     * This may be null
     **/
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
     * @since 5.24
     **/
    void setSelection(AbstractDataSource *selection);

    void setPrimarySelection(AbstractDataSource *selection);

    static SeatInterface *get(wl_resource *native);

Q_SIGNALS:
    void nameChanged(const QString&);
    void hasPointerChanged(bool);
    void hasKeyboardChanged(bool);
    void hasTouchChanged(bool);
    void pointerPosChanged(const QPointF &pos);
    void touchMoved(qint32 id, quint32 serial, const QPointF &globalPosition);
    void timestampChanged(quint32);

    void pointerCreated(KWaylandServer::PointerInterface*);
    void keyboardCreated(KWaylandServer::KeyboardInterface*);
    void touchCreated(KWaylandServer::TouchInterface*);

    /**
     * Emitted whenever the focused pointer changes
     * @since 5.6
     **/
    void focusedPointerChanged(KWaylandServer::PointerInterface*);

    /**
     * Emitted whenever the selection changes
     * @since 5.56
     * @see selection
     * @see setSelection
     **/
    void selectionChanged(KWaylandServer::AbstractDataSource*);

    /**
     * Emitted whenever the primary selection changes
     * @see primarySelection
     **/
    void primarySelectionChanged(KWaylandServer::AbstractDataSource*);

    /**
     * Emitted when a drag'n'drop operation is started
     * @since 5.6
     * @see dragEnded
     **/
    void dragStarted();
    /**
     * Emitted when a drag'n'drop operation ended, either by dropping or canceling.
     * @since 5.6
     * @see dragStarted
     **/
    void dragEnded();
    /**
     * Emitted whenever the drag surface for motion events changed.
     * @since 5.6
     * @see dragSurface
     **/
    void dragSurfaceChanged();
    /**
     * Emitted whenever the focused text input changed.
     * @see focusedTextInput
     * @since 5.23
     **/
    void focusedTextInputSurfaceChanged();

private:
    friend class DataControlDeviceV1Interface;
    friend class DataDeviceInterface;
    friend class PrimarySelectionDeviceV1Interface;
    friend class TextInputManagerV2InterfacePrivate;
    friend class KeyboardInterface;

    class Private;
    Private *d_func() const;
};

}

Q_DECLARE_METATYPE(KWaylandServer::SeatInterface*)

#endif
