/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "opengl/eglbackend.h"
#include "opengl/glutils.h"

namespace KWin
{

class X11WindowedBackend;

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class X11WindowedEglBackend : public EglBackend
{
    Q_OBJECT

public:
    explicit X11WindowedEglBackend(X11WindowedBackend *backend, RenderDevice *device);
    ~X11WindowedEglBackend() override;

    X11WindowedBackend *backend() const;

    bool init() override;
    void endFrame(BackendOutput *output, const Region &renderedDeviceRegion, const Region &damagedDeviceRegion);
    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;

private:
    X11WindowedBackend *m_backend;
};

} // namespace KWin
