/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_SURFACE_H
#define KWIN_WAYLAND_SURFACE_H

#include <QObject>
#include <QPoint>

#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

class Surface : public QObject
{
    Q_OBJECT
public:
    explicit Surface(QObject *parent = nullptr);
    virtual ~Surface();

    void setup(wl_surface *surface);
    void release();
    void destroy();
    bool isValid() const {
        return m_surface != nullptr;
    }
    void setupFrameCallback();
    enum class CommitFlag {
        None,
        FrameCallback
    };
    void commit(CommitFlag flag = CommitFlag::FrameCallback);
    void damage(const QRect &rect);
    void damage(const QRegion &region);
    void attachBuffer(wl_buffer *buffer, const QPoint &offset = QPoint());

    operator wl_surface*() {
        return m_surface;
    }
    operator wl_surface*() const {
        return m_surface;
    }

    static void frameCallback(void *data, wl_callback *callback, uint32_t time);

Q_SIGNALS:
    void frameRendered();

private:
    void handleFrameCallback();
    static const wl_callback_listener s_listener;
    wl_surface *m_surface;
    bool m_frameCallbackInstalled;
};

}
}

#endif
