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

struct DmaBufAttributes
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;

    FileDescriptor fd[4];
    uint32_t offset[4] = {0, 0, 0, 0};
    uint32_t pitch[4] = {0, 0, 0, 0};
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

    static bool alphaChannelFromDrmFormat(uint32_t format);

Q_SIGNALS:
    void released();

protected:
    int m_refCount = 0;
    bool m_dropped = false;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::GraphicsBuffer::MapFlags)
