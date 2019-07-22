/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>
Copyright 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_SCENE_QPAINTER_WAYLAND_BACKEND_H
#define KWIN_SCENE_QPAINTER_WAYLAND_BACKEND_H

#include <platformsupport/scenes/qpainter/backend.h>

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
namespace Wayland
{
class WaylandBackend;
class WaylandOutput;
class WaylandQPainterBackend;

class WaylandQPainterOutput : public QObject
{
    Q_OBJECT
public:
    WaylandQPainterOutput(WaylandOutput *output, QObject *parent = nullptr);
    ~WaylandQPainterOutput() override;

    bool init(KWayland::Client::ShmPool *pool);
    void updateSize(const QSize &size);
    void remapBuffer();

    void prepareRenderingFrame();
    void present(const QRegion &damage);

private:
    WaylandOutput *m_waylandOutput;
    KWayland::Client::ShmPool *m_pool;

    QWeakPointer<KWayland::Client::Buffer> m_buffer;
    QImage m_backBuffer;

    friend class WaylandQPainterBackend;
};

class WaylandQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(WaylandBackend *b);
    ~WaylandQPainterBackend() override;

    bool usesOverlayWindow() const override;

    QImage *buffer() override;
    QImage *bufferForScreen(int screenId) override;

    void present(int mask, const QRegion& damage) override;
    void prepareRenderingFrame() override;

    bool needsFullRepaint() const override;
    bool perScreenRendering() const override;

private:
    void createOutput(WaylandOutput *waylandOutput);
    void frameRendered();

    WaylandBackend *m_backend;
    bool m_needsFullRepaint;

    QVector<WaylandQPainterOutput*> m_outputs;
};

}
}

#endif
