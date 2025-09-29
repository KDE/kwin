/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// KWin
#include "effect/globals.h"
// Qt
#include "core/inputdevice.h"

#include <QKeySequence>

#include <memory>

class QAction;
#if KWIN_BUILD_GLOBALSHORTCUTS
class KGlobalAccelD;
class KGlobalAccelInterface;
#endif
namespace KWin
{
class GlobalShortcut;
class SwipeGesture;
class PinchGesture;
class StrokeGesture;
class StrokeGestures;
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

    void registerTouchpadSwipe(SwipeDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback = {});
    void registerTouchpadSwipe(SwipeGesture *swipeGesture);
    void registerTouchpadPinch(PinchDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback = {});
    void registerTouchpadPinch(PinchGesture *pinchGesture);
    void registerTouchscreenSwipe(SwipeDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback = {});
    void registerTouchscreenSwipe(SwipeGesture *swipeGesture);
    void forceRegisterTouchscreenSwipe(SwipeDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback = {});

    void registerStroke(Qt::KeyboardModifiers modifiers, const QList<QPointF> &points, QAction *action);

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
    bool processKey(Qt::KeyboardModifiers modifiers, int keyQt, KeyboardKeyState state);
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
    bool processAxis(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, qreal delta);

    void processSwipeStart(DeviceType device, uint fingerCount);
    void processSwipeUpdate(DeviceType device, const QPointF &delta);
    void processSwipeCancel(DeviceType device);
    void processSwipeEnd(DeviceType device);

    void processPinchStart(uint fingerCount);
    void processPinchUpdate(qreal scale, qreal angleDelta, const QPointF &delta);
    void processPinchCancel();
    void processPinchEnd();

    StrokeGestures *strokeGestures() const;

private:
    void objectDeleted(QObject *object);
    bool add(GlobalShortcut sc, DeviceType device = DeviceType::Touchpad);

    QList<GlobalShortcut> m_shortcuts;

#if KWIN_BUILD_GLOBALSHORTCUTS
    std::unique_ptr<KGlobalAccelD> m_kglobalAccel;
    KGlobalAccelInterface *m_kglobalAccelInterface = nullptr;
#endif
    std::unique_ptr<GestureRecognizer> m_touchpadGestureRecognizer;
    std::unique_ptr<GestureRecognizer> m_touchscreenGestureRecognizer;
    std::unique_ptr<StrokeGestures> m_strokeGestures;
};

struct KeyboardShortcut
{
    QKeySequence sequence;
    bool operator==(const KeyboardShortcut &) const = default;
};
struct PointerButtonShortcut
{
    Qt::KeyboardModifiers pointerModifiers;
    Qt::MouseButtons pointerButtons;
    bool operator==(const PointerButtonShortcut &) const = default;
};
struct PointerAxisShortcut
{
    Qt::KeyboardModifiers axisModifiers;
    PointerAxisDirection axisDirection;
    bool operator==(const PointerAxisShortcut &rhs) const = default;
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
struct StrokeShortcut
{
    Qt::KeyboardModifiers modifiers;
    QList<QPointF> points;
    bool operator==(const StrokeShortcut &rhs) const = default;
};

using Shortcut = std::variant<KeyboardShortcut, PointerButtonShortcut, PointerAxisShortcut, RealtimeFeedbackSwipeShortcut, RealtimeFeedbackPinchShortcut, StrokeShortcut>;

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
    StrokeGesture *strokeGesture() const;

private:
    std::shared_ptr<SwipeGesture> m_swipeGesture;
    std::shared_ptr<PinchGesture> m_pinchGesture;
    std::shared_ptr<StrokeGesture> m_strokeGesture;
    Shortcut m_shortcut = {};
    QAction *m_action = nullptr;
};

} // namespace
