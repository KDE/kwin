/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_INPUT_H
#define KWIN_INPUT_H
#include <kwinglobals.h>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QEvent>
#include <QWeakPointer>
#include <config-kwin.h>
#if HAVE_XKB
#include <xkbcommon/xkbcommon.h>
#endif

namespace KWin
{
class Toplevel;
class Xkb;

/**
 * @brief This class is responsible for redirecting incoming input to the surface which currently
 * has input or send enter/leave events.
 *
 * In addition input is intercepted before passed to the surfaces to have KWin internal areas
 * getting input first (e.g. screen edges) and filter the input event out if we currently have
 * a full input grab.
 *
 */
class InputRedirection : public QObject
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
    enum KeyboardKeyState {
        KeyboardKeyReleased,
        KeyboardKeyPressed
    };
    virtual ~InputRedirection();

    /**
     * @return const QPointF& The current global pointer position
     */
    const QPointF &globalPointer() const;
    /**
     * @brief The last known state of the @p button. If @p button is still unknown the state is
     * @c PointerButtonReleased.
     *
     * @param button The button for which the last known state should be queried.
     * @return KWin::InputRedirection::PointerButtonState
     */
    PointerButtonState pointerButtonState(uint32_t button) const;
    Qt::MouseButtons qtButtonStates() const;
    Qt::KeyboardModifiers keyboardModifiers() const;

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
    void processPointerAxis(PointerAxis axis, qreal delta, uint32_t time);
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
     **/
    void processKeymapChange(int fd, uint32_t size);

    static uint8_t toXPointerButton(uint32_t button);
    static uint8_t toXPointerButton(PointerAxis axis, qreal delta);

public Q_SLOTS:
    void updatePointerWindow();

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

private:
    static QEvent::Type buttonStateToEvent(PointerButtonState state);
    static Qt::MouseButton buttonToQtMouseButton(uint32_t button);
    Toplevel *findToplevel(const QPoint &pos);
    QPointF m_globalPointer;
    QHash<uint32_t, PointerButtonState> m_pointerButtons;
#if HAVE_XKB
    QScopedPointer<Xkb> m_xkb;
#endif
    /**
     * @brief The Toplevel which currently receives pointer events
     */
    QWeakPointer<Toplevel> m_pointerWindow;

    KWIN_SINGLETON(InputRedirection)
    friend InputRedirection *input();
};

#if HAVE_XKB
class Xkb
{
public:
    Xkb();
    ~Xkb();
    void installKeymap(int fd, uint32_t size);
    void updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void updateKey(uint32_t key, InputRedirection::KeyboardKeyState state);
    xkb_keysym_t toKeysym(uint32_t key);
    QString toString(xkb_keysym_t keysym);
    Qt::Key toQtKey(xkb_keysym_t keysym);
    Qt::KeyboardModifiers modifiers() const;
private:
    xkb_context *m_context;
    xkb_keymap *m_keymap;
    xkb_state *m_state;
    xkb_mod_index_t m_shiftModifier;
    xkb_mod_index_t m_controlModifier;
    xkb_mod_index_t m_altModifier;
    xkb_mod_index_t m_metaModifier;
    Qt::KeyboardModifiers m_modifiers;
};
#endif

inline
InputRedirection *input()
{
    return InputRedirection::s_self;
}

inline
const QPointF &InputRedirection::globalPointer() const
{
    return m_globalPointer;
}

inline
InputRedirection::PointerButtonState InputRedirection::pointerButtonState(uint32_t button) const
{
    auto it = m_pointerButtons.constFind(button);
    if (it != m_pointerButtons.constEnd()) {
        return it.value();
    } else {
        return KWin::InputRedirection::PointerButtonReleased;
    }
}

#if HAVE_XKB
inline
Qt::KeyboardModifiers Xkb::modifiers() const
{
    return m_modifiers;
}
#endif

} // namespace KWin

#endif // KWIN_INPUT_H
