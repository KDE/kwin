/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

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
class Buffer;
}
}

namespace KWin
{
namespace Wayland
{
class WaylandBackend;
}

class WaylandQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(Wayland::WaylandBackend *b);
    virtual ~WaylandQPainterBackend();

    virtual void present(int mask, const QRegion& damage) override;
    virtual bool usesOverlayWindow() const override;
    virtual void screenGeometryChanged(const QSize &size) override;
    virtual QImage *buffer() override;
    virtual void prepareRenderingFrame() override;
    virtual bool needsFullRepaint() const override;
private Q_SLOTS:
    void remapBuffer();
private:
    Wayland::WaylandBackend *m_backend;
    bool m_needsFullRepaint;
    QImage m_backBuffer;
    QWeakPointer<KWayland::Client::Buffer> m_buffer;
};

}

#endif
