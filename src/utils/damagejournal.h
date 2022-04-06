/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QList>
#include <QRegion>

namespace KWin
{

/**
 * The DamageJournal class is a helper that tracks last N damage regions.
 */
class KWIN_EXPORT DamageJournal
{
public:
    /**
     * Returns the maximum number of damage regions that can be stored in the journal.
     */
    int capacity() const
    {
        return m_capacity;
    }

    /**
     * Sets the maximum number of damage regions that can be stored in the journal
     * to @a capacity.
     */
    void setCapacity(int capacity)
    {
        m_capacity = capacity;
    }

    /**
     * Adds the specified @a region to the journal.
     */
    void add(const QRegion &region)
    {
        while (m_log.size() >= m_capacity) {
            m_log.takeLast();
        }
        m_log.prepend(region);
    }

    /**
     * Clears the damage journal. Typically, one would want to clear the damage journal
     * if a buffer swap fails for some reason.
     */
    void clear()
    {
        m_log.clear();
    }

    /**
     * Accumulates the damage regions in the log up to the specified @a bufferAge.
     *
     * If the specified buffer age value refers to a damage region older than the last
     * one in the journal, @a fallback will be returned.
     */
    QRegion accumulate(int bufferAge, const QRegion &fallback = QRegion()) const
    {
        QRegion region;
        if (bufferAge > 0 && bufferAge <= m_log.size()) {
            for (int i = 0; i < bufferAge - 1; ++i) {
                region |= m_log[i];
            }
        } else {
            region = fallback;
        }
        return region;
    }

    QRegion lastDamage() const
    {
        return m_log.first();
    }

private:
    QList<QRegion> m_log;
    int m_capacity = 10;
};

} // namespace KWin
