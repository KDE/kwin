/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientbufferref.h"
#include "clientbuffer.h"

namespace KWin
{

ClientBufferRef::ClientBufferRef()
{
}

ClientBufferRef::ClientBufferRef(ClientBufferRef &&other)
    : m_buffer(std::exchange(other.m_buffer, nullptr))
{
}

ClientBufferRef::ClientBufferRef(const ClientBufferRef &other)
{
    m_buffer = other.m_buffer;
    if (m_buffer) {
        m_buffer->ref();
    }
}

ClientBufferRef::ClientBufferRef(ClientBuffer *buffer)
{
    m_buffer = buffer;
    if (m_buffer) {
        m_buffer->ref();
    }
}

ClientBufferRef::~ClientBufferRef()
{
    if (m_buffer) {
        m_buffer->unref();
    }
}

ClientBuffer *ClientBufferRef::internalBuffer() const
{
    return m_buffer;
}

bool ClientBufferRef::isNull() const
{
    return !m_buffer;
}

bool ClientBufferRef::hasAlphaChannel() const
{
    if (m_buffer) {
        return m_buffer->hasAlphaChannel();
    }
    return false;
}

QSize ClientBufferRef::size() const
{
    if (m_buffer) {
        return m_buffer->size();
    }
    return QSize();
}

ClientBufferRef &ClientBufferRef::operator=(ClientBufferRef &&other)
{
    if (this != &other) {
        if (m_buffer) {
            m_buffer->unref();
        }
        m_buffer = std::exchange(other.m_buffer, nullptr);
    }
    return *this;
}

ClientBufferRef &ClientBufferRef::operator=(const ClientBufferRef &other)
{
    if (other.m_buffer) {
        other.m_buffer->ref();
    }
    if (m_buffer) {
        m_buffer->unref();
    }
    m_buffer = other.m_buffer;
    return *this;
}

ClientBufferRef &ClientBufferRef::operator=(ClientBuffer *buffer)
{
    if (buffer) {
        buffer->ref();
    }
    if (m_buffer) {
        m_buffer->unref();
    }
    m_buffer = buffer;
    return *this;
}

bool ClientBufferRef::operator==(const ClientBufferRef &other) const
{
    return m_buffer == other.m_buffer;
}

bool ClientBufferRef::operator!=(const ClientBufferRef &other) const
{
    return !(*this == other);
}

ClientBufferRef::operator bool() const
{
    return m_buffer;
}

} // namespace KWin
