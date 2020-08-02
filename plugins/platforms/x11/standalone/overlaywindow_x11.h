/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_OVERLAYWINDOW_X11_H
#define KWIN_OVERLAYWINDOW_X11_H

#include "../../../../overlaywindow.h"
#include "../../../../x11eventfilter.h"

namespace KWin {
class KWIN_EXPORT OverlayWindowX11 : public OverlayWindow, public X11EventFilter {
public:
    OverlayWindowX11();
    ~OverlayWindowX11() override;
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

    bool event(xcb_generic_event_t *event) override;
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
