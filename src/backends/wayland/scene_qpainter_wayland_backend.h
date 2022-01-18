/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_WAYLAND_BACKEND_H
#define KWIN_SCENE_QPAINTER_WAYLAND_BACKEND_H

#include "qpainterbackend.h"
#include "utils/common.h"

#include <QObject>
#include <QImage>
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
class AbstractOutput;
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

class WaylandQPainterOutput : public QObject
{
    Q_OBJECT
public:
    WaylandQPainterOutput(WaylandOutput *output, QObject *parent = nullptr);
    ~WaylandQPainterOutput() override;

    bool init(KWayland::Client::ShmPool *pool);
    void updateSize(const QSize &size);
    void remapBuffer();

    WaylandQPainterBufferSlot *back() const;

    WaylandQPainterBufferSlot *acquire();
    void present(const QRegion &damage);

    QRegion accumulateDamage(int bufferAge) const;
    QRegion mapToLocal(const QRegion &region) const;

private:
    WaylandOutput *m_waylandOutput;
    KWayland::Client::ShmPool *m_pool;
    DamageJournal m_damageJournal;

    QVector<WaylandQPainterBufferSlot *> m_slots;
    WaylandQPainterBufferSlot *m_back = nullptr;

    friend class WaylandQPainterBackend;
};

class WaylandQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(WaylandBackend *b);
    ~WaylandQPainterBackend() override;

    QImage *bufferForScreen(AbstractOutput *output) override;

    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    QRegion beginFrame(AbstractOutput *output) override;

private:
    void createOutput(AbstractOutput *waylandOutput);
    void frameRendered();

    WaylandBackend *m_backend;
    QMap<AbstractOutput *, WaylandQPainterOutput*> m_outputs;
};

}
}

#endif
