/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef VULKAN_SCENE_H
#define VULKAN_SCENE_H

#include "../../../scene.h"
#include "decorations/decorationrenderer.h"
#include "kwinvulkanutils.h"

#include <deque>
#include <queue>
#include <unordered_set>

namespace KWin
{

class VulkanBackend;
class VulkanSwapchain;
class SceneDescriptorPool;
class TextureDescriptorSet;
class ColorDescriptorSet;
class Shadow;



// ------------------------------------------------------------------



class KWIN_EXPORT VulkanFactory : public SceneFactory
{
    Q_OBJECT
    Q_INTERFACES(KWin::SceneFactory)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Scene" FILE "vulkan.json")

public:
    explicit VulkanFactory(QObject *parent = nullptr);
    ~VulkanFactory() override;

    Scene *create(QObject *parent = nullptr) const override;
};



// ------------------------------------------------------------------



template <typename T>
class SharedVulkanObjectSet : public std::unordered_set<std::shared_ptr<T>>
{
public:
    SharedVulkanObjectSet() : std::unordered_set<std::shared_ptr<T>>() {}

    auto toNativeHandleVector() const {
        std::vector<typename T::NativeHandleType> vector;
        vector.reserve(this->size());
        for (const auto &entry : *this)
            vector.push_back(entry->handle());
        return vector;
    }
};



// ------------------------------------------------------------------



class VulkanScene : public Scene
{
    Q_OBJECT

    enum QueueFamily {
        GraphicsQueueFamily = 0,
        QueueFamilyCount
    };

    struct FrameData
    {
        std::shared_ptr<VulkanSemaphore> imageAcquisitionSemaphore;
        std::shared_ptr<VulkanSemaphore> acquireOwnershipSemaphore;
        std::shared_ptr<VulkanSemaphore> releaseOwnershipSemaphore;
        VulkanFence imageAcquisitionFence;
        bool imageAcquisitionFenceSubmitted;
    };

    struct PaintPassData
    {
        VulkanCommandPool commandPools[QueueFamilyCount];
        std::unique_ptr<VulkanCommandBuffer> setupCommandBuffer;
        std::unique_ptr<VulkanCommandBuffer> mainCommandBuffer;
        std::unordered_set<std::shared_ptr<VulkanObject>> busyObjects;
        std::unordered_set<std::shared_ptr<VulkanCommandBuffer>> busyGraphicsCommandBuffers;
        std::shared_ptr<VulkanSemaphore> semaphore;
        VulkanFence fence;
        bool fenceSubmitted;
    };

    enum { FramesInFlight = 2 };

public:
    explicit VulkanScene(std::unique_ptr<VulkanBackend> &&backend, QObject *parent = nullptr);
    ~VulkanScene() override;

    bool usesOverlayWindow() const override;
    OverlayWindow* overlayWindow() override;
    qint64 paint(QRegion damage, ToplevelList windows) override;
    void paintSimpleScreen(int mask, QRegion region) override;
    void paintGenericScreen(int mask, ScreenPaintData data) override;
    void paintBackground(QRegion region) override;
    void paintCursor() override;
    CompositingType compositingType() const override;
    bool initFailed() const override;
    EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    void screenGeometryChanged(const QSize &size) override;

    bool animationsSupported() const override { return true; }

    VulkanBackend *backend() const { return m_backend.get(); }

    /**
     * Returns a pointer to the logical device.
     */
    VulkanDevice *device() const { return m_device.get(); }

    VulkanPipelineManager *pipelineManager() const { return m_pipelineManager.get(); }
    VulkanUploadManager *uploadManager() const { return m_uploadManager.get(); }
    VulkanUploadManager *imageUploadManager() const { return m_imageUploadManager.get(); }
    VulkanDeviceMemoryAllocator *deviceMemoryAllocator() const { return m_deviceMemoryAllocator.get(); }
    VulkanStagingImageAllocator *stagingImageAllocator() const { return m_stagingImageAllocator.get(); }
    SceneDescriptorPool *textureDescriptorPool() const { return m_textureDescriptorPool.get(); }
    SceneDescriptorPool *crossFadeDescriptorPool() const { return m_crossFadeDescriptorPool.get(); }
    SceneDescriptorPool *colorDescriptorPool() const { return m_colorDescriptorPool.get(); }

    VkBuffer indexBufferForQuadCount(uint32_t quadCount);

    FrameData &currentFrame() { return m_frameData[m_frameIndex]; }
    const FrameData &currentFrame() const { return m_frameData[m_frameIndex]; }

    PaintPassData &currentPaintPass() { return m_paintPassData[m_paintPassIndex]; }
    const PaintPassData &currentPaintPass() const { return m_paintPassData[m_paintPassIndex]; }

    /**
     * Adds the given object to the list of objects referenced by the current frame.
     */
    void addBusyReference(std::shared_ptr<VulkanObject> object) { currentPaintPass().busyObjects.insert(object); }

    QMatrix4x4 projectionMatrix() const { return m_projectionMatrix; }
    QMatrix4x4 screenProjectionMatrix() const override final { return m_screenProjectionMatrix; }

    VulkanCommandBuffer *mainCommandBuffer();
    VulkanCommandBuffer *setupCommandBuffer();

    VkSampler linearSampler() const { return m_linearSampler.handle(); }
    VkSampler nearestSampler() const { return m_nearestSampler.handle(); }

    uint32_t optimalBufferCopyOffsetAlignment() const { return m_physicalDeviceProperties.limits.optimalBufferCopyOffsetAlignment; }
    uint32_t optimalBufferCopyRowPitchAlignment() const { return m_physicalDeviceProperties.limits.optimalBufferCopyRowPitchAlignment; }

    QMatrix4x4 ortho(float left, float right, float bottom, float top, float near, float far) const;
    QMatrix4x4 frustum(float left, float right, float bottom, float top, float zNear, float zFar) const;

protected:
    Scene::Window *createWindow(Toplevel *toplevel) override;

private:
    bool init();
    void advertiseSurfaceFormats();
    bool createRenderPasses();
    void beginRenderPass(VulkanCommandBuffer *cmd);
    QMatrix4x4 createProjectionMatrix() const;
    QMatrix4x4 screenMatrix(int mask, const ScreenPaintData &data) const;
    void handleDeviceLostError();
    void handleFatalError();
    bool checkResult(VkResult result, std::function<void(void)> debugMessage);

private:
    VulkanInstance                                           m_instance;
    VulkanDebugReportCallback                                m_debugReportCallback;
    std::unique_ptr<VulkanBackend>                           m_backend;
    std::unique_ptr<VulkanDevice>                            m_device;
    std::unique_ptr<VulkanPipelineCache>                     m_pipelineCache;
    std::unique_ptr<VulkanPipelineManager>                   m_pipelineManager;
    std::unique_ptr<VulkanUploadManager>                     m_uploadManager;
    std::unique_ptr<VulkanUploadManager>                     m_imageUploadManager;
    std::unique_ptr<VulkanDeviceMemoryAllocator>             m_deviceMemoryAllocator;
    std::unique_ptr<VulkanStagingImageAllocator>             m_stagingImageAllocator;
    std::unique_ptr<SceneDescriptorPool>                     m_textureDescriptorPool;
    std::unique_ptr<SceneDescriptorPool>                     m_crossFadeDescriptorPool;
    std::unique_ptr<SceneDescriptorPool>                     m_colorDescriptorPool;
    VulkanCommandPool                                        m_presentCommandPool;
    std::unique_ptr<VulkanSwapchain>                         m_swapchain;
    VulkanQueue                                              m_graphicsQueue;
    VulkanQueue                                              m_presentQueue;
    VulkanRenderPass                                         m_clearRenderPass;
    VulkanRenderPass                                         m_transitionRenderPass;
    VulkanRenderPass                                         m_loadRenderPass;
    VkFormat                                                 m_surfaceFormat;
    VkColorSpaceKHR                                          m_surfaceColorSpace;
    VkPresentModeKHR                                         m_presentMode;
    VkPhysicalDeviceProperties                               m_physicalDeviceProperties;
    uint32_t                                                 m_maxDiscardRects;
    uint32_t                                                 m_maxPushDescriptors;
    VkImageLayout                                            m_surfaceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::array<FrameData, FramesInFlight>                    m_frameData;
    std::array<PaintPassData, FramesInFlight>                m_paintPassData;
    SharedVulkanObjectSet<VulkanSemaphore>                   m_waitSemaphores;
    std::shared_ptr<VulkanBuffer>                            m_indexBuffer;
    std::shared_ptr<VulkanDeviceMemory>                      m_indexBufferMemory;
    uint32_t                                                 m_indexBufferQuadCount = 0;
    std::shared_ptr<ColorDescriptorSet>                      m_clearDescriptorSet;
    VulkanSampler                                            m_nearestSampler;
    VulkanSampler                                            m_linearSampler;
    QMatrix4x4                                               m_projectionMatrix;
    QMatrix4x4                                               m_screenProjectionMatrix;
    int                                                      m_frameIndex = 0;
    int                                                      m_paintPassIndex = 0;
    int                                                      m_bufferAge = 0;
    bool                                                     m_valid = false;
    bool                                                     m_separatePresentQueue = false;
    bool                                                     m_needQueueOwnershipTransfer = false;
    bool                                                     m_imageAcquired = false;
    bool                                                     m_renderPassStarted = false;
    bool                                                     m_commandBuffersPending = false;
    bool                                                     m_clearPending = false;
    bool                                                     m_usedIndexBuffer = false;
    bool                                                     m_haveMaintenance1 = false;
};

} // namespace KWin

#endif // VULKAN_SCENE_H
