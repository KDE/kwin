/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/overlaywindow.h"
#include "x11eventfilter.h"

namespace KWin
{

class X11StandaloneBackend;

class KWIN_EXPORT OverlayWindowX11 : public OverlayWindow, public X11EventFilter
{
public:
    explicit OverlayWindowX11(X11StandaloneBackend *backend);
    ~OverlayWindowX11() override;
    /// Creates XComposite overlay window, call initOverlay() and resize afterwards
    bool create() override;
    /// Init overlay and the destination window in it
    void setup(xcb_window_t window) override;
    void show() override;
    void hide() override; // hides and resets overlay window
    void setShape(const QRegion &reg);
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

    X11StandaloneBackend *m_backend;
    QSize m_size;
    QRegion m_shape;
    xcb_window_t m_window;
};
} // namespace
