#include "vulkan_allocator.h"
#include "core/graphicsbuffer.h"
#include "vulkan_device.h"
#include "vulkan_logging.h"

namespace KWin
{

VulkanGraphicsBufferAllocator::VulkanGraphicsBufferAllocator(VulkanDevice *device)
    : m_device(device)
{
}

GraphicsBuffer *VulkanGraphicsBufferAllocator::allocate(const GraphicsBufferOptions &options)
{
    const auto format = FormatInfo::get(options.format);
    if (!format) {
        qCWarning(KWIN_VULKAN, "Dmabuf has unknown format");
        return nullptr;
    }
    vk::ImageDrmFormatModifierListCreateInfoEXT modifierInfo{options.modifiers};
    vk::ExternalMemoryImageCreateInfo externalInfo{
        vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT,
        &modifierInfo,
    };
    // TODO how to query disjoint-ness since we don't have a dmabuf yet?
    const bool disjoint = false;
    vk::ImageCreateInfo imageInfo{
        disjoint ? vk::ImageCreateFlagBits::eDisjoint : vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format(format->vulkanFormat),
        vk::Extent3D(options.size.width(), options.size.height(), 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eDrmFormatModifierEXT,
        // FIXME options might need a usage flags parameter of some sort
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive,
        // the queue family index is ignored with share mode exclusive,
        // instead Vulkan implicitly assigns ownership to the first queue
        // the image is used in.
        0,
        vk::ImageLayout::eUndefined,
        &externalInfo,
    };
    auto [imageResult, image] = m_device->logicalDevice().createImage(imageInfo);
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating vulkan image failed!" << vk::to_string(imageResult);
        return nullptr;
    }

    const uint32_t memoryCount = disjoint ? attributes->planeCount : 1;
    std::vector<vk::BindImageMemoryInfo> bindInfos;
    bindInfos.resize(memoryCount);
    std::array<vk::BindImagePlaneMemoryInfo, 4> planeInfo;
    std::vector<vk::raii::DeviceMemory> deviceMemory;

    std::array<FileDescriptor, 4> duplicatedFds;
    for (size_t i = 0; i < memoryCount; i++) {
        duplicatedFds[i] = attributes->fd[i].duplicate();
    }

    for (uint32_t i = 0; i < memoryCount; i++) {
        const auto [memoryFdResult, memoryFdProperties] = m_logical.getMemoryFdPropertiesKHR(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFds[i].get());
        if (memoryFdResult != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN) << "failed to get memory fd properties!" << vk::to_string(memoryFdResult);
            return nullptr;
        }
        vk::ImageMemoryRequirementsInfo2 memRequirementsInfo{image};
        vk::ImagePlaneMemoryRequirementsInfo planeRequirementsInfo;
        if (disjoint) {
            switch (i) {
            case 0:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane0EXT);
                break;
            case 1:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane1EXT);
                break;
            case 2:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane2EXT);
                break;
            case 3:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane3EXT);
                break;
            }
            memRequirementsInfo.setPNext(&planeRequirementsInfo);
        }
        const vk::MemoryRequirements2 memRequirements = m_logical.getImageMemoryRequirements2(memRequirementsInfo);
        const auto memoryIndex = findMemoryType(memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits, {});
        if (!memoryIndex) {
            qCWarning(KWIN_VULKAN, "couldn't find a suitable memory type for %x & %x = %x",
                      memRequirements.memoryRequirements.memoryTypeBits, memoryFdProperties.memoryTypeBits,
                      memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits);
            return nullptr;
        }

        vk::MemoryDedicatedAllocateInfo dedicatedInfo{disjoint ? nullptr : *image};
        vk::ImportMemoryFdInfoKHR importInfo(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFds[i].get(), &dedicatedInfo);
        vk::MemoryAllocateInfo memoryInfo(memRequirements.memoryRequirements.size, memoryIndex.value(), &importInfo);
        auto [allocateResult, memory] = m_logical.allocateMemory(memoryInfo);
        if (allocateResult != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN, "'Allocating' memory for dmabuf failed: %s", vk::to_string(allocateResult).c_str());
            return nullptr;
        }

        bindInfos[i] = vk::BindImageMemoryInfo{image, memory, 0};
        if (disjoint) {
            planeInfo[i] = vk::BindImagePlaneMemoryInfo{
                planeRequirementsInfo.planeAspect,
            };
            bindInfos[i].setPNext(&planeInfo[i]);
        }
        deviceMemory.push_back(std::move(memory));
    }
    const vk::Result bindResult = m_logical.bindImageMemory2(bindInfos);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to bind image to memory";
        return nullptr;
    }
    // on successful import, the driver takes ownership of the file descriptors
    for (FileDescriptor &fd : duplicatedFds) {
        fd.take();
    }

    vk::ImageViewCreateInfo viewInfo{
        vk::ImageViewCreateFlags{},
        image,
        vk::ImageViewType::e2D,
        vk::Format(format->vulkanFormat),
        format->swizzles,
        vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1,
        },
    };
    auto [viewResult, view] = m_logical.createImageView(viewInfo);
    if (viewResult != vk::Result::eSuccess) {
        return nullptr;
    }

    vk::raii::Sampler sampler{nullptr};
    if (usage & vk::ImageUsageFlagBits::eSampled) {
        sampler = VulkanTexture::createSampler(this);
        if (!*sampler) {
            return nullptr;
        }
    }

    return std::make_shared<VulkanTexture>(this, vk::Format(format->vulkanFormat), std::move(image),
                                           std::move(view), std::move(sampler), std::move(deviceMemory),
                                           QSize(attributes->width, attributes->height));
}

}
