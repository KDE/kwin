/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_CURSOR_THEME_H
#define KWIN_WAYLAND_CURSOR_THEME_H

#include <kwin_export.h>

#include <QObject>

struct wl_cursor_image;
struct wl_cursor_theme;

namespace KWayland
{
namespace Client
{
class ShmPool;
}
}

namespace KWin
{

class KWIN_EXPORT WaylandCursorTheme : public QObject
{
    Q_OBJECT
public:
    explicit WaylandCursorTheme(KWayland::Client::ShmPool *shm, QObject *parent = nullptr);
    virtual ~WaylandCursorTheme();

    wl_cursor_image *get(Qt::CursorShape shape);
    wl_cursor_image *get(const QByteArray &name);

Q_SIGNALS:
    void themeChanged();

private:
    void loadTheme();
    void destroyTheme();
    wl_cursor_theme *m_theme;
    KWayland::Client::ShmPool *m_shm = nullptr;
};

}

#endif
