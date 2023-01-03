/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_qpainter_backend.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <cmath>

namespace KWin
{
namespace Wayland
{

WaylandQPainterBufferSlot::WaylandQPainterBufferSlot(QSharedPointer<KWayland::Client::Buffer> buffer)
    : buffer(buffer)
    , image(buffer->address(), buffer->size().width(), buffer->size().height(), QImage::Format_RGB32)
{
    buffer->setUsed(true);
}

WaylandQPainterBufferSlot::~WaylandQPainterBufferSlot()
{
    buffer->setUsed(false);
}

WaylandQPainterPrimaryLayer::WaylandQPainterPrimaryLayer(WaylandOutput *output)
    : m_waylandOutput(output)
    , m_pool(output->backend()->display()->shmPool())
{
    connect(m_pool, &KWayland::Client::ShmPool::poolResized, this, &WaylandQPainterPrimaryLayer::remapBuffer);
}

WaylandQPainterPrimaryLayer::~WaylandQPainterPrimaryLayer()
{
    m_slots.clear();
}

void WaylandQPainterPrimaryLayer::remapBuffer()
{
    qCDebug(KWIN_WAYLAND_BACKEND) << "Remapped back buffer of surface" << m_waylandOutput->surface();

    const QSize nativeSize(m_waylandOutput->geometry().size() * m_waylandOutput->scale());
    for (const auto &slot : m_slots) {
        slot->image = QImage(slot->buffer->address(), nativeSize.width(), nativeSize.height(), QImage::Format_RGB32);
    }
}

void WaylandQPainterPrimaryLayer::present()
{
    for (const auto &slot : m_slots) {
        if (slot.get() == m_back) {
            slot->age = 1;
        } else if (slot->age > 0) {
            slot->age++;
        }
    }

    auto s = m_waylandOutput->surface();
    s->attachBuffer(m_back->buffer);
    s->damage(m_damageJournal.lastDamage());
    s->setScale(std::ceil(m_waylandOutput->scale()));
    s->commit();
}

WaylandQPainterBufferSlot *WaylandQPainterPrimaryLayer::back() const
{
    return m_back;
}

WaylandQPainterBufferSlot *WaylandQPainterPrimaryLayer::acquire()
{
    const QSize nativeSize(m_waylandOutput->pixelSize());
    if (m_swapchainSize != nativeSize) {
        m_swapchainSize = nativeSize;
        m_slots.clear();
    }

    for (const auto &slot : m_slots) {
        if (slot->buffer->isReleased()) {
            m_back = slot.get();
            slot->buffer->setReleased(false);
            return m_back;
        }
    }

    auto buffer = m_pool->getBuffer(nativeSize, nativeSize.width() * 4, KWayland::Client::Buffer::Format::RGB32).toStrongRef();
    if (!buffer) {
        qCDebug(KWIN_WAYLAND_BACKEND) << "Did not get a new Buffer from Shm Pool";
        return nullptr;
    }

    m_slots.push_back(std::make_unique<WaylandQPainterBufferSlot>(buffer));
    m_back = m_slots.back().get();

    //    qCDebug(KWIN_WAYLAND_BACKEND) << "Created a new back buffer for output surface" << m_waylandOutput->surface();
    return m_back;
}

QRegion WaylandQPainterPrimaryLayer::accumulateDamage(int bufferAge) const
{
    return m_damageJournal.accumulate(bufferAge, infiniteRegion());
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterPrimaryLayer::beginFrame()
{
    WaylandQPainterBufferSlot *slot = acquire();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&slot->image),
        .repaint = accumulateDamage(slot->age),
    };
}

bool WaylandQPainterPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_damageJournal.add(damagedRegion);
    return true;
}

WaylandQPainterCursorLayer::WaylandQPainterCursorLayer(WaylandOutput *output)
    : m_output(output)
{
}

WaylandQPainterCursorLayer::~WaylandQPainterCursorLayer()
{
}

qreal WaylandQPainterCursorLayer::scale() const
{
    return m_scale;
}

void WaylandQPainterCursorLayer::setScale(qreal scale)
{
    m_scale = scale;
}

QPoint WaylandQPainterCursorLayer::hotspot() const
{
    return m_hotspot;
}

void WaylandQPainterCursorLayer::setHotspot(const QPoint &hotspot)
{
    m_hotspot = hotspot;
}

QSize WaylandQPainterCursorLayer::size() const
{
    return m_size;
}

void WaylandQPainterCursorLayer::setSize(const QSize &size)
{
    m_size = size;
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterCursorLayer::beginFrame()
{
    const QSize bufferSize = m_size.expandedTo(QSize(64, 64));
    if (m_backingStore.size() != bufferSize) {
        m_backingStore = QImage(bufferSize, QImage::Format_ARGB32_Premultiplied);
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_backingStore),
        .repaint = infiniteRegion(),
    };
}

bool WaylandQPainterCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    KWayland::Client::Buffer::Ptr buffer = m_output->backend()->display()->shmPool()->createBuffer(m_backingStore);
    m_output->cursor()->update(*buffer.lock(), m_scale, m_hotspot);
    return true;
}

WaylandQPainterBackend::WaylandQPainterBackend(Wayland::WaylandBackend *b)
    : QPainterBackend()
    , m_backend(b)
{

    const auto waylandOutputs = m_backend->waylandOutputs();
    for (auto *output : waylandOutputs) {
        createOutput(output);
    }
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandQPainterBackend::createOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *waylandOutput) {
        m_outputs.erase(waylandOutput);
    });
}

WaylandQPainterBackend::~WaylandQPainterBackend()
{
}

void WaylandQPainterBackend::createOutput(Output *waylandOutput)
{
    m_outputs[waylandOutput] = Layers{
        .primaryLayer = std::make_unique<WaylandQPainterPrimaryLayer>(static_cast<WaylandOutput *>(waylandOutput)),
        .cursorLayer = std::make_unique<WaylandQPainterCursorLayer>(static_cast<WaylandOutput *>(waylandOutput)),
    };
}

void WaylandQPainterBackend::present(Output *output)
{
    m_outputs[output].primaryLayer->present();
}

OutputLayer *WaylandQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

WaylandQPainterCursorLayer *WaylandQPainterBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
}
