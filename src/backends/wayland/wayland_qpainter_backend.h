/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "qpainterbackend.h"
#include "utils/damagejournal.h"

#include <KWayland/Client/buffer.h>

#include <QImage>
#include <QObject>
#include <QWeakPointer>

namespace KWayland
{
namespace Client
{
class ShmPool;
class Buffer;
}
}

namespace KWin
{
class Output;
namespace Wayland
{
class WaylandBackend;
class WaylandOutput;
class WaylandQPainterBackend;

class WaylandQPainterBufferSlot
{
public:
    WaylandQPainterBufferSlot(QSharedPointer<KWayland::Client::Buffer> buffer);
    ~WaylandQPainterBufferSlot();

    QSharedPointer<KWayland::Client::Buffer> buffer;
    QImage image;
    int age = 0;
};

class WaylandQPainterPrimaryLayer : public OutputLayer
{
public:
    WaylandQPainterPrimaryLayer(WaylandOutput *output);
    ~WaylandQPainterPrimaryLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    void remapBuffer();

    WaylandQPainterBufferSlot *back() const;

    WaylandQPainterBufferSlot *acquire();
    void present();

    QRegion accumulateDamage(int bufferAge) const;

private:
    WaylandOutput *m_waylandOutput;
    KWayland::Client::ShmPool *m_pool;
    DamageJournal m_damageJournal;

    std::vector<std::unique_ptr<WaylandQPainterBufferSlot>> m_slots;
    WaylandQPainterBufferSlot *m_back = nullptr;
    QSize m_swapchainSize;

    friend class WaylandQPainterBackend;
};

class WaylandQPainterCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    explicit WaylandQPainterCursorLayer(WaylandOutput *output);
    ~WaylandQPainterCursorLayer() override;

    qreal scale() const;
    void setScale(qreal scale);

    QPoint hotspot() const;
    void setHotspot(const QPoint &hotspot);

    QSize size() const;
    void setSize(const QSize &size);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    WaylandOutput *m_output;
    QImage m_backingStore;
    QPoint m_hotspot;
    QSize m_size;
    qreal m_scale = 1.0;
};

class WaylandQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(WaylandBackend *b);
    ~WaylandQPainterBackend() override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    WaylandQPainterCursorLayer *cursorLayer(Output *output);

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
