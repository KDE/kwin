/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QList>

namespace KWin
{

class KWIN_EXPORT KeyStroke
{
public:
    KeyStroke();

    /**
     * Returns all pressed keys in this key stroke.
     */
    QList<uint32_t> keys() const;

    /**
     * Returns @c true if the key stroke has no pressed keys; otherwise returns @c false.
     */
    bool isEmpty() const;

    /**
     * Adds the specified @a key to the key stroke.
     */
    void add(uint32_t key);

    /**
     * Removes the specified @a key from the key stroke. Returns @c true if any key has been removed;
     * otherwise returns @c false.
     */
    bool remove(uint32_t key);

    /**
     * Returns @c true if the key stroke contains the specified @a key; otherwise returns @c false.
     */
    bool contains(uint32_t key) const;

private:
    QList<uint32_t> m_keys;
};

} // namespace KWin
