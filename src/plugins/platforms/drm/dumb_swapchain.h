/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVector>
#include <QSize>
#include <QSharedPointer>

namespace KWin
{

class DrmDumbBuffer;
class DrmGpu;

class DumbSwapchain
{
public:
    DumbSwapchain(DrmGpu *gpu, const QSize &size);

    QSharedPointer<DrmDumbBuffer> acquireBuffer();
    QSharedPointer<DrmDumbBuffer> currentBuffer() const;

    QSize size() const {
        return m_size;
    }

    bool isEmpty() const {
        return m_buffers.isEmpty();
    }

private:
    QSize m_size;

    int index = 0;
    QVector<QSharedPointer<DrmDumbBuffer>> m_buffers;
};

}
