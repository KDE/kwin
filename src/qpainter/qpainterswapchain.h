/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QImage>
#include <QList>
#include <QSize>

#include <memory>

namespace KWin
{

class GraphicsBuffer;
class GraphicsBufferAllocator;
class GraphicsBufferView;

class KWIN_EXPORT QPainterSwapchainSlot
{
public:
    QPainterSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<GraphicsBufferView> &&view);
    ~QPainterSwapchainSlot();

    GraphicsBuffer *buffer() const;
    GraphicsBufferView *view() const;
    int age() const;

    static std::shared_ptr<QPainterSwapchainSlot> create(GraphicsBuffer *buffer);

private:
    GraphicsBuffer *m_buffer;
    std::unique_ptr<GraphicsBufferView> m_view;
    int m_age = 0;
    friend class QPainterSwapchain;
};

class KWIN_EXPORT QPainterSwapchain
{
public:
    QPainterSwapchain(GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format);
    ~QPainterSwapchain();

    QSize size() const;
    uint32_t format() const;

    std::shared_ptr<QPainterSwapchainSlot> acquire();
    void release(std::shared_ptr<QPainterSwapchainSlot> slot);

private:
    GraphicsBufferAllocator *m_allocator;
    QList<std::shared_ptr<QPainterSwapchainSlot>> m_slots;
    QSize m_size;
    uint32_t m_format;
};

}
