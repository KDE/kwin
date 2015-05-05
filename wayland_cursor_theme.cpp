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
#include "wayland_cursor_theme.h"
#include "cursor.h"
// KWayland
#include <KWayland/Client/shm_pool.h>
// Wayland
#include <wayland-cursor.h>

namespace KWin
{

WaylandCursorTheme::WaylandCursorTheme(KWayland::Client::ShmPool *shm, QObject *parent)
    : QObject(parent)
    , m_theme(nullptr)
    , m_shm(shm)
{
}

WaylandCursorTheme::~WaylandCursorTheme()
{
    destroyTheme();
}

void WaylandCursorTheme::loadTheme()
{
    if (!m_shm->isValid()) {
        return;
    }
    Cursor *c = Cursor::self();
    if (!m_theme) {
        // so far the theme had not been created, this means we need to start tracking theme changes
        connect(c, &Cursor::themeChanged, this, &WaylandCursorTheme::loadTheme);
    } else {
        destroyTheme();
    }
    m_theme = wl_cursor_theme_load(c->themeName().toUtf8().constData(),
                                   c->themeSize() ? c->themeSize() : -1, m_shm->shm());
}

void WaylandCursorTheme::destroyTheme()
{
    if (!m_theme) {
        return;
    }
    wl_cursor_theme_destroy(m_theme);
    m_theme = nullptr;
}

wl_cursor_image *WaylandCursorTheme::get(Qt::CursorShape shape)
{
    if (!m_theme) {
        loadTheme();
    }
    if (!m_theme) {
        // loading cursor failed
        return nullptr;
    }
    wl_cursor *c = wl_cursor_theme_get_cursor(m_theme, Cursor::self()->cursorName(shape).constData());
    if (!c || c->image_count <= 0) {
        return nullptr;
    }
    return c->images[0];
}

}
