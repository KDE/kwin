/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"

#include <QMap>
#include <QObject>
#include <QVector>
#include <memory>

namespace KWin
{

class GraphicsBuffer;
class ShmGraphicsBufferAllocator;
class VirtualBackend;
class VirtualQPainterBackend;

class VirtualQPainterBufferSlot
{
public:
    VirtualQPainterBufferSlot(GraphicsBuffer *graphicsBuffer);
    ~VirtualQPainterBufferSlot();

    GraphicsBuffer *graphicsBuffer;
    QImage image;
    void *data = nullptr;
    int size;
};

class VirtualQPainterSwapchain
{
public:
    VirtualQPainterSwapchain(const QSize &size, uint32_t format, VirtualQPainterBackend *backend);

    QSize size() const;

    std::shared_ptr<VirtualQPainterBufferSlot> acquire();

private:
    VirtualQPainterBackend *m_backend;
    QSize m_size;
    uint32_t m_format;
    std::vector<std::shared_ptr<VirtualQPainterBufferSlot>> m_slots;
};

class VirtualQPainterLayer : public OutputLayer
{
public:
    VirtualQPainterLayer(Output *output, VirtualQPainterBackend *backend);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    QImage *image();
    quint32 format() const override;

private:
    VirtualQPainterBackend *const m_backend;
    Output *const m_output;
    std::unique_ptr<VirtualQPainterSwapchain> m_swapchain;
    std::shared_ptr<VirtualQPainterBufferSlot> m_current;
};

class VirtualQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    VirtualQPainterBackend(VirtualBackend *backend);
    ~VirtualQPainterBackend() override;

    void present(Output *output) override;
    VirtualQPainterLayer *primaryLayer(Output *output) override;
    GraphicsBufferAllocator *graphicsBufferAllocator() const override;

private:
    void addOutput(Output *output);
    void removeOutput(Output *output);

    std::unique_ptr<ShmGraphicsBufferAllocator> m_allocator;
    std::map<Output *, std::unique_ptr<VirtualQPainterLayer>> m_outputs;
};

} // namespace KWin
