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
#ifndef KWIN_WAYLAND_COMPOSITOR_H
#define KWIN_WAYLAND_COMPOSITOR_H

#include <QObject>

struct wl_compositor;

namespace KWin
{
namespace Wayland
{

class Surface;

class Compositor : public QObject
{
    Q_OBJECT
public:
    explicit Compositor(QObject *parent = nullptr);
    virtual ~Compositor();

    bool isValid() const {
        return m_compositor != nullptr;
    }
    void setup(wl_compositor *compositor);
    void release();
    void destroy();

    Surface *createSurface(QObject *parent = nullptr);

    operator wl_compositor*() {
        return m_compositor;
    }
    operator wl_compositor*() const {
        return m_compositor;
    }

private:
    wl_compositor *m_compositor;
};

}
}

#endif
