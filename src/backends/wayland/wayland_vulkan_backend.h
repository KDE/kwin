/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "utils/damagejournal.h"
#include "vulkan/vulkan_backend.h"
#include "wayland_layer.h"

namespace KWin
{

namespace Wayland
{

class WaylandBackend;

class WaylandVulkanBackend : public VulkanBackend
{
    Q_OBJECT

public:
    WaylandVulkanBackend(WaylandBackend *b, RenderDevice *device);
    ~WaylandVulkanBackend() override;

    WaylandBackend *backend() const;
    FormatModifierMap supportedFormats() const override;
    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;

private:
    void createOutputLayers(BackendOutput *output);

    WaylandBackend *const m_backend;
    FormatModifierMap m_formats;
};

}
}
