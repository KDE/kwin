/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QImage>
#include <QSize>
#include <QVector>
#include <memory>

namespace KWin
{

class DrmGpu;
class GraphicsBuffer;
class GraphicsBufferAllocator;
class GraphicsBufferView;

class DumbSwapchainSlot
{
public:
    DumbSwapchainSlot(GraphicsBuffer *buffer);
    ~DumbSwapchainSlot();

    GraphicsBuffer *buffer() const;
    GraphicsBufferView *view() const;
    int age() const;

private:
    GraphicsBuffer *m_buffer;
    std::unique_ptr<GraphicsBufferView> m_view;
    int m_age = 0;
    friend class DumbSwapchain;
};

class DumbSwapchain
{
public:
    DumbSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format);
    ~DumbSwapchain();

    QSize size() const;
    uint32_t format() const;

    std::shared_ptr<DumbSwapchainSlot> acquire();
    void release(std::shared_ptr<DumbSwapchainSlot> slot);

private:
    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    QVector<std::shared_ptr<DumbSwapchainSlot>> m_slots;
    QSize m_size;
    uint32_t m_format;
};

}
