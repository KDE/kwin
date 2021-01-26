/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRegion>
#include <QSize>

#include <xcb/render.h>

namespace KWin
{

class OverlayWindow;

/**
 * @brief Backend for the SceneXRender to hold the compositing buffer and take care of buffer
 * swapping.
 *
 * This class is intended as a small abstraction to support multiple compositing backends in the
 * SceneXRender.
 */
class XRenderBackend
{
public:
    virtual ~XRenderBackend();
    virtual void present(int mask, const QRegion &damage) = 0;

    /**
     * @brief Returns the OverlayWindow used by the backend.
     *
     * A backend does not have to use an OverlayWindow, this is mostly for the X world.
     * In case the backend does not use an OverlayWindow it is allowed to return @c null.
     * It's the task of the caller to check whether it is @c null.
     *
     * @return :OverlayWindow*
     */
    virtual OverlayWindow *overlayWindow();
    /**
     * @brief Shows the Overlay Window
     *
     * Default implementation does nothing.
     */
    virtual void showOverlay();
    /**
     * @brief React on screen geometry changes.
     *
     * Default implementation does nothing. Override if specific functionality is required.
     *
     * @param size The new screen size
     */
    virtual void screenGeometryChanged(const QSize &size);
    /**
     * @brief The compositing buffer hold by this backend.
     *
     * The Scene composites the new frame into this buffer.
     *
     * @return xcb_render_picture_t
     */
    xcb_render_picture_t buffer() const;
    /**
     * @brief Whether the creation of the Backend failed.
     *
     * The SceneXRender should test whether the Backend got constructed correctly. If this method
     * returns @c true, the SceneXRender should not try to start the rendering.
     *
     * @return bool @c true if the creation of the Backend failed, @c false otherwise.
     */
    bool isFailed() const;

protected:
    XRenderBackend();
    /**
     * @brief A subclass needs to call this method once it created the compositing back buffer.
     *
     * @param buffer The buffer to use for compositing
     * @return void
     */
    void setBuffer(xcb_render_picture_t buffer);
    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     */
    void setFailed(const QString &reason);

private:
    // Create the compositing buffer. The root window is not double-buffered,
    // so it is done manually using this buffer,
    xcb_render_picture_t m_buffer;
    bool m_failed;
};

} // namespace KWin
