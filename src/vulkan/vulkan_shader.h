/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "vulkan/vulkan_device.h"

namespace KWin
{

class VulkanShader
{
public:
    explicit VulkanShader(vk::raii::ShaderEXT &&shader);
    ~VulkanShader();

    const vk::raii::ShaderEXT &handle() const;

    static std::unique_ptr<VulkanShader> compile(VulkanDevice *device, QByteArrayView spirV);
    static std::unique_ptr<VulkanShader> compileFromPath(VulkanDevice *device, const QString &path);

private:
    vk::raii::ShaderEXT m_shader;
};

}
