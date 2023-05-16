/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"
#include "utils/damagejournal.h"

#include <QImage>
#include <QObject>

namespace KWin
{
class Output;
class ShmGraphicsBuffer;
class ShmGraphicsBufferAllocator;

namespace Wayland
{
class WaylandBackend;
class WaylandDisplay;
class WaylandOutput;
class WaylandQPainterBackend;

class WaylandQPainterBufferSlot
{
public:
    WaylandQPainterBufferSlot(ShmGraphicsBuffer *graphicsBuffer);
    ~WaylandQPainterBufferSlot();

    ShmGraphicsBuffer *graphicsBuffer;
    QImage image;
    void *data = nullptr;
    int size;
    int age = 0;
};

class WaylandQPainterSwapchain
{
public:
    WaylandQPainterSwapchain(const QSize &size, uint32_t format);

    QSize size() const;

    std::shared_ptr<WaylandQPainterBufferSlot> acquire();
    void release(std::shared_ptr<WaylandQPainterBufferSlot> buffer);

private:
    std::unique_ptr<ShmGraphicsBufferAllocator> m_allocator;
    QSize m_size;
    uint32_t m_format;
    std::vector<std::shared_ptr<WaylandQPainterBufferSlot>> m_slots;
};

class WaylandQPainterPrimaryLayer : public OutputLayer
{
public:
    WaylandQPainterPrimaryLayer(WaylandOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const RegionF &renderedRegion, const RegionF &damagedRegion) override;
    quint32 format() const override;

    void present();

    RegionF accumulateDamage(int bufferAge) const;

private:
    WaylandOutput *m_waylandOutput;
    DamageJournal m_damageJournal;

    std::unique_ptr<WaylandQPainterSwapchain> m_swapchain;
    std::shared_ptr<WaylandQPainterBufferSlot> m_back;

    friend class WaylandQPainterBackend;
};

class WaylandQPainterCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    explicit WaylandQPainterCursorLayer(WaylandOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const RegionF &renderedRegion, const RegionF &damagedRegion) override;
    quint32 format() const override;

private:
    WaylandOutput *m_output;
    std::unique_ptr<WaylandQPainterSwapchain> m_swapchain;
    std::shared_ptr<WaylandQPainterBufferSlot> m_back;
};

class WaylandQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(WaylandBackend *b);
    ~WaylandQPainterBackend() override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

private:
    void createOutput(Output *waylandOutput);

    struct Layers
    {
        std::unique_ptr<WaylandQPainterPrimaryLayer> primaryLayer;
        std::unique_ptr<WaylandQPainterCursorLayer> cursorLayer;
    };

    WaylandBackend *m_backend;
    std::map<Output *, Layers> m_outputs;
};

} // namespace Wayland
} // namespace KWin
