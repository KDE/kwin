/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbuffer.h"

namespace KWin
{

GraphicsBuffer::GraphicsBuffer(QObject *parent)
    : QObject(parent)
{
}

bool GraphicsBuffer::isReferenced() const
{
    return m_refCount > 0;
}

bool GraphicsBuffer::isDropped() const
{
    return m_dropped;
}

void GraphicsBuffer::ref()
{
    ++m_refCount;
}

void GraphicsBuffer::unref()
{
    Q_ASSERT(m_refCount > 0);
    --m_refCount;
    if (!m_refCount) {
        if (m_dropped) {
            delete this;
        } else {
            Q_EMIT released();
        }
    }
}

void GraphicsBuffer::drop()
{
    m_dropped = true;
    Q_EMIT dropped();

    if (!m_refCount) {
        delete this;
    }
}

} // namespace KWin
