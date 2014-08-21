/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "keyboard.h"

namespace KWin
{
namespace Wayland
{

const wl_keyboard_listener Keyboard::s_listener = {
    keymapCallback,
    enterCallback,
    leaveCallback,
    keyCallback,
    modifiersCallback
};

Keyboard::Keyboard(QObject *parent)
    : QObject(parent)
    , m_keyboard(nullptr)
{
}

Keyboard::~Keyboard()
{
    release();
}

void Keyboard::release()
{
    if (!m_keyboard) {
        return;
    }
    wl_keyboard_destroy(m_keyboard);
    m_keyboard = nullptr;
}

void Keyboard::setup(wl_keyboard *keyboard)
{
    Q_ASSERT(keyboard);
    Q_ASSERT(!m_keyboard);
    m_keyboard = keyboard;
    wl_keyboard_add_listener(m_keyboard, &Keyboard::s_listener, this);
}

void Keyboard::enterCallback(void *data, wl_keyboard *keyboard, uint32_t serial, wl_surface *surface, wl_array *keys)
{
    // ignore
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    Q_UNUSED(surface)
    Q_UNUSED(keys)
}

void Keyboard::leaveCallback(void *data, wl_keyboard *keyboard, uint32_t serial, wl_surface *surface)
{
    // ignore
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    Q_UNUSED(surface)
}

void Keyboard::keyCallback(void *data, wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_UNUSED(serial)
    Keyboard *k = reinterpret_cast<Keyboard*>(data);
    Q_ASSERT(k->m_keyboard == keyboard);
    auto toState = [state] {
        if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
            return KeyState::Released;
        } else {
            return KeyState::Pressed;
        }
    };
    emit k->keyChanged(key, toState(), time);
}

void Keyboard::keymapCallback(void *data, wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    Keyboard *k = reinterpret_cast<Keyboard*>(data);
    Q_ASSERT(k->m_keyboard == keyboard);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        return;
    }
    emit k->keymapChanged(fd, size);
}

void Keyboard::modifiersCallback(void *data, wl_keyboard *keyboard, uint32_t serial, uint32_t modsDepressed,
                                 uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    Q_UNUSED(serial)
    Keyboard *k = reinterpret_cast<Keyboard*>(data);
    Q_ASSERT(k->m_keyboard == keyboard);
    emit k->modifiersChanged(modsDepressed, modsLatched, modsLocked, group);
}

}
}
