/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/keystroke.h"

namespace KWin
{

KeyStroke::KeyStroke()
{
}

QList<uint32_t> KeyStroke::keys() const
{
    return m_keys;
}

bool KeyStroke::isEmpty() const
{
    return m_keys.isEmpty();
}

bool KeyStroke::contains(uint32_t key) const
{
    return m_keys.contains(key);
}

void KeyStroke::add(uint32_t key)
{
    if (!m_keys.contains(key)) {
        m_keys.append(key);
    }
}

bool KeyStroke::remove(uint32_t key)
{
    return m_keys.removeOne(key);
}

} // namespace KWin
