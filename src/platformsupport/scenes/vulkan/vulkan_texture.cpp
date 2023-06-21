/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_texture.h"

#include <QDebug>
#include <utility>

namespace KWin
{

VulkanTexture::VulkanTexture(vk::Format format, vk::UniqueImage &&image, std::vector<vk::UniqueDeviceMemory> &&memory, vk::UniqueImageView &&imageView)
    : m_format(format)
    , m_memory(std::move(memory))
    , m_image(std::move(image))
    , m_view(std::move(imageView))
{
}

VulkanTexture::VulkanTexture(VulkanTexture &&other)
    : m_format(other.m_format)
    , m_memory(std::move(other.m_memory))
    , m_image(std::move(other.m_image))
    , m_view(std::move(other.m_view))
{
}

VulkanTexture::~VulkanTexture()
{
    m_view.reset();
    m_image.reset();
    m_memory.clear();
}

}
