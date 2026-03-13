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
#include <vulkan/vulkan_raii.hpp>

typedef void *EGLImageKHR;

namespace KWin
{

class DrmDevice;
class EglDisplay;
class EglContext;
class GraphicsBuffer;
class VulkanDevice;

class KWIN_EXPORT RenderDevice : public QObject
{
    Q_OBJECT

public:
    explicit RenderDevice(std::unique_ptr<DrmDevice> &&device, std::unique_ptr<EglDisplay> &&display);
    ~RenderDevice();

    /**
     * the underlying drm device that can be used to allocate buffers for this render device
     * This doesn't necessarily represent a render node!
     */
    DrmDevice *drmDevice() const;
    EglDisplay *eglDisplay() const;
    /**
     * @returns an EGL context suitable for rendering with this render device,
     * Note that the EGL context is lazily created, and destroyed once no
     * references to it exist anymore.
     * If the context already exists, @p shareContext is ignored
     */
    std::shared_ptr<EglContext> eglContext(EglContext *shareContext = nullptr);

    VulkanDevice *vulkanDevice() const;

    // TODO move these + caching to EglDisplay?
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size);

    /**
     * @returns all format+modifiers that can be imported with either
     *          EGL or Vulkan
     */
    const FormatModifierMap &allImportableFormats() const;

    /**
     * @returns whether or not this device is handling a GPU reset at the moment.
     *          This can be used to avoid attempting to allocate new GPU resources
     *          until the reset is complete
     */
    bool isInReset() const;

    static std::unique_ptr<RenderDevice> open(const QString &path, int authenticatedFd = -1);

private:
    void handleVulkanDeviceLoss();
    void createVulkanDevice();

    const std::unique_ptr<DrmDevice> m_device;
    const std::unique_ptr<EglDisplay> m_display;
    const vk::raii::Context m_vulkanContext;
    const vk::raii::Instance m_vulkanInstance;
    std::unique_ptr<VulkanDevice> m_vulkanDevice;
    FormatModifierMap m_allImportableFormats;
    std::weak_ptr<EglContext> m_eglContext;
    bool m_inReset = false;

    QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> m_importedBuffers;
};

}
