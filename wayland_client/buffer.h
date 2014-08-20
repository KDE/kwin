/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_BUFFER_H
#define KWIN_WAYLAND_BUFFER_H

#include <QSize>

#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

class ShmPool;

class Buffer
{
public:
    ~Buffer();
    void copy(const void *src);
    void setReleased(bool released);
    void setUsed(bool used);

    wl_buffer *buffer() const;
    const QSize &size() const;
    int32_t stride() const;
    bool isReleased() const;
    bool isUsed() const;
    uchar *address();

    operator wl_buffer*() {
        return m_nativeBuffer;
    }
    operator wl_buffer*() const {
        return m_nativeBuffer;
    }

    static void releasedCallback(void *data, wl_buffer *wl_buffer);

private:
    friend class ShmPool;
    explicit Buffer(ShmPool *parent, wl_buffer *buffer, const QSize &size, int32_t stride, size_t offset);
    static const struct wl_buffer_listener s_listener;
    ShmPool *m_shm;
    wl_buffer *m_nativeBuffer;
    bool m_released;
    QSize m_size;
    int32_t m_stride;
    size_t m_offset;
    bool m_used;
};

inline
wl_buffer *Buffer::buffer() const
{
    return m_nativeBuffer;
}

inline
const QSize &Buffer::size() const
{
    return m_size;
}

inline
int32_t Buffer::stride() const
{
    return m_stride;
}

inline
bool Buffer::isReleased() const
{
    return m_released;
}

inline
void Buffer::setReleased(bool released)
{
    m_released = released;
}

inline
bool Buffer::isUsed() const
{
    return m_used;
}

inline
void Buffer::setUsed(bool used)
{
    m_used = used;
}

}
}

#endif
