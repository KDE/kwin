/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_GLOBALSHORTCUTS_H
#define KWIN_GLOBALSHORTCUTS_H
// KWin
#include <kwinglobals.h>
// Qt
#include <QKeySequence>
#include <QSharedPointer>

class QAction;
class KGlobalAccelD;
class KGlobalAccelInterface;

namespace KWin
{
class GlobalShortcut;
class SwipeGesture;
class GestureRecognizer;

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

    void registerTouchpadSwipe(QAction *action, SwipeDirection direction);

    void registerRealtimeTouchpadSwipe(QAction *onUp, std::function<void(qreal)> progressCallback, SwipeDirection direction);

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

    void processSwipeStart(uint fingerCount);
    void processSwipeUpdate(const QSizeF &delta);
    void processSwipeCancel();
    void processSwipeEnd();

    void setKGlobalAccelInterface(KGlobalAccelInterface *interface)
    {
        m_kglobalAccelInterface = interface;
    }

private:
    void objectDeleted(QObject *object);
    bool addIfNotExists(GlobalShortcut sc);

    QVector<GlobalShortcut> m_shortcuts;

    KGlobalAccelD *m_kglobalAccel = nullptr;
    KGlobalAccelInterface *m_kglobalAccelInterface = nullptr;
    GestureRecognizer *m_gestureRecognizer;
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
struct FourFingerSwipeShortcut
{
    SwipeDirection swipeDirection;
    bool operator==(const FourFingerSwipeShortcut &rhs) const
    {
        return swipeDirection == rhs.swipeDirection;
    }
};
struct FourFingerRealtimeFeedbackSwipeShortcut
{
    SwipeDirection swipeDirection;
    std::function<void(qreal)> progressCallback;

    template<typename T>
    bool operator==(const T& rhs) const
    {
        return swipeDirection == rhs.swipeDirection;
    }
};

using Shortcut = std::variant<KeyboardShortcut, PointerButtonShortcut, PointerAxisShortcut, FourFingerSwipeShortcut, FourFingerRealtimeFeedbackSwipeShortcut>;

class GlobalShortcut
{
public:
    GlobalShortcut(Shortcut &&shortcut, QAction *action);
    ~GlobalShortcut();

    void invoke() const;
    QAction *action() const;
    const Shortcut &shortcut() const;
    SwipeGesture *swipeGesture() const;

private:
    QSharedPointer<SwipeGesture> m_gesture;
    Shortcut m_shortcut = {};
    QAction *m_action = nullptr;
};

} // namespace

#endif
