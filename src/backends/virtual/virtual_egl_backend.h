/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "opengl/eglbackend.h"

#include <chrono>
#include <memory>

namespace KWin
{

class VirtualBackend;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class VirtualEglBackend : public EglBackend
{
    Q_OBJECT

public:
    VirtualEglBackend(VirtualBackend *b, RenderDevice *renderDevice);
    ~VirtualEglBackend() override;

    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;
    bool init() override;

    VirtualBackend *backend() const;

private:
    void addOutput(BackendOutput *output);

    VirtualBackend *m_backend;
};

} // namespace KWin
