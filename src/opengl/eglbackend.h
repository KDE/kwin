/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/renderbackend.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "wayland/linuxdmabufv1clientbuffer.h"

#include <memory>

#include <epoxy/egl.h>

namespace KWin
{
class LogicalOutput;
class GLTexture;
class EglContext;
class EglDisplay;
class RenderDevice;

struct DmaBufAttributes;

class KWIN_EXPORT EglBackend : public RenderBackend
{
    Q_OBJECT

public:
    virtual bool init() = 0;
    CompositingType compositingType() const override final;
    bool checkGraphicsReset() override final;

    EglContext *openglContext() const;
    std::shared_ptr<EglContext> openglContextRef() const;
    EglDisplay *eglDisplayObject() const;

    bool testImportBuffer(GraphicsBuffer *buffer) override;
    QHash<uint32_t, QList<uint64_t>> supportedFormats() const override;

    QList<LinuxDmaBufV1Feedback::Tranche> tranches() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size);

protected:
    EglBackend();

    void cleanup();
    void setRenderDevice(RenderDevice *device);
    bool initClientExtensions();
    void initWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext();

    bool ensureGlobalShareContext();
    void destroyGlobalShareContext();
    ::EGLContext createContextInternal(::EGLContext sharedContext);
    void teardown();

    RenderDevice *m_renderDevice = nullptr;
    std::shared_ptr<EglContext> m_context;
    QList<QByteArray> m_clientExtensions;
    QList<LinuxDmaBufV1Feedback::Tranche> m_tranches;
    QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> m_importedBuffers;
};

}
