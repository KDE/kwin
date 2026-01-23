/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QHash>
#include <QObject>
#include <QSize>
#include <memory>

typedef void *EGLImageKHR;

namespace KWin
{

class DrmDevice;
class EglDisplay;
class EglContext;
class GraphicsBuffer;

class KWIN_EXPORT RenderDevice : public QObject
{
    Q_OBJECT
public:
    explicit RenderDevice(std::unique_ptr<DrmDevice> &&device, std::unique_ptr<EglDisplay> &&display);
    ~RenderDevice();

    /**
     * the underlying drm device that can be used to allocate buffers for this render device
     * This doesn't necessarily represent a render node!
     */
    DrmDevice *drmDevice() const;
    EglDisplay *eglDisplay() const;
    /**
     * @returns an EGL context suitable for rendering with this render device,
     * Note that the EGL context is lazily created, and destroyed once no
     * references to it exist anymore.
     * If the context already exists, @p shareContext is ignored
     */
    std::shared_ptr<EglContext> eglContext(EglContext *shareContext = nullptr);

    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size);

    static std::unique_ptr<RenderDevice> open(const QString &path, int authenticatedFd = -1);

private:
    const std::unique_ptr<DrmDevice> m_device;
    const std::unique_ptr<EglDisplay> m_display;
    std::weak_ptr<EglContext> m_eglContext;

    QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> m_importedBuffers;
};

}
