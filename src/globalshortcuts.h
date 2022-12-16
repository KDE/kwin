/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// KWin
#include <kwinglobals.h>
// Qt
#include <QKeySequence>

#include <memory>

class QAction;
class KGlobalAccelD;
class KGlobalAccelInterface;

namespace KWin
{
class GlobalShortcut;
class SwipeGesture;
class PinchGesture;
class GestureRecognizer;

enum class DeviceType {
    Touchpad,
    Touchscreen
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

    void registerTouchpadSwipe(QAction *action, SwipeDirection direction, uint fingerCount = 4);

    void registerRealtimeTouchpadSwipe(QAction *onUp, std::function<void(qreal)> progressCallback, SwipeDirection direction, uint fingerCount = 4);

    void registerTouchpadPinch(QAction *action, PinchDirection direction, uint fingerCount = 4);

    void registerRealtimeTouchpadPinch(QAction *onUp, std::function<void(qreal)> progressCallback, PinchDirection direction, uint fingerCount = 4);

    void registerTouchscreenSwipe(QAction *action, std::function<void(qreal)> progressCallback, SwipeDirection direction, uint fingerCount);
    void forceRegisterTouchscreenSwipe(QAction *action, std::function<void(qreal)> progressCallback, SwipeDirection direction, uint fingerCount);

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

    void processSwipeStart(DeviceType device, uint fingerCount);
    void processSwipeUpdate(DeviceType device, const QPointF &delta);
    void processSwipeCancel(DeviceType device);
    void processSwipeEnd(DeviceType device);

    void processPinchStart(uint fingerCount);
    void processPinchUpdate(qreal scale, qreal angleDelta, const QPointF &delta);
    void processPinchCancel();
    void processPinchEnd();

    void setKGlobalAccelInterface(KGlobalAccelInterface *interface)
    {
        m_kglobalAccelInterface = interface;
    }

private:
    void objectDeleted(QObject *object);
    bool addIfNotExists(GlobalShortcut sc, DeviceType device = DeviceType::Touchpad);

    QVector<GlobalShortcut> m_shortcuts;

    std::unique_ptr<KGlobalAccelD> m_kglobalAccel;
    KGlobalAccelInterface *m_kglobalAccelInterface = nullptr;
    std::unique_ptr<GestureRecognizer> m_touchpadGestureRecognizer;
    std::unique_ptr<GestureRecognizer> m_touchscreenGestureRecognizer;
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
struct SwipeShortcut
{
    DeviceType device;
    SwipeDirection direction;
    uint fingerCount;
    bool operator==(const SwipeShortcut &rhs) const
    {
        return direction == rhs.direction && fingerCount == rhs.fingerCount && device == rhs.device;
    }
};
struct RealtimeFeedbackSwipeShortcut
{
    DeviceType device;
    SwipeDirection direction;
    std::function<void(qreal)> progressCallback;
    uint fingerCount;

    template<typename T>
    bool operator==(const T &rhs) const
    {
        return direction == rhs.direction && fingerCount == rhs.fingerCount && device == rhs.device;
    }
};

struct PinchShortcut
{
    PinchDirection direction;
    uint fingerCount;
    bool operator==(const PinchShortcut &rhs) const
    {
        return direction == rhs.direction && fingerCount == rhs.fingerCount;
    }
};

struct RealtimeFeedbackPinchShortcut
{
    PinchDirection direction;
    std::function<void(qreal)> scaleCallback;
    uint fingerCount;

    template<typename T>
    bool operator==(const T &rhs) const
    {
        return direction == rhs.direction && fingerCount == rhs.fingerCount;
    }
};

using Shortcut = std::variant<KeyboardShortcut, PointerButtonShortcut, PointerAxisShortcut, SwipeShortcut, RealtimeFeedbackSwipeShortcut, PinchShortcut, RealtimeFeedbackPinchShortcut>;

class GlobalShortcut
{
public:
    GlobalShortcut(Shortcut &&shortcut, QAction *action);
    ~GlobalShortcut();

    void invoke() const;
    QAction *action() const;
    const Shortcut &shortcut() const;
    SwipeGesture *swipeGesture() const;
    PinchGesture *pinchGesture() const;

private:
    std::shared_ptr<SwipeGesture> m_swipeGesture;
    std::shared_ptr<PinchGesture> m_pinchGesture;
    Shortcut m_shortcut = {};
    QAction *m_action = nullptr;
};

} // namespace
