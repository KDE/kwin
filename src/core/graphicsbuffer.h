/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QObject>
#include <QSize>
#include <mutex>
#include <set>
#include <sys/types.h>
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
    dev_t device;

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

struct SinglePixelAttributes
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t alpha;
};

/**
 * The GraphicsBuffer class represents a chunk of memory containing graphics data.
 *
 * A graphics buffer can be referenced. In which case, it won't be destroyed until all
 * references are dropped. You can use the isDropped() function to check whether the
 * buffer has been marked as destroyed.
 */
class KWIN_EXPORT GraphicsBuffer : public std::enable_shared_from_this<GraphicsBuffer>
{
public:
    explicit GraphicsBuffer();
    virtual ~GraphicsBuffer();

    /**
     * NOTE this assumes that once the buffer is no longer referenced,
     * only the caller of this method will reference it again.
     * Otherwise, calling this may be racy with multi-threading.
     */
    bool isReferenced() const;

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

    virtual const DmaBufAttributes *udmabufAttributes() const;
    virtual const DmaBufAttributes *dmabufAttributes() const;
    virtual const ShmAttributes *shmAttributes() const;
    virtual const SinglePixelAttributes *singlePixelAttributes() const;

    /**
     * the added release point will be referenced as long as this buffer is referenced
     */
    void addReleasePoint(const std::shared_ptr<SyncReleasePoint> &releasePoint);

    class AttachedResource
    {
    public:
        explicit AttachedResource(GraphicsBuffer *buffer);
        virtual ~AttachedResource();

        void bufferDeleted();

        GraphicsBuffer *m_buffer;

    private:
        virtual void handleBufferDeleted();

        std::weak_ptr<GraphicsBuffer> m_bufferRef;
    };
    void attachResource(AttachedResource *resource);
    void detachResource(AttachedResource *resource);

    static bool alphaChannelFromDrmFormat(uint32_t format);

protected:
    friend class GraphicsBufferRef;

    void ref();
    void unref();

    /**
     * called the first time the buffer is referenced after it's created or released
     */
    virtual void referenced();
    virtual void released();

    std::atomic<int> m_references = 0;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<SyncReleasePoint>> m_releasePoints;
    std::set<AttachedResource *> m_resources;
};

/**
 * The GraphicsBufferRef type holds a reference to a GraphicsBuffer. While the reference
 * exists, the graphics buffer cannot be destroyed and the client cannot modify it.
 */
class GraphicsBufferRef
{
public:
    GraphicsBufferRef()
        : m_buffer(nullptr)
    {
    }

    GraphicsBufferRef(GraphicsBuffer *buffer)
        : GraphicsBufferRef(buffer ? buffer->shared_from_this() : nullptr)
    {
    }

    GraphicsBufferRef(const std::weak_ptr<GraphicsBuffer> &buffer)
        : GraphicsBufferRef(buffer.lock())
    {
    }

    GraphicsBufferRef(const std::shared_ptr<GraphicsBuffer> &buffer)
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
            m_buffer->ref();
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

    void reset()
    {
        if (m_buffer) {
            m_buffer->unref();
            m_buffer.reset();
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
        return operator=(buffer ? buffer->shared_from_this() : nullptr);
    }

    GraphicsBufferRef &operator=(const std::weak_ptr<GraphicsBuffer> &buffer)
    {
        return operator=(buffer.lock());
    }

    GraphicsBufferRef &operator=(const std::shared_ptr<GraphicsBuffer> &buffer)
    {
        if (m_buffer != buffer) {
            if (buffer) {
                buffer->ref();
            }
            if (m_buffer) {
                m_buffer->unref();
            }
            m_buffer = buffer;
        }
        return *this;
    }

    inline GraphicsBuffer *buffer() const
    {
        return m_buffer.get();
    }

    inline GraphicsBuffer *operator*() const
    {
        return m_buffer.get();
    }

    inline GraphicsBuffer *operator->() const
    {
        return m_buffer.get();
    }

    inline operator bool() const
    {
        return m_buffer != nullptr;
    }

private:
    std::shared_ptr<GraphicsBuffer> m_buffer;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::GraphicsBuffer::MapFlags)
