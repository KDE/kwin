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
    explicit EglBackend();

    bool init();

    FormatModifierMap supportedFormats(RenderDevice *device) const override;

    bool testImportBuffer(GraphicsBuffer *buffer, dev_t targetDevice) override;

    QList<LinuxDmaBufV1Feedback::Tranche> tranches() const;

protected:
    bool initClientExtensions();
    bool hasClientExtension(const QByteArray &ext) const;
    void updateDmabufTranches();

    QList<QByteArray> m_clientExtensions;
    QList<LinuxDmaBufV1Feedback::Tranche> m_tranches;
    QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> m_importedBuffers;
};

}
