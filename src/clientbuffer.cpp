/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientbuffer.h"

namespace KWin
{

ClientBuffer::ClientBuffer(QObject *parent)
    : QObject(parent)
{
}

bool ClientBuffer::isDestroyed() const
{
    return m_isDestroyed;
}

bool ClientBuffer::isReferenced() const
{
    return m_refCount > 0;
}

void ClientBuffer::ref()
{
    m_refCount++;
}

void ClientBuffer::unref()
{
    Q_ASSERT_X(isReferenced(), "unref", "Buffer must be referenced");
    m_refCount--;

    if (!isReferenced()) {
        if (isDestroyed()) {
            delete this;
        } else {
            release();
        }
    }
}

void ClientBuffer::markAsDestroyed()
{
    if (isReferenced()) {
        m_isDestroyed = true;
    } else {
        delete this;
    }
}

} // namespace KWin
