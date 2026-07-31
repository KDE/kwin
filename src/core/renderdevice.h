/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/drm_formats.h"
#include "kwin_export.h"

#include <QHash>
#include <QObject>
#include <QSize>
#include <memory>
#include <sys/types.h>
#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class DrmDevice;
class EglDisplay;
class EglContext;
class GraphicsBuffer;
class VulkanDevice;
class GraphicsBufferAllocator;
class UDmabufAllocator;

class KWIN_EXPORT RenderDevice : public QObject
{
    Q_OBJECT

public:
    explicit RenderDevice(std::unique_ptr<DrmDevice> &&device, std::unique_ptr<EglDisplay> &&display);
    explicit RenderDevice(std::unique_ptr<UDmabufAllocator> &&allocator, std::unique_ptr<EglDisplay> &&display, dev_t deviceId);
    ~RenderDevice();

    /**
     * the underlying drm device that can be used to allocate buffers for this render device
     * This doesn't necessarily represent a render node, and may be nullptr!
     */
    DrmDevice *drmDevice() const;
    GraphicsBufferAllocator *allocator() const;
    QString path() const;
    QString name() const;

    dev_t deviceId() const;
    EglDisplay *eglDisplay() const;
    /**
     * @returns an EGL context suitable for rendering with this render device,
     * Note that the EGL context is lazily created, and destroyed once no
     * references to it exist anymore.
     * This context is shared with eglShareContext.
     */
    std::shared_ptr<EglContext> eglContext();
    /**
     * @returns an EGL context that can be used as a share context for
     *          OpenGL contexts targeting this device.
     *          Note that the EGL context is lazily created, and destroyed
     *          once no references to it exist anymore.
     */
    std::shared_ptr<EglContext> eglShareContext();

    VulkanDevice *vulkanDevice() const;

    /**
     * @returns all format+modifiers that can be imported with either
     *          EGL or Vulkan
     */
    const FormatModifierMap &allImportableFormats() const;

    /**
     * @returns all format+modifiers that can be rendered to be EGL
     *          and sampled by both EGL and Vulkan
     */
    const FormatModifierMap &renderableFormats() const;

    /**
     * @returns whether or not this device is handling a GPU reset at the moment.
     *          This can be used to avoid attempting to allocate new GPU resources
     *          until the reset is complete
     */
    bool isInReset() const;

    /**
     * @returns if *both* EGL and Vulkan aren't hardware
     *          accelerated with this device
     */
    bool isSoftwareDevice() const;

    /**
     * @returns if this device is definitely an internal GPU. May have false negatives,
     *          depending on driver support for Vulkan and EGL_EXT_device_type.
     */
    bool isInternal() const;

    bool isIntel() const;
    bool isNvidia() const;

    static std::unique_ptr<RenderDevice> open(const QString &path, int authenticatedFd = -1);
    /**
     * @returns a RenderDevice without a drm device, using udmabuf for allocations
     *          and a software renderer for OpenGL and Vulkan
     */
    static std::unique_ptr<RenderDevice> createSoftwareDevice(dev_t deviceId);

private:
    void handleVulkanDeviceLoss();
    void createVulkanDevice();
    void fetchName();

    const std::unique_ptr<DrmDevice> m_device;
    const std::unique_ptr<UDmabufAllocator> m_udmabufAllocator;
    const std::unique_ptr<EglDisplay> m_display;
    const vk::raii::Context m_vulkanContext;
    const vk::raii::Instance m_vulkanInstance;
    std::unique_ptr<VulkanDevice> m_vulkanDevice;
    FormatModifierMap m_allImportableFormats;
    FormatModifierMap m_renderableFormats;
    std::weak_ptr<EglContext> m_shareContext;
    std::weak_ptr<EglContext> m_eglContext;
    bool m_inReset = false;
    const QString m_path;
    const dev_t m_deviceId;
    QString m_name;
};

}
