/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QObject>
#include <QSize>
#include <utility>

namespace KWin
{

class SyncReleasePoint;

struct DmaBufAttributes
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;

    std::array<FileDescriptor, 4> fd;
    std::array<uint32_t, 4> offset{0, 0, 0, 0};
    std::array<uint32_t, 4> pitch{0, 0, 0, 0};
};

struct ShmAttributes
{
    FileDescriptor fd;
    int stride;
    off_t offset;
    QSize size;
    uint32_t format;
};

/**
 * The GraphicsBuffer class represents a chunk of memory containing graphics data.
 *
 * A graphics buffer can be referenced. In which case, it won't be destroyed until all
 * references are dropped. You can use the isDropped() function to check whether the
 * buffer has been marked as destroyed.
 */
class KWIN_EXPORT GraphicsBuffer : public QObject
{
    Q_OBJECT

public:
    explicit GraphicsBuffer(QObject *parent = nullptr);

    bool isReferenced() const;
    bool isDropped() const;

    void ref();
    void unref();
    void drop();

    enum MapFlag {
        Read = 0x1,
        Write = 0x2,
    };
    Q_DECLARE_FLAGS(MapFlags, MapFlag)

    struct Map
    {
        void *data = nullptr;
        uint32_t stride = 0;
    };
    virtual Map map(MapFlags flags);
    virtual void unmap();

    virtual QSize size() const = 0;
    virtual bool hasAlphaChannel() const = 0;

    virtual const DmaBufAttributes *dmabufAttributes() const;
    virtual const ShmAttributes *shmAttributes() const;

    /**
     * the added release point will be referenced as long as this buffer is referenced
     */
    void addReleasePoint(const std::shared_ptr<SyncReleasePoint> &releasePoint);

    static bool alphaChannelFromDrmFormat(uint32_t format);

Q_SIGNALS:
    void released();

protected:
    int m_refCount = 0;
    bool m_dropped = false;
    std::vector<std::shared_ptr<SyncReleasePoint>> m_releasePoints;
};

/**
 * The GraphicsBufferRef type holds a reference to a GraphicsBuffer. While the reference
 * exists, the graphics buffer cannot be destroyed and the client cannnot modify it.
 */
class GraphicsBufferRef
{
public:
    GraphicsBufferRef()
        : m_buffer(nullptr)
    {
    }

    GraphicsBufferRef(GraphicsBuffer *buffer)
        : m_buffer(buffer)
    {
        if (m_buffer) {
            m_buffer->ref();
        }
    }

    GraphicsBufferRef(const GraphicsBufferRef &other)
        : m_buffer(other.m_buffer)
    {
        if (m_buffer) {
            m_buffer->unref();
        }
    }

    GraphicsBufferRef(GraphicsBufferRef &&other)
        : m_buffer(std::exchange(other.m_buffer, nullptr))
    {
    }

    ~GraphicsBufferRef()
    {
        if (m_buffer) {
            m_buffer->unref();
        }
    }

    GraphicsBufferRef &operator=(const GraphicsBufferRef &other)
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

    GraphicsBufferRef &operator=(GraphicsBufferRef &&other)
    {
        if (m_buffer) {
            m_buffer->unref();
        }
        m_buffer = std::exchange(other.m_buffer, nullptr);
        return *this;
    }

    GraphicsBufferRef &operator=(GraphicsBuffer *buffer)
    {
        if (m_buffer != buffer) {
            if (m_buffer) {
                m_buffer->unref();
            }
            if (buffer) {
                buffer->ref();
            }
            m_buffer = buffer;
        }
        return *this;
    }

    inline GraphicsBuffer *buffer() const
    {
        return m_buffer;
    }

    inline GraphicsBuffer *operator*() const
    {
        return m_buffer;
    }

    inline GraphicsBuffer *operator->() const
    {
        return m_buffer;
    }

    inline operator bool() const
    {
        return m_buffer;
    }

private:
    GraphicsBuffer *m_buffer;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::GraphicsBuffer::MapFlags)
