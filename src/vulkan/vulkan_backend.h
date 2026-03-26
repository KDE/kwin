/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/renderbackend.h"
#include "wayland/linuxdmabufv1clientbuffer.h"

namespace KWin
{

class RenderDevice;

class VulkanBackend : public RenderBackend
{
    Q_OBJECT

public:
    bool checkGraphicsReset() override final;
    bool testImportBuffer(GraphicsBuffer *buffer) override;
    CompositingType compositingType() const override;

    RenderDevice *renderDevice() const;

protected:
    explicit VulkanBackend(RenderDevice *device);
    void initWayland();

    RenderDevice *const m_device;
};

}
