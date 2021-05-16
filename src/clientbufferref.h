/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

namespace KWin
{

class ClientBuffer;

class KWIN_EXPORT ClientBufferRef
{
public:
    ClientBufferRef();
    ClientBufferRef(ClientBufferRef &&other);
    ClientBufferRef(const ClientBufferRef &other);
    ClientBufferRef(ClientBuffer *buffer);
    ~ClientBufferRef();

    ClientBufferRef &operator=(ClientBufferRef &&other);
    ClientBufferRef &operator=(const ClientBufferRef &other);
    ClientBufferRef &operator=(ClientBuffer *buffer);

    bool operator==(const ClientBufferRef &other) const;
    bool operator!=(const ClientBufferRef &other) const;
    operator bool() const;

    /**
     * @internal
     *
     * Returns the reference to the concrete implementation of the client buffer. If the buffer
     * reference is valid, this function can never return @c null.
     */
    ClientBuffer *internalBuffer() const;

    /**
     * Returns @c true if the reference doesn't reference any buffer; otherwise returns @c false.
     */
    bool isNull() const;

    /**
     * Returns @c true if the client buffer has an alpha channel; otherwise returns @c false.
     */
    bool hasAlphaChannel() const;

    /**
     * Returns the size of the client buffer, in native pixels.
     *
     * Note that the returned value is not affected by the buffer scale or viewporter transform.
     */
    QSize size() const;

private:
    ClientBuffer *m_buffer = nullptr;
};

} // namespace KWin
