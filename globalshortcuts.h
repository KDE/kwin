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

    void setKGlobalAccelInterface(KGlobalAccelInterface *interface) {
        m_kglobalAccelInterface = interface;
    }

private:
    void objectDeleted(QObject *object);
    QHash<Qt::KeyboardModifiers, QHash<Qt::MouseButtons, GlobalShortcut*> > m_pointerShortcuts;
    QHash<Qt::KeyboardModifiers, QHash<PointerAxisDirection, GlobalShortcut*> > m_axisShortcuts;
    QHash<Qt::KeyboardModifiers, QHash<SwipeDirection, GlobalShortcut*> > m_swipeShortcuts;
    KGlobalAccelD *m_kglobalAccel = nullptr;
    KGlobalAccelInterface *m_kglobalAccelInterface = nullptr;
    GestureRecognizer *m_gestureRecognizer;
};

class GlobalShortcut
{
public:
    virtual ~GlobalShortcut();

    const QKeySequence &shortcut() const;
    Qt::KeyboardModifiers pointerButtonModifiers() const;
    Qt::MouseButtons pointerButtons() const;
    SwipeDirection swipeDirection() const {
        return m_swipeDirection;
    }
    virtual void invoke() = 0;

protected:
    GlobalShortcut(const QKeySequence &shortcut);
    GlobalShortcut(Qt::KeyboardModifiers pointerButtonModifiers, Qt::MouseButtons pointerButtons);
    GlobalShortcut(Qt::KeyboardModifiers axisModifiers);
    GlobalShortcut(SwipeDirection direction);

private:
    QKeySequence m_shortcut;
    Qt::KeyboardModifiers m_pointerModifiers;
    Qt::MouseButtons m_pointerButtons;
    SwipeDirection m_swipeDirection = SwipeDirection::Invalid;;
};

class InternalGlobalShortcut : public GlobalShortcut
{
public:
    InternalGlobalShortcut(Qt::KeyboardModifiers modifiers, const QKeySequence &shortcut, QAction *action);
    InternalGlobalShortcut(Qt::KeyboardModifiers pointerButtonModifiers, Qt::MouseButtons pointerButtons, QAction *action);
    InternalGlobalShortcut(Qt::KeyboardModifiers axisModifiers, PointerAxisDirection axis, QAction *action);
    InternalGlobalShortcut(Qt::KeyboardModifiers swipeModifier, SwipeDirection direction, QAction *action);
    ~InternalGlobalShortcut() override;

    void invoke() override;

    QAction *action() const;

    SwipeGesture *swipeGesture() const {
        return m_swipe.data();
    }
private:
    QAction *m_action;
    QScopedPointer<SwipeGesture> m_swipe;
};

inline
QAction *InternalGlobalShortcut::action() const
{
    return m_action;
}

inline
const QKeySequence &GlobalShortcut::shortcut() const
{
    return m_shortcut;
}

inline
Qt::KeyboardModifiers GlobalShortcut::pointerButtonModifiers() const
{
    return m_pointerModifiers;
}

inline
Qt::MouseButtons GlobalShortcut::pointerButtons() const
{
    return m_pointerButtons;
}

} // namespace

#endif
