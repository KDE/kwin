/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <chrono>

#include "core/renderbackend.h"
#include "kwin_export.h"
#include "vulkan/vulkan_device.h"

namespace KWin
{

class KWIN_EXPORT VulkanRenderTimeQuery : public QObject, public RenderTimeQuery
{
    Q_OBJECT

public:
    explicit VulkanRenderTimeQuery(VulkanDevice *device, vk::raii::QueryPool &&pool);
    ~VulkanRenderTimeQuery();

    void end(vk::raii::CommandBuffer &buffer);

    /**
     * fetches the result of the query. If rendering is not done yet, this will block!
     */
    std::optional<RenderTimeSpan> query() override;

    static std::unique_ptr<VulkanRenderTimeQuery> begin(VulkanDevice *device, vk::raii::CommandBuffer &buffer, uint32_t queueFamily);

private:
    void reset();

    VulkanDevice *m_device = nullptr;
    vk::raii::QueryPool m_pool;

    struct
    {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
    } m_cpuProbe;
    std::optional<RenderTimeSpan> m_result;
};

}
