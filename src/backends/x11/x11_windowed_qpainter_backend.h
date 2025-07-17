/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "qpainter/qpainterbackend.h"

#include <QImage>
#include <QList>
#include <QObject>

#include <chrono>
#include <memory>

namespace KWin
{

class GraphicsBufferAllocator;
class QPainterSwapchainSlot;
class QPainterSwapchain;
class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedQPainterBackend;

class X11WindowedQPainterPrimaryLayer : public OutputLayer
{
public:
    X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output, X11WindowedQPainterBackend *backend);
    ~X11WindowedQPainterPrimaryLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    X11WindowedOutput *const m_output;
    X11WindowedQPainterBackend *const m_backend;
    std::unique_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_current;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};

class X11WindowedQPainterCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    explicit X11WindowedQPainterCursorLayer(X11WindowedOutput *output);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    QImage m_buffer;
    X11WindowedOutput *m_output;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend() override;

    GraphicsBufferAllocator *graphicsBufferAllocator() const;

    QList<OutputLayer *> compatibleOutputLayers(Output *output) override;

private:
    void addOutput(Output *output);
    void removeOutput(Output *output);

    struct Layers
    {
        std::unique_ptr<X11WindowedQPainterPrimaryLayer> primaryLayer;
        std::unique_ptr<X11WindowedQPainterCursorLayer> cursorLayer;
    };

    X11WindowedBackend *m_backend;
    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::map<Output *, Layers> m_outputs;
};

} // namespace KWin
