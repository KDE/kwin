/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"

#include <QImage>
#include <QObject>
#include <QVector>

#include <memory>

namespace KWin
{

class ShmGraphicsBuffer;
class ShmGraphicsBufferAllocator;
class X11WindowedBackend;
class X11WindowedOutput;

class X11WindowedQPainterLayerBuffer
{
public:
    X11WindowedQPainterLayerBuffer(ShmGraphicsBuffer *buffer, X11WindowedOutput *output);
    ~X11WindowedQPainterLayerBuffer();

    ShmGraphicsBuffer *graphicsBuffer() const;
    QImage *view() const;

private:
    ShmGraphicsBuffer *m_graphicsBuffer;
    void *m_data = nullptr;
    int m_size = 0;
    std::unique_ptr<QImage> m_view;
};

class X11WindowedQPainterLayerSwapchain
{
public:
    X11WindowedQPainterLayerSwapchain(const QSize &size, uint32_t format, X11WindowedOutput *output);

    QSize size() const;

    std::shared_ptr<X11WindowedQPainterLayerBuffer> acquire();

private:
    X11WindowedOutput *m_output;
    QSize m_size;
    uint32_t m_format;
    std::unique_ptr<ShmGraphicsBufferAllocator> m_allocator;
    QVector<std::shared_ptr<X11WindowedQPainterLayerBuffer>> m_buffers;
};

class X11WindowedQPainterPrimaryLayer : public OutputLayer
{
public:
    explicit X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    quint32 format() const override;

    void present();

private:
    X11WindowedOutput *const m_output;
    std::unique_ptr<X11WindowedQPainterLayerSwapchain> m_swapchain;
    std::shared_ptr<X11WindowedQPainterLayerBuffer> m_current;
};

class X11WindowedQPainterCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    explicit X11WindowedQPainterCursorLayer(X11WindowedOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    quint32 format() const override;

private:
    QImage m_buffer;
    X11WindowedOutput *m_output;
};

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend() override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

private:
    void addOutput(Output *output);
    void removeOutput(Output *output);

    struct Layers
    {
        std::unique_ptr<X11WindowedQPainterPrimaryLayer> primaryLayer;
        std::unique_ptr<X11WindowedQPainterCursorLayer> cursorLayer;
    };

    X11WindowedBackend *m_backend;
    std::map<Output *, Layers> m_outputs;
};

} // namespace KWin
