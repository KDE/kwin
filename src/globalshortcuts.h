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
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QKeySequence>

#include <KCoreConfigSkeleton>
#include <memory>
#include <vector>

class QAction;
class KGlobalAccelD;
class KGlobalAccelInterface;

namespace KWin
{
class GestureShortcut;
class GlobalShortcut;
class GlobalShortcutsManager;

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

    void registerGesture(GestureDeviceType device, GestureDirections direction, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback = nullptr);

    void registerGesture(Gesture *gesture, GestureDeviceType device);
    void unregisterGesture(Gesture *gesture, GestureDeviceType device);

    void forceRegisterTouchscreenSwipe(QAction *action, std::function<void(qreal)> progressCallback, GestureDirection direction, uint fingerCount);

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

private:
    void objectDeleted(QObject *object);
    bool addIfNotExists(GlobalShortcut sc, GestureDeviceType device = GestureDeviceType::Touchpad);

    std::vector<GlobalShortcut> m_shortcuts;

    std::unique_ptr<KGlobalAccelD> m_kglobalAccel;

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
struct GestureShortcut
{
    GestureDeviceType device;
    GestureDirections direction;

    Gesture *gesture() const;
    std::unique_ptr<SwipeGesture> swipeGesture;
    std::unique_ptr<PinchGesture> pinchGesture;

    bool operator==(const GestureShortcut &shortcut) const
    {
        return device == shortcut.device && direction == shortcut.direction && gesture()->acceptableFingerCounts() == shortcut.gesture()->acceptableFingerCounts();
    }
};

using Shortcut = std::variant<KeyboardShortcut, PointerButtonShortcut, PointerAxisShortcut, GestureShortcut>;

class GlobalShortcut
{
public:
    GlobalShortcut(Shortcut &&shortcut, QAction *action);
    ~GlobalShortcut();

    GlobalShortcut(GlobalShortcut &&sc) = default;
    GlobalShortcut &operator=(GlobalShortcut &&sc) = default;

    void invoke() const;
    QAction *action() const;
    const Shortcut &shortcut() const;

private:
    Shortcut m_shortcut = {};
    QAction *m_action = nullptr;
};

} // namespace

#endif
