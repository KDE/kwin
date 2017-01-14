/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_KEYBOARD_INPUT_H
#define KWIN_KEYBOARD_INPUT_H

#include "input.h"

#include <QObject>
#include <QPointer>
#include <QPointF>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KWIN_XKB)

class QWindow;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;
typedef uint32_t xkb_mod_index_t;
typedef uint32_t xkb_led_index_t;
typedef uint32_t xkb_keysym_t;

namespace KWin
{

class InputRedirection;
class KeyboardLayout;
class ModifiersChangedSpy;
class Toplevel;

namespace LibInput
{
class Device;
}

class KWIN_EXPORT Xkb
{
public:
    Xkb(InputRedirection *input);
    ~Xkb();
    void reconfigure();

    void installKeymap(int fd, uint32_t size);
    void updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void updateKey(uint32_t key, InputRedirection::KeyboardKeyState state);
    xkb_keysym_t toKeysym(uint32_t key);
    xkb_keysym_t currentKeysym() const {
        return m_keysym;
    }
    QString toString(xkb_keysym_t keysym);
    Qt::Key toQtKey(xkb_keysym_t keysym) const;
    Qt::KeyboardModifiers modifiers() const;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const;
    bool shouldKeyRepeat(quint32 key) const;

    void switchToNextLayout();

    enum class LED {
        NumLock = 1 << 0,
        CapsLock = 1 << 1,
        ScrollLock = 1 << 2
    };
    Q_DECLARE_FLAGS(LEDs, LED)
    LEDs leds() const {
        return m_leds;
    }

    xkb_keymap *keymap() const {
        return m_keymap;
    }

    xkb_state *state() const {
        return m_state;
    }

    quint32 currentLayout() const {
        return m_currentLayout;
    }
    QString layoutName() const;

private:
    xkb_keymap *loadKeymapFromConfig();
    xkb_keymap *loadDefaultKeymap();
    void updateKeymap(xkb_keymap *keymap);
    void createKeymapFile();
    void updateModifiers();
    void updateConsumedModifiers(uint32_t key);
    InputRedirection *m_input;
    xkb_context *m_context;
    xkb_keymap *m_keymap;
    xkb_state *m_state;
    xkb_mod_index_t m_shiftModifier;
    xkb_mod_index_t m_capsModifier;
    xkb_mod_index_t m_controlModifier;
    xkb_mod_index_t m_altModifier;
    xkb_mod_index_t m_metaModifier;
    xkb_led_index_t m_numLock;
    xkb_led_index_t m_capsLock;
    xkb_led_index_t m_scrollLock;
    Qt::KeyboardModifiers m_modifiers;
    Qt::KeyboardModifiers m_consumedModifiers;
    xkb_keysym_t m_keysym;
    struct {
        uint pressCount = 0;
        Qt::KeyboardModifier modifier = Qt::NoModifier;
    } m_modOnlyShortcut;
    quint32 m_currentLayout = 0;

    struct {
        xkb_compose_table *table = nullptr;
        xkb_compose_state *state = nullptr;
    } m_compose;
    LEDs m_leds;
};

class KWIN_EXPORT KeyboardInputRedirection : public QObject
{
    Q_OBJECT
public:
    explicit KeyboardInputRedirection(InputRedirection *parent);
    virtual ~KeyboardInputRedirection();

    void init();

    void update();

    /**
     * @internal
     */
    void processKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    /**
     * @internal
     **/
    void processKeymapChange(int fd, uint32_t size);

    Xkb *xkb() const {
        return m_xkb.data();
    }
    Qt::KeyboardModifiers modifiers() const {
        return m_xkb->modifiers();
    }
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const {
        return m_xkb->modifiersRelevantForGlobalShortcuts();
    }

Q_SIGNALS:
    void ledsChanged(KWin::Xkb::LEDs);

private:
    InputRedirection *m_input;
    bool m_inited = false;
    QScopedPointer<Xkb> m_xkb;
    QMetaObject::Connection m_activeClientSurfaceChangedConnection;
    struct {
        quint32 key = 0;
        quint32 time = 0;
        QTimer *timer = nullptr;
    } m_keyRepeat;
    ModifiersChangedSpy *m_modifiersChangedSpy = nullptr;
    KeyboardLayout *m_keyboardLayout = nullptr;
};

inline
Qt::KeyboardModifiers Xkb::modifiers() const
{
    return m_modifiers;
}

}

Q_DECLARE_METATYPE(KWin::Xkb::LED)
Q_DECLARE_METATYPE(KWin::Xkb::LEDs)

#endif
