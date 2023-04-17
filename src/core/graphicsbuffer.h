/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWin
{

struct DmaBufAttributes;

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

    /**
     * This enum type is used to specify the corner where the origin is. That is, the
     * buffer corner where 0,0 is located.
     */
    enum class Origin {
        TopLeft,
        BottomLeft,
    };

    bool isReferenced() const;
    bool isDropped() const;

    void ref();
    void unref();
    void drop();

    virtual QSize size() const = 0;
    virtual bool hasAlphaChannel() const = 0;
    virtual Origin origin() const = 0;

    virtual const DmaBufAttributes *dmabufAttributes() const;

Q_SIGNALS:
    void released();
    void dropped();

protected:
    int m_refCount = 0;
    bool m_dropped = false;
};

} // namespace KWin
