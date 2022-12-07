/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "../common/x11_common_egl_backend.h"
#include "core/outputlayer.h"
#include "kwinglutils.h"

#include <QMap>

namespace KWin
{

class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedEglBackend;

class X11WindowedEglPrimaryLayer : public OutputLayer
{
public:
    X11WindowedEglPrimaryLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output, EGLSurface surface);
    ~X11WindowedEglPrimaryLayer();

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    EGLSurface surface() const;
    QRegion lastDamage() const;

private:
    void ensureFbo();

    EGLSurface m_eglSurface;
    std::unique_ptr<GLFramebuffer> m_fbo;
    QRegion m_lastDamage;

    X11WindowedOutput *const m_output;
    X11WindowedEglBackend *const m_backend;
};

class X11WindowedEglCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    X11WindowedEglCursorLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output);
    ~X11WindowedEglCursorLayer() override;

    QPoint hotspot() const;
    void setHotspot(const QPoint &hotspot);

    QSize size() const;
    void setSize(const QSize &size);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    X11WindowedOutput *const m_output;
    X11WindowedEglBackend *const m_backend;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::unique_ptr<GLTexture> m_texture;
    QPoint m_hotspot;
    QSize m_size;
};

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class X11WindowedEglBackend : public EglOnXBackend
{
    Q_OBJECT

public:
    explicit X11WindowedEglBackend(X11WindowedBackend *backend);
    ~X11WindowedEglBackend() override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    void init() override;
    void endFrame(Output *output, const QRegion &renderedRegion, const QRegion &damagedRegion);
    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    X11WindowedEglCursorLayer *cursorLayer(Output *output);

protected:
    void cleanupSurfaces() override;
    bool createSurfaces() override;

private:
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);

    struct Layers
    {
        std::unique_ptr<X11WindowedEglPrimaryLayer> primaryLayer;
        std::unique_ptr<X11WindowedEglCursorLayer> cursorLayer;
    };

    std::map<Output *, Layers> m_outputs;
    X11WindowedBackend *m_backend;
};

} // namespace KWin
