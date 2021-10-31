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

    QSharedPointer<DrmDumbBuffer> acquireBuffer(int *age = nullptr);
    QSharedPointer<DrmDumbBuffer> currentBuffer() const;
    void releaseBuffer(QSharedPointer<DrmDumbBuffer> buffer);

    qsizetype slotCount() const {
        return m_slots.count();
    }

    QSize size() const {
        return m_size;
    }

    bool isEmpty() const {
        return m_slots.isEmpty();
    }

private:
    struct Slot
    {
        QSharedPointer<DrmDumbBuffer> buffer;
        int age = 0;
    };

    QSize m_size;
    int index = 0;
    QVector<Slot> m_slots;
};

}
