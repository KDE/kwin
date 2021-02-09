/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "xrenderbackend.h"

namespace KWin
{

class SoftwareVsyncMonitor;
class X11StandalonePlatform;

/**
 * @brief XRenderBackend using an X11 Overlay Window as compositing target.
 */
class X11XRenderBackend : public QObject, public XRenderBackend
{
    Q_OBJECT

public:
    explicit X11XRenderBackend(X11StandalonePlatform *backend);
    ~X11XRenderBackend() override;

    void present(int mask, const QRegion &damage) override;
    OverlayWindow *overlayWindow() override;
    void showOverlay() override;
    void screenGeometryChanged(const QSize &size) override;

private:
    void init(bool createOverlay);
    void createBuffer();
    void vblank(std::chrono::nanoseconds timestamp);

    X11StandalonePlatform *m_backend;
    SoftwareVsyncMonitor *m_vsyncMonitor;
    QScopedPointer<OverlayWindow> m_overlayWindow;
    xcb_render_picture_t m_front;
    xcb_render_pictformat_t m_format;
};

} // namespace KWin
