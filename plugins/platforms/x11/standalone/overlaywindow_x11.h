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

#ifndef KWIN_OVERLAYWINDOW_X11_H
#define KWIN_OVERLAYWINDOW_X11_H

#include "../../../../overlaywindow.h"

namespace KWin {
class KWIN_EXPORT OverlayWindowX11 : public OverlayWindow {
public:
    OverlayWindowX11();
    ~OverlayWindowX11();
    /// Creates XComposite overlay window, call initOverlay() afterwards
    bool create() override;
    /// Init overlay and the destination window in it
    void setup(xcb_window_t window) override;
    void show() override;
    void hide() override; // hides and resets overlay window
    void setShape(const QRegion& reg) override;
    void resize(const QSize &size) override;
    /// Destroys XComposite overlay window
    void destroy() override;
    xcb_window_t window() const override;
    bool isVisible() const override;
    void setVisibility(bool visible) override;
private:
    void setNoneBackgroundPixmap(xcb_window_t window);
    void setupInputShape(xcb_window_t window);
    bool m_visible;
    bool m_shown; // For showOverlay()
    QRegion m_shape;
    xcb_window_t m_window;
};
} // namespace

#endif //KWIN_OVERLAYWINDOW_H
