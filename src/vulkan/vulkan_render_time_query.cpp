/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_render_time_query.h"

namespace KWin
{

VulkanRenderTimeQuery::VulkanRenderTimeQuery(VulkanDevice *device, vk::raii::QueryPool &&pool)
    : m_device(device)
    , m_pool(std::move(pool))
{
    m_cpuProbe.start = std::chrono::steady_clock::now();
    connect(device, &VulkanDevice::deviceLost, this, &VulkanRenderTimeQuery::reset);
}

VulkanRenderTimeQuery::~VulkanRenderTimeQuery()
{
}

void VulkanRenderTimeQuery::end(vk::raii::CommandBuffer &buffer)
{
    if (!m_device) {
        return;
    }
    m_cpuProbe.end = std::chrono::steady_clock::now();
    buffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_pool, 1);
}

std::optional<RenderTimeSpan> VulkanRenderTimeQuery::query()
{
    if (!m_device) {
        return std::nullopt;
    }
    if (!m_result) {
        auto [result, timestamps] = m_pool.getResults<uint64_t>(0, 2, 2 * sizeof(uint64_t), sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
        if (result != vk::Result::eSuccess) {
            reset();
            return std::nullopt;
        }
        const uint64_t gpuTicks = timestamps[1] - timestamps[0];
        const std::chrono::nanoseconds gpuDuration(uint64_t(std::round(gpuTicks * m_device->nanosecondsPerQueryTick())));
        m_result = RenderTimeSpan{
            .start = m_cpuProbe.start,
            .end = std::max(m_cpuProbe.end, m_cpuProbe.start + gpuDuration),
        };
    }
    return m_result;
}

void VulkanRenderTimeQuery::reset()
{
    m_pool.clear();
    m_device = nullptr;
}

std::unique_ptr<VulkanRenderTimeQuery> VulkanRenderTimeQuery::begin(VulkanDevice *device, vk::raii::CommandBuffer &buffer, uint32_t queueFamily)
{
    if (!device->queueFamilyProperties()[queueFamily].timestampValidBits) {
        return nullptr;
    }
    auto [result, query] = device->logicalDevice().createQueryPool(vk::QueryPoolCreateInfo{
        vk::QueryPoolCreateFlags{},
        vk::QueryType::eTimestamp,
        2,
    });
    if (result != vk::Result::eSuccess) {
        return nullptr;
    }
    buffer.resetQueryPool(query, 0, 2);
    buffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, query, 0);
    return std::make_unique<VulkanRenderTimeQuery>(device, std::move(query));
}

}
#include "moc_vulkan_render_time_query.cpp"
