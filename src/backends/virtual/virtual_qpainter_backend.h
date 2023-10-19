/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <chrono>
#include <memory>

namespace KWin
{

class GraphicsBufferAllocator;
class QPainterSwapchainSlot;
class QPainterSwapchain;
class VirtualBackend;
class VirtualQPainterBackend;

class VirtualQPainterLayer : public OutputLayer
{
public:
    VirtualQPainterLayer(Output *output, VirtualQPainterBackend *backend);
    ~VirtualQPainterLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    QImage *image();
    quint32 format() const override;
    std::chrono::nanoseconds queryRenderTime() const override;

private:
    Output *const m_output;
    VirtualQPainterBackend *const m_backend;
    std::unique_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_current;
    std::chrono::steady_clock::time_point m_renderStart;
    std::chrono::nanoseconds m_renderTime = std::chrono::nanoseconds::zero();
};

class VirtualQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    VirtualQPainterBackend(VirtualBackend *backend);
    ~VirtualQPainterBackend() override;

    GraphicsBufferAllocator *graphicsBufferAllocator() const override;

    void present(Output *output) override;
    VirtualQPainterLayer *primaryLayer(Output *output) override;

private:
    void addOutput(Output *output);
    void removeOutput(Output *output);

    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::map<Output *, std::unique_ptr<VirtualQPainterLayer>> m_outputs;
};

} // namespace KWin
