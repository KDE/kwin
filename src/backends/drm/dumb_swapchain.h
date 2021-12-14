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
#include <QImage>

namespace KWin
{

class DrmDumbBuffer;
class DrmGpu;

class DumbSwapchain
{
public:
    DumbSwapchain(DrmGpu *gpu, const QSize &size, uint32_t drmFormat, QImage::Format imageFormat = QImage::Format_RGB32);

    QSharedPointer<DrmDumbBuffer> acquireBuffer(int *age = nullptr);
    QSharedPointer<DrmDumbBuffer> currentBuffer() const;
    void releaseBuffer(QSharedPointer<DrmDumbBuffer> buffer);

    qsizetype slotCount() const;
    QSize size() const;
    bool isEmpty() const;
    uint32_t drmFormat() const;

private:
    struct Slot
    {
        QSharedPointer<DrmDumbBuffer> buffer;
        int age = 0;
    };

    QSize m_size;
    int index = 0;
    uint32_t m_format;
    QVector<Slot> m_slots;
};

}
