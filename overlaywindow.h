/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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

#ifndef KWIN_OVERLAYWINDOW_H
#define KWIN_OVERLAYWINDOW_H

#include <QRegion>

#include <X11/Xlib.h>

namespace KWin {
class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();
    /// Creates XComposite overlay window, call initOverlay() afterwards
    bool create();
    /// Init overlay and the destination window in it
    void setup(Window window);
    void show();
    void hide(); // hides and resets overlay window
    void setShape(const QRegion& reg);
    void resize(const QSize &size);
    /// Destroys XComposite overlay window
    void destroy();
    Window window() const;
    bool isVisible() const;
    void setVisibility(bool visible);
private:
    bool m_visible;
    bool m_shown; // For showOverlay()
    QRegion m_shape;
    Window m_window;
};
} // namespace

#endif //KWIN_OVERLAYWINDOW_H
