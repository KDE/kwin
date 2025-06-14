/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "qpainter/qpainterbackend.h"
#include "utils/damagejournal.h"

#include <QImage>
#include <QObject>
#include <chrono>

namespace KWin
{
class Output;
class GraphicsBufferAllocator;
class QPainterSwapchainSlot;
class QPainterSwapchain;

namespace Wayland
{
class WaylandBackend;
class WaylandDisplay;
class WaylandOutput;
class WaylandQPainterBackend;

class WaylandQPainterPrimaryLayer : public OutputLayer
{
public:
    WaylandQPainterPrimaryLayer(WaylandOutput *output, WaylandQPainterBackend *backend);
    ~WaylandQPainterPrimaryLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

    QRegion accumulateDamage(int bufferAge) const;

private:
    WaylandOutput *m_waylandOutput;
    WaylandQPainterBackend *m_backend;
    DamageJournal m_damageJournal;

    std::unique_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_back;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;

    friend class WaylandQPainterBackend;
};

class WaylandQPainterCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    WaylandQPainterCursorLayer(WaylandOutput *output, WaylandQPainterBackend *backend);
    ~WaylandQPainterCursorLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    WaylandQPainterBackend *m_backend;
    std::unique_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_back;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};

class WaylandQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(WaylandBackend *b);
    ~WaylandQPainterBackend() override;

    GraphicsBufferAllocator *graphicsBufferAllocator() const;

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
    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::map<Output *, Layers> m_outputs;
};

} // namespace Wayland
} // namespace KWin
