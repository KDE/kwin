/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_KEYBOARD_INPUT_H
#define KWIN_KEYBOARD_INPUT_H

#include "input.h"
#include "xkb.h"

#include <QObject>
#include <QPointer>
#include <QPointF>

#include <KSharedConfig>

class QWindow;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;
typedef uint32_t xkb_mod_index_t;
typedef uint32_t xkb_led_index_t;
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_layout_index_t;

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

class KWIN_EXPORT KeyboardInputRedirection : public QObject
{
    Q_OBJECT
public:
    explicit KeyboardInputRedirection(InputRedirection *parent);
    ~KeyboardInputRedirection() override;

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
     */
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
    ModifiersChangedSpy *m_modifiersChangedSpy = nullptr;
    KeyboardLayout *m_keyboardLayout = nullptr;
};

}

#endif
