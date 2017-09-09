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

#include "kwinvulkanutils.h"

#include "kwineffects.h"
#include "../utils.h"
#include "logging_p.h"

#include <QFile>
#include <QDebug>


namespace KWin {



#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))



// Finds the first struct of type sType in the pNext chain given by pNext
static const void *findNextImpl(const void *pNext, VkStructureType sType)
{
    struct Header {
        VkStructureType sType;
        const void *pNext;
    };

    while (pNext) {
        const Header *header = static_cast<const Header *>(pNext);
        if (header->sType == sType)
            return pNext;

        pNext = header->pNext;
    }

    return nullptr;
}


// Returns the VkStructureType enumerator corresponding to T
template <typename T> constexpr VkStructureType getStructureType() { return VK_STRUCTURE_TYPE_MAX_ENUM; }

template <> constexpr VkStructureType getStructureType<VkMemoryDedicatedRequirementsKHR>() { return VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR; }
template <> constexpr VkStructureType getStructureType<VkImportMemoryFdInfoKHR>()          { return VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR; }
template <> constexpr VkStructureType getStructureType<VkExportMemoryAllocateInfoKHR>()    { return VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR; }


// Finds the first struct of type T in the pNext chain given by pNext
template <typename T>
const T *findNext(const void *pNext)
{
    const VkStructureType sType = getStructureType<T>();
    static_assert(sType != VK_STRUCTURE_TYPE_MAX_ENUM);
    return static_cast<const T *>(findNextImpl(pNext, sType));
}



// ------------------------------------------------------------------



VulkanDeviceMemoryAllocator::VulkanDeviceMemoryAllocator(VulkanDevice *device, const std::vector<const char *> &enabledDeviceExtensions)
    : m_device(device)
{
    VulkanPhysicalDevice gpu = device->physicalDevice();
    gpu.getMemoryProperties(&m_memoryProperties);

    auto extensionEnabled = [&](const char *extension) -> bool {
        return std::find_if(enabledDeviceExtensions.cbegin(), enabledDeviceExtensions.cend(),
                            [=](const char *entry) { return strcmp(extension, entry) == 0; }) != enabledDeviceExtensions.cend();
    };

    m_haveExternalMemory         = extensionEnabled(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    m_haveExternalMemoryFd       = extensionEnabled(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    m_haveDedicatedAllocation    = extensionEnabled(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    m_haveGetMemoryRequirements2 = extensionEnabled(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    m_haveBindMemory2            = extensionEnabled(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    m_haveExternalMemoryDmaBuf   = extensionEnabled(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
}


VulkanDeviceMemoryAllocator::~VulkanDeviceMemoryAllocator()
{
}


int VulkanDeviceMemoryAllocator::findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags mask) const
{
    for (unsigned int i = 0; i < m_memoryProperties.memoryTypeCount; i++) {
        if (memoryTypeBits & (1 << i) &&
            (m_memoryProperties.memoryTypes[i].propertyFlags & mask) == mask) {
            return i;
        }
    }

    return -1;
}


std::shared_ptr<VulkanDeviceMemory> VulkanDeviceMemoryAllocator::allocateMemory(VkDeviceSize size,
                                                                                uint32_t memoryTypeBits,
                                                                                VkMemoryPropertyFlags optimal,
                                                                                VkMemoryPropertyFlags required)
{
    return allocateMemoryInternal(size, memoryTypeBits, optimal, required, nullptr);
}


std::shared_ptr<VulkanDeviceMemory> VulkanDeviceMemoryAllocator::allocateMemory(std::shared_ptr<VulkanImage> &image,
                                                                                VkMemoryPropertyFlags optimal, VkMemoryPropertyFlags required)
{
    // Out
    VkMemoryDedicatedRequirementsKHR dedicatedRequirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR,
        .pNext = nullptr,
        .prefersDedicatedAllocation = VK_FALSE,
        .requiresDedicatedAllocation = VK_FALSE
    };

    VkMemoryRequirements2KHR memoryRequirements2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
        .pNext = &dedicatedRequirements,
        .memoryRequirements = {}
    };

    if (m_haveGetMemoryRequirements2 && m_haveDedicatedAllocation) {
        m_device->getImageMemoryRequirements2KHR(VkImageMemoryRequirementsInfo2KHR {
                                                      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
                                                      .pNext = nullptr,
                                                      .image = image->handle()
                                                 }, &memoryRequirements2);
    } else {
        m_device->getImageMemoryRequirements(image->handle(), &memoryRequirements2.memoryRequirements);
    }

    const VkMemoryRequirements &memoryRequirements = memoryRequirements2.memoryRequirements;

    const VkMemoryDedicatedAllocateInfoKHR dedicatedAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
        .pNext = nullptr,
        .image = image->handle(),
        .buffer = VK_NULL_HANDLE
    };

    const void *pAllocateInfoNext = nullptr;

    if (dedicatedRequirements.prefersDedicatedAllocation || dedicatedRequirements.requiresDedicatedAllocation)
        pAllocateInfoNext = &dedicatedAllocateInfo;

    auto memory = allocateMemoryInternal(memoryRequirements.size, memoryRequirements.memoryTypeBits,
                                         optimal, required, pAllocateInfoNext);

    if (memory) {
        m_device->bindImageMemory(image->handle(), memory->handle(), 0);
    }

    return memory;
}


std::shared_ptr<VulkanDeviceMemory> VulkanDeviceMemoryAllocator::allocateMemory(std::shared_ptr<VulkanBuffer> &buffer,
                                                                                VkMemoryPropertyFlags optimal, VkMemoryPropertyFlags required)
{
    // Out
    VkMemoryDedicatedRequirementsKHR dedicatedRequirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR,
        .pNext = nullptr,
        .prefersDedicatedAllocation = VK_FALSE,
        .requiresDedicatedAllocation = VK_FALSE
    };

    VkMemoryRequirements2KHR memoryRequirements2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
        .pNext = &dedicatedRequirements,
        .memoryRequirements = {}
    };

    if (m_haveGetMemoryRequirements2 && m_haveDedicatedAllocation) {
        m_device->getBufferMemoryRequirements2KHR(VkBufferMemoryRequirementsInfo2KHR {
                                                      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR,
                                                      .pNext = nullptr,
                                                      .buffer = buffer->handle()
                                                  }, &memoryRequirements2);
    } else {
        m_device->getBufferMemoryRequirements(buffer->handle(), &memoryRequirements2.memoryRequirements);
    }

    const VkMemoryRequirements &memoryRequirements = memoryRequirements2.memoryRequirements;

    const VkMemoryDedicatedAllocateInfoKHR dedicatedAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
        .pNext = nullptr,
        .image = VK_NULL_HANDLE,
        .buffer = buffer->handle()
    };

    const void *pAllocateInfoNext = nullptr;

    if (dedicatedRequirements.prefersDedicatedAllocation || dedicatedRequirements.requiresDedicatedAllocation)
        pAllocateInfoNext = &dedicatedAllocateInfo;

    auto memory = allocateMemoryInternal(memoryRequirements.size, memoryRequirements.memoryTypeBits,
                                         optimal, required, pAllocateInfoNext);

    if (memory) {
        m_device->bindBufferMemory(buffer->handle(), memory->handle(), 0);
    }

    return memory;
}


std::shared_ptr<VulkanDeviceMemory> VulkanDeviceMemoryAllocator::allocateMemoryInternal(VkDeviceSize size,
                                                                                        uint32_t memoryTypeBits,
                                                                                        VkMemoryPropertyFlags optimal,
                                                                                        VkMemoryPropertyFlags required,
                                                                                        const void *pAllocateInfoNext)
{
    VkDeviceMemory memory = VK_NULL_HANDLE;
    int index = -1;

    const auto *dedicatedRequirements    = findNext<VkMemoryDedicatedRequirementsKHR>(pAllocateInfoNext);
    const auto *importMemoryFdInfo       = findNext<VkImportMemoryFdInfoKHR>(pAllocateInfoNext);
    const auto *exportMemoryAllocateInfo = findNext<VkExportMemoryAllocateInfoKHR>(pAllocateInfoNext);

    while (memory == VK_NULL_HANDLE) {
        index = findMemoryType(memoryTypeBits, optimal);

        if (index == -1)
            index = findMemoryType(memoryTypeBits, required);

        if (index == -1)
            break;

        const VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = pAllocateInfoNext,
            .allocationSize = size,
            .memoryTypeIndex = (uint32_t) index
        };

        VkResult result = m_device->allocateMemory(&allocateInfo, nullptr, &memory);
        if (result != VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
                // Clear the memory type bit and try again
                memoryTypeBits &= ~(1 << index);
                continue;
            }
            break;
        }
    }

    if (memory == VK_NULL_HANDLE) {
        qCCritical(LIBKWINVULKANUTILS) << "Failed to allocate" << size << "bytes of device memory";
        return std::shared_ptr<VulkanDeviceMemory>();
    }

    return std::make_shared<VulkanDeviceMemory>(VulkanDeviceMemory::CreateInfo {
                                                    .device = m_device,
                                                    .memory = memory,
                                                    .size = size,
                                                    .flags = m_memoryProperties.memoryTypes[index].propertyFlags,
                                                    .exportableHandleTypes = exportMemoryAllocateInfo ?
                                                            exportMemoryAllocateInfo->handleTypes : 0,
                                                    .dedicated = dedicatedRequirements != nullptr,
                                                    .imported = importMemoryFdInfo != nullptr
                                                });
}



// --------------------------------------------------------------------



VulkanShaderModule::VulkanShaderModule(VulkanDevice *device, const QString &fileName)
    : VulkanObject(),
      m_device(device),
      m_shaderModule(VK_NULL_HANDLE)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) {
        qCCritical(LIBKWINVULKANUTILS) << "Failed to open" << fileName << "for reading";
        return;
    }

    const QByteArray code = file.readAll();

    const VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = (size_t) code.size(),
        .pCode    = (uint32_t *) code.constData()
    };

    if (m_device->createShaderModule(&createInfo, nullptr, &m_shaderModule) != VK_SUCCESS) {
        qCCritical(LIBKWINVULKANUTILS) << "Failed to create shader module for" << fileName;
    }
}



// --------------------------------------------------------------------



VulkanPipelineManager::VulkanPipelineManager(VulkanDevice *device, VulkanPipelineCache *cache,
                                             VkSampler nearestSampler, VkSampler linearSampler,
                                             VkRenderPass swapchainRenderPass, VkRenderPass offscreenRenderPass,
                                             bool havePushDescriptors)
    : m_device(device),
      m_pipelineCache(cache),
      m_nearestSampler(nearestSampler),
      m_linearSampler(linearSampler),
      m_havePushDescriptors(havePushDescriptors)
{
    m_renderPasses[SwapchainRenderPass] = swapchainRenderPass;
    m_renderPasses[OffscreenRenderPass] = offscreenRenderPass;

    for (int i = 0; i < MaterialCount; i++) {
        m_descriptorSetLayout[i] = VK_NULL_HANDLE;
        m_pipelineLayout[i] = VK_NULL_HANDLE;
        m_pushDescriptorSetLayout[i] = VK_NULL_HANDLE;
        m_pushDescriptorPipelineLayout[i] = VK_NULL_HANDLE;
    }

    const struct {
        QString fileName;
        VulkanShaderModule &module;
    } modules[] = {
        { QStringLiteral(":/resources/shaders/vulkan/color_vert.spv"),            m_colorVertexShader              },
        { QStringLiteral(":/resources/shaders/vulkan/texture_vert.spv"),          m_textureVertexShader            },
        { QStringLiteral(":/resources/shaders/vulkan/crossfade_vert.spv"),        m_crossFadeVertexShader          },
        { QStringLiteral(":/resources/shaders/vulkan/updatedecoration_vert.spv"), m_updateDecorationVertexShader   },

        { QStringLiteral(":/resources/shaders/vulkan/color_frag.spv"),            m_colorFragmentShader            },
        { QStringLiteral(":/resources/shaders/vulkan/texture_frag.spv"),          m_textureFragmentShader          },
        { QStringLiteral(":/resources/shaders/vulkan/modulate_frag.spv"),         m_modulateFragmentShader         },
        { QStringLiteral(":/resources/shaders/vulkan/desaturate_frag.spv"),       m_desaturateFragmentShader       },
        { QStringLiteral(":/resources/shaders/vulkan/crossfade_frag.spv"),        m_crossFadeFragmentShader        },
        { QStringLiteral(":/resources/shaders/vulkan/updatedecoration_frag.spv"), m_updateDecorationFragmentShader },
    };

    m_valid = true;

    for (auto &entry : modules) {
        entry.module = VulkanShaderModule(m_device, entry.fileName);
        m_valid = m_valid && entry.module.isValid();
    }

    m_valid = m_valid && createDescriptorSetLayouts();
    m_valid = m_valid && createPipelineLayouts();
}


VulkanPipelineManager::~VulkanPipelineManager()
{
    for (const std::pair<uint64_t, VkPipeline> &entry : m_pipelines) {
        m_device->destroyPipeline(entry.second);
    }

    for (auto layout : m_pipelineLayout) {
        if (layout) {
            m_device->destroyPipelineLayout(layout);
        }
    }

    for (auto layout : m_pushDescriptorPipelineLayout) {
        if (layout) {
            m_device->destroyPipelineLayout(layout);
        }
    }

    for (auto layout : m_descriptorSetLayout) {
        if (layout) {
            m_device->destroyDescriptorSetLayout(layout);
        }
    }

    for (auto layout : m_pushDescriptorSetLayout) {
        if (layout) {
            m_device->destroyDescriptorSetLayout(layout);
        }
    }
}


bool VulkanPipelineManager::createDescriptorSetLayouts()
{
    bool valid = true;

    struct CreateInfo {
        VkDescriptorSetLayoutCreateFlags flags;
        std::initializer_list<VkDescriptorSetLayoutBinding> bindings;
    };

    auto create = [&](const CreateInfo &info) -> VkDescriptorSetLayout {
        VkDescriptorSetLayout layout;
        VkResult result;

        if ((info.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) && !m_havePushDescriptors)
            return VK_NULL_HANDLE;

        result = m_device->createDescriptorSetLayout({
                                                         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                         .pNext = nullptr,
                                                         .flags = info.flags,
                                                         .bindingCount = (uint32_t) info.bindings.size(),
                                                         .pBindings = info.bindings.begin()
                                                     }, nullptr, &layout);

        if (result != VK_SUCCESS) {
            qCCritical(LIBKWINVULKANUTILS) << "Failed to create a descriptor set layout";
            valid = false;
        }

        return layout;
    };


    // layout (set = N, binding = 0) uniform UBO {
    //     mat4 matrix;
    //     vec4 color;
    // }
    m_descriptorSetLayout[FlatColor] =
            create({
                       .flags = 0,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           }
                       }
                   });

    m_pushDescriptorSetLayout[FlatColor] =
            create({
                       .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           }
                       }
                   });


    // layout (set = N, binding = 0) uniform sampler2D texture;
    // layout (set = N, binding = 1) uniform UBO {
    //     mat4 matrix;
    //     ...
    // }
    m_descriptorSetLayout[Texture] =
            create({
                       .flags = 0,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 1,
                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           }
                       }
                   });

    m_pushDescriptorSetLayout[Texture] =
            create({
                       .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 1,
                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           }
                       }
                   });

    // layout (set = N, binding = 0) uniform texture2D textures[2];
    // layout (set = N, binding = 1) uniform sampler sampler {
    // layout (set = N, binding = 2) uniform UBO {
    //     mat4 matrix;
    //     ...
    // }
    m_descriptorSetLayout[TwoTextures] =
            create({
                       .flags = 0,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                               .descriptorCount = 2,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 1,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 2,
                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           }
                       }
                   });


    m_pushDescriptorSetLayout[TwoTextures] =
            create({
                       .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                               .descriptorCount = 2,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 1,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 2,
                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           }
                       }
                   });

    // layout (set = N, binding = 0) uniform texture2D textures[4];
    // layout (set = N, binding = 1) uniform sampler sampler {
    m_descriptorSetLayout[DecorationStagingImages] =
            create({
                       .flags = 0,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                               .descriptorCount = 4,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 1,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = &m_nearestSampler
                           }
                       }
                   });

    m_pushDescriptorSetLayout[DecorationStagingImages] =
            create({
                       .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                       .bindings = {
                           {
                               .binding = 0,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                               .descriptorCount = 4,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = nullptr
                           },
                           {
                               .binding = 1,
                               .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                               .descriptorCount = 1,
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pImmutableSamplers = &m_nearestSampler
                           }
                       }
                   });

    return valid;
}


bool VulkanPipelineManager::createPipelineLayouts()
{
    bool valid = true;

    auto create = [&](const std::initializer_list<VkDescriptorSetLayout> setLayouts,
                      const std::initializer_list<VkPushConstantRange> pushConstantRanges = {}) {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkResult result = m_device->createPipelineLayout({
                                                             .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                             .pNext = nullptr,
                                                             .flags = 0,
                                                             .setLayoutCount = (uint32_t) setLayouts.size(),
                                                             .pSetLayouts = setLayouts.begin(),
                                                             .pushConstantRangeCount = (uint32_t) pushConstantRanges.size(),
                                                             .pPushConstantRanges = pushConstantRanges.begin()
                                                         }, nullptr, &layout);
        if (result != VK_SUCCESS) {
            qCCritical(LIBKWINVULKANUTILS) << "Failed to create a pipeline layout";
            valid = false;
        }

        return layout;
    };

    for (int i = 0; i <= TwoTextures; i++) {
        m_pipelineLayout[i] = create({m_descriptorSetLayout[i]});

        if (m_havePushDescriptors)
            m_pushDescriptorPipelineLayout[i] = create({m_pushDescriptorSetLayout[i]});
        else
            m_pushDescriptorPipelineLayout[i] = VK_NULL_HANDLE;
    }

    m_pipelineLayout[DecorationStagingImages] =
            create({
                       m_descriptorSetLayout[DecorationStagingImages]
                   },
                   {
                       {
                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                           .offset = 0,
                           .size = sizeof(uint32_t)
                       }
                   });

    if (m_havePushDescriptors) {
        m_pushDescriptorPipelineLayout[DecorationStagingImages] =
                create({
                           m_pushDescriptorSetLayout[DecorationStagingImages]
                       },
                       {
                           {
                               .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .offset = 0,
                               .size = sizeof(uint32_t)
                           }
                       });
    } else {
        m_pushDescriptorPipelineLayout[DecorationStagingImages] = VK_NULL_HANDLE;
    }

    return valid;
}


VkPipeline VulkanPipelineManager::createPipeline(Material material, Traits traits, DescriptorType descriptorType, Topology topology, VkRenderPass renderPass)
{
    // Viewport state
    // --------------
    static const VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = 1.0f,
        .height = 1.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    static const VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = { 1, 1 }
    };

    const VkPipelineViewportStateCreateInfo viewportState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };


    // Rasterization state
    // -------------------
    static const VkPipelineRasterizationStateCreateInfo rasterizationState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };


    // Multisample state
    // -----------------
    static const VkPipelineMultisampleStateCreateInfo multisampleState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };


    // Dynamic states
    //---------------
    static const VkDynamicState dynamicStates[] {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineDynamicStateCreateInfo dynamicState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = ARRAY_SIZE(dynamicStates),
        .pDynamicStates = dynamicStates
    };


    // Depth-stencil state
    // -------------------
    static const VkPipelineDepthStencilStateCreateInfo depthStencilState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_NEVER,
            .compareMask = 0,
            .writeMask = 0,
            .reference = 0
        },
        .back = {
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_NEVER,
            .compareMask = 0,
            .writeMask = 0,
            .reference = 0
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };


    // Color blend attachment states
    // -----------------------------
    static const VkPipelineColorBlendAttachmentState preMultipliedBlendAttachment {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = 0xf
    };

    static const VkPipelineColorBlendAttachmentState opaqueBlendAttachment {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = 0xf
    };


    // Color blend states
    // ------------------
    const VkPipelineColorBlendStateCreateInfo preMultipliedAlphaBlendState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &preMultipliedBlendAttachment,
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    const VkPipelineColorBlendStateCreateInfo opaqueBlendState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &opaqueBlendAttachment,
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
    };



    // Input assembly states
    // ---------------------
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = (VkPrimitiveTopology) topology,
        .primitiveRestartEnable = VK_FALSE
    };


    // Color inputs
    // ------------
    static const VkVertexInputAttributeDescription colorVertexInputAttribs[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
    };

    static const VkVertexInputBindingDescription colorVertexInputBindings[] {
        { .binding = 0, .stride = sizeof(QVector2D), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
    };

    const VkPipelineVertexInputStateCreateInfo colorVertexInputState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = ARRAY_SIZE(colorVertexInputBindings),
        .pVertexBindingDescriptions = colorVertexInputBindings,
        .vertexAttributeDescriptionCount = ARRAY_SIZE(colorVertexInputAttribs),
        .pVertexAttributeDescriptions = colorVertexInputAttribs
    };


    // Texture inputs
    // --------------
    static const VkVertexInputAttributeDescription textureVertexInputAttribs[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(GLVertex2D, position) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(GLVertex2D, texcoord) },
    };

    static const VkVertexInputBindingDescription textureVertexInputBindings[] {
        { .binding = 0, .stride = sizeof(GLVertex2D), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
    };

    const VkPipelineVertexInputStateCreateInfo textureVertexInputState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = ARRAY_SIZE(textureVertexInputBindings),
        .pVertexBindingDescriptions = textureVertexInputBindings,
        .vertexAttributeDescriptionCount = ARRAY_SIZE(textureVertexInputAttribs),
        .pVertexAttributeDescriptions = textureVertexInputAttribs
    };


    // Cross-fade inputs
    // -----------------
    static const VkVertexInputAttributeDescription crossFadeVertexInputAttribs[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(GLCrossFadeVertex2D, position)  },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(GLCrossFadeVertex2D, texcoord1) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(GLCrossFadeVertex2D, texcoord2) },
    };

    static const VkVertexInputBindingDescription crossFadeVertexInputBindings[] {
        { .binding = 0, .stride = sizeof(GLCrossFadeVertex2D), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
    };

    const VkPipelineVertexInputStateCreateInfo crossFadeVertexInputState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = ARRAY_SIZE(crossFadeVertexInputBindings),
        .pVertexBindingDescriptions = crossFadeVertexInputBindings,
        .vertexAttributeDescriptionCount = ARRAY_SIZE(crossFadeVertexInputAttribs),
        .pVertexAttributeDescriptions = crossFadeVertexInputAttribs
    };

    const VkPipelineVertexInputStateCreateInfo *vertexInputState = nullptr;
    const VkPipelineColorBlendStateCreateInfo *blendState = nullptr;
    VkShaderModule fs = VK_NULL_HANDLE;
    VkShaderModule vs = VK_NULL_HANDLE;
    VkPipelineLayout layout;

    if (descriptorType == PushDescriptors)
        layout = m_pushDescriptorPipelineLayout[material];
    else
        layout = m_pipelineLayout[material];

    if (traits & PreMultipliedAlphaBlend)
        blendState = &preMultipliedAlphaBlendState;
    else
        blendState = &opaqueBlendState;

    switch (material) {
    case FlatColor:
        vertexInputState = &colorVertexInputState;
        vs = m_colorVertexShader;
        fs = m_colorFragmentShader;
        break;

    case Texture:
        vertexInputState = &textureVertexInputState;
        vs = m_textureVertexShader;
        if (traits & Desaturate)
            fs = m_desaturateFragmentShader;
        else if (traits & Modulate)
            fs = m_modulateFragmentShader;
        else
            fs = m_textureFragmentShader;
        break;

    case TwoTextures:
        vertexInputState = &crossFadeVertexInputState;
        vs = m_crossFadeVertexShader;
        fs = m_crossFadeFragmentShader;
        break;

    case DecorationStagingImages:
        vertexInputState = &textureVertexInputState;
        vs = m_updateDecorationVertexShader;
        fs = m_updateDecorationFragmentShader;
        break;

    default:
        break;
    };


    // Shader stages
    // -------------
    const VkPipelineShaderStageCreateInfo pipelineStages[] {
        // Vertex shader
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vs,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        // Fragment shader
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fs,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    const VkGraphicsPipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = ARRAY_SIZE(pipelineStages),
        .pStages = pipelineStages,
        .pVertexInputState = vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pTessellationState = nullptr,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = blendState,
        .pDynamicState = &dynamicState,
        .layout = layout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };

    VkPipeline pipeline;
    const VkResult result = m_device->createGraphicsPipelines(m_pipelineCache->handle(), 1, &createInfo, nullptr, &pipeline);

    if (result != VK_SUCCESS) {
        qCCritical(LIBKWINVULKANUTILS) << "vkCreateGraphicsPipelines returned" << enumToString(result);
        return VK_NULL_HANDLE;
    }

    return pipeline;
}


std::tuple<VkPipeline, VkPipelineLayout> VulkanPipelineManager::pipeline(Material material,
                                                                         Traits traits,
                                                                         DescriptorType descriptorType,
                                                                         Topology topology,
                                                                         RenderPassType renderPassType)
{
    union {
        struct {
            unsigned material:3;
            unsigned traits:5;
            unsigned topology:3;
            unsigned descriptorType:1;
            unsigned renderPassType:1;
            unsigned unused:19;
        } u;
        uint32_t value;
    } key;

    key.value = 0;

    key.u.material       = material;
    key.u.traits         = traits;
    key.u.topology       = topology;
    key.u.descriptorType = descriptorType;
    key.u.renderPassType = renderPassType;

    VkPipeline &pipe = m_pipelines[key.value];

    if (pipe == VK_NULL_HANDLE)
        pipe = createPipeline(material, traits, descriptorType, topology, m_renderPasses[renderPassType]);

    const VkPipelineLayout layout = descriptorType == PushDescriptors ?
            m_pushDescriptorPipelineLayout[material] :
            m_pipelineLayout[material];

    return std::tuple<VkPipeline, VkPipelineLayout>(pipe, layout);
}



// --------------------------------------------------------------------



class VulkanCircularAllocatorBase::CircularBuffer : public std::enable_shared_from_this<CircularBuffer>
{
public:
    struct BusyOffset {
        BusyOffset(const BusyOffset &) = delete;
        BusyOffset(BusyOffset &&) = default;
        ~BusyOffset() { if (m_buffer) m_buffer->m_tail = m_offset; }
        BusyOffset &operator = (const BusyOffset &) = delete;
        BusyOffset &operator = (BusyOffset &&) = default;
        std::shared_ptr<CircularBuffer> m_buffer;
        uint32_t m_offset;
    };

    CircularBuffer(const std::shared_ptr<VulkanBuffer> &buffer, const std::shared_ptr<VulkanDeviceMemory> &memory, uint8_t *data, uint32_t size)
        : m_buffer(buffer), m_memory(memory), m_size(size), m_data(data) {}

    CircularBuffer(const std::shared_ptr<VulkanDeviceMemory> &memory, uint8_t *data, uint32_t size)
        : m_buffer(nullptr), m_memory(memory), m_size(size), m_data(data) {}

    bool allocate(size_t size, uint32_t alignment, uint32_t *pOffset);

    bool isBusy() const { return m_head != m_lastBusyOffset; }

    void getNonCoherentAllocatedRanges(std::back_insert_iterator<std::vector<VkMappedMemoryRange>> &it, VkDeviceSize nonCoherentAtomSize);
    BusyOffset busyOffset();

    std::shared_ptr<VulkanBuffer> buffer() const { return m_buffer; }
    std::shared_ptr<VulkanDeviceMemory> memory() const { return m_memory; }
    VkDeviceMemory memoryHandle() const { return m_memory->handle(); }
    uint32_t size() const { return m_size; }
    uint8_t *data() const { return m_data; }

private:
    std::shared_ptr<VulkanBuffer> m_buffer;
    std::shared_ptr<VulkanDeviceMemory> m_memory;
    uint32_t m_head = 0;
    uint32_t m_tail = 0;
    uint32_t m_size = 0;
    uint32_t m_nextWrapAroundOffset = 0;
    uint32_t m_lastBusyOffset = 0;
    uint32_t m_flushedOffset = 0;
    uint8_t *m_data = nullptr;

    friend class BusyOffset;
};


bool VulkanCircularAllocatorBase::CircularBuffer::allocate(size_t allocationSize, uint32_t alignment, uint32_t *pOffset)
{
    // Make sure the allocation will fit in the buffer
    if (allocationSize > m_size)
        return false;

    // Align the offset
    uint32_t offset = align(m_head, alignment);

    // Handle wrap around
    uint32_t wrapAroundOffset = m_nextWrapAroundOffset;
    if (offset + allocationSize > wrapAroundOffset) {
        offset = wrapAroundOffset;
        wrapAroundOffset += m_size;
    }

    // Make sure there is enough free space for the allocation
    if (m_tail + m_size - offset < allocationSize)
        return false;

    m_head = offset + allocationSize;
    m_nextWrapAroundOffset = wrapAroundOffset;

    *pOffset = offset & (m_size - 1);
    return true;
}


void VulkanCircularAllocatorBase::CircularBuffer::getNonCoherentAllocatedRanges(std::back_insert_iterator<std::vector<VkMappedMemoryRange>> &it, VkDeviceSize nonCoherentAtomSize)
{
    if (!m_memory || m_memory->isHostCoherent())
        return;

    if (m_flushedOffset == m_head)
        return;

    uint32_t start = m_flushedOffset & ~(nonCoherentAtomSize - 1); // Round down
    uint32_t end = align(m_head, nonCoherentAtomSize);             // Round up

    if ((end - start) >= m_buffer->size()) {
        *it++ = VkMappedMemoryRange {
             .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
             .pNext  = nullptr,
             .memory = m_memory->handle(),
             .offset = 0,
             .size   = VK_WHOLE_SIZE
        };
    } else {
        const uint32_t mask = m_buffer->size() - 1;
        start &= mask;
        end &= mask;

        assert(!(m_memory->size() & (nonCoherentAtomSize - 1)));
        assert(end <= m_memory->size());

        if (start < end) {
            *it++ = VkMappedMemoryRange {
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext  = nullptr,
                .memory = m_memory->handle(),
                .offset = start,
                .size   = end - start
            };
        } else {
            // We have wrapped around and have dirty ranges at both the beginning
            // and the end of the buffer
            *it++ = VkMappedMemoryRange {
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext  = nullptr,
                .memory = m_memory->handle(),
                .offset = 0,
                .size   = end
            };

            *it++ = VkMappedMemoryRange {
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext  = nullptr,
                .memory = m_memory->handle(),
                .offset = start,
                .size   = m_memory->size() - start
            };
        }
    }

    m_flushedOffset = m_head;
}


VulkanCircularAllocatorBase::CircularBuffer::BusyOffset VulkanCircularAllocatorBase::CircularBuffer::busyOffset()
{
    if (m_lastBusyOffset != m_head) {
        m_lastBusyOffset = m_head;
        return BusyOffset{shared_from_this(), m_head};
    }

    return BusyOffset{nullptr, 0};
}



// -------------------------------------------------------------------



VulkanCircularAllocatorBase::VulkanCircularAllocatorBase(VulkanDevice *device, uint32_t nonCoherentAtomSize)
    : m_device(device),
      m_nonCoherentAtomSize(nonCoherentAtomSize)
{
}


VulkanCircularAllocatorBase::~VulkanCircularAllocatorBase()
{
}


void VulkanCircularAllocatorBase::getNonCoherentAllocatedRanges(std::back_insert_iterator<std::vector<VkMappedMemoryRange>> it)
{
    for (auto &buffer : m_orphanedBuffers)
        buffer->getNonCoherentAllocatedRanges(it, m_nonCoherentAtomSize);

    if (m_circularBuffer)
        m_circularBuffer->getNonCoherentAllocatedRanges(it, m_nonCoherentAtomSize);
}


std::shared_ptr<VulkanObject> VulkanCircularAllocatorBase::createFrameBoundary()
{
    struct FrameBoundary : public VulkanObject
    {
        FrameBoundary(CircularBuffer::BusyOffset &&offset, std::vector<std::shared_ptr<CircularBuffer>> &&buffers)
            : offset(std::move(offset)), buffers(std::move(buffers)) {}
        CircularBuffer::BusyOffset offset;
        std::vector<std::shared_ptr<CircularBuffer>> buffers;
    };

    std::shared_ptr<FrameBoundary> boundary;

    if (m_circularBuffer)
        boundary = std::make_shared<FrameBoundary>(m_circularBuffer->busyOffset(), std::move(m_orphanedBuffers));
    else
        boundary = std::make_shared<FrameBoundary>(CircularBuffer::BusyOffset{nullptr, 0}, std::move(m_orphanedBuffers));

    m_orphanedBuffers = std::vector<std::shared_ptr<CircularBuffer>>();
    return boundary;
}



// -------------------------------------------------------------------



VulkanUploadManager::VulkanUploadManager(VulkanDevice *device,
                                         VulkanDeviceMemoryAllocator *allocator,
                                         size_t initialSize,
                                         VkBufferUsageFlags bufferUsage,
                                         VkMemoryPropertyFlags optimalFlags,
                                         const VkPhysicalDeviceLimits &limits)
    : VulkanCircularAllocatorBase(device, limits.nonCoherentAtomSize),
      m_allocator(allocator),
      m_initialSize(initialSize),
      m_bufferUsage(bufferUsage),
      m_optimalMemoryFlags(optimalFlags),
      m_minTexelBufferOffsetAlignment(limits.minTexelBufferOffsetAlignment),
      m_minUniformBufferOffsetAlignment(limits.minUniformBufferOffsetAlignment),
      m_minStorageBufferOffsetAlignment(limits.minStorageBufferOffsetAlignment)
{
    // The size must be a power-of-two
    assert(initialSize == (initialSize & -initialSize));
}


VulkanUploadManager::~VulkanUploadManager()
{
}


VulkanBufferRange VulkanUploadManager::allocate(size_t size, uint32_t alignment)
{
    uint32_t offset = 0;

    if (!m_circularBuffer || !m_circularBuffer->allocate(size, alignment, &offset)) {
        reallocate(size);

        if (!m_circularBuffer || !m_circularBuffer->allocate(size, alignment, &offset))
            return VulkanBufferRange();
    }

    return VulkanBufferRange(m_circularBuffer->buffer(), offset, size, m_circularBuffer->data() + offset);
}


void VulkanUploadManager::reallocate(size_t minimumSize)
{
    size_t newSize = (m_circularBuffer && m_circularBuffer->size() > 0) ? m_circularBuffer->size() * 2 : m_initialSize;
    while (newSize < minimumSize)
        newSize *= 2;

    newSize = align(newSize, m_nonCoherentAtomSize);

    // The size must be a power-of-two
    assert(newSize == (newSize & -newSize));

    if (m_circularBuffer && m_circularBuffer->isBusy())
        m_orphanedBuffers.push_back(m_circularBuffer);

    m_circularBuffer = nullptr;

    auto buffer = std::make_shared<VulkanBuffer>(m_device,
                                                 VkBufferCreateInfo {
                                                     .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .size = newSize,
                                                     .usage = m_bufferUsage,
                                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                     .queueFamilyIndexCount = 0,
                                                     .pQueueFamilyIndices = nullptr
                                                 });

    if (buffer && buffer->isValid()) {
        auto memory = m_allocator->allocateMemory(buffer, m_optimalMemoryFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (memory && memory->isValid()) {
            uint8_t *data = nullptr;
            memory->map(0, (void **) &data);

            m_circularBuffer = std::make_shared<CircularBuffer>(buffer, memory, data, newSize);
        }
    }
}



// -------------------------------------------------------------------



VulkanStagingImageAllocator::VulkanStagingImageAllocator(VulkanDevice *device,
                                                         VulkanDeviceMemoryAllocator *allocator,
                                                         size_t initialSize,
                                                         VkMemoryPropertyFlags optimalMemoryFlags,
                                                         const VkPhysicalDeviceLimits &limits)
    : VulkanCircularAllocatorBase(device, limits.nonCoherentAtomSize),
      m_allocator(allocator),
      m_initialSize(initialSize),
      m_optimalMemoryFlags(optimalMemoryFlags)
{
    // The size must be a power-of-two
    assert(initialSize == (initialSize & -initialSize));
}


VulkanStagingImageAllocator::~VulkanStagingImageAllocator()
{
}


std::shared_ptr<VulkanStagingImage> VulkanStagingImageAllocator::createImage(const VkImageCreateInfo &createInfo)
{
    auto image = std::make_shared<VulkanStagingImage>(m_device, createInfo);
    if (!image->isValid())
        return nullptr;

    VkMemoryRequirements memoryRequirements;
    m_device->getImageMemoryRequirements(image->handle(), &memoryRequirements);

    uint32_t offset = 0;
    if (!m_circularBuffer || !m_circularBuffer->allocate(memoryRequirements.size, memoryRequirements.alignment, &offset)) {
        reallocate(memoryRequirements.size, memoryRequirements.memoryTypeBits);

        if (!m_circularBuffer || !m_circularBuffer->allocate(memoryRequirements.size, memoryRequirements.alignment, &offset))
            return nullptr;
    }

    m_device->bindImageMemory(image->handle(), m_circularBuffer->memoryHandle(), offset);
    image->setData(m_circularBuffer->data() + offset);

    return image;
}


void VulkanStagingImageAllocator::reallocate(size_t minimumSize, uint32_t memoryTypeBits)
{
    size_t newSize = (m_circularBuffer && m_circularBuffer->size() > 0) ? m_circularBuffer->size() * 2 : m_initialSize;
    while (newSize < minimumSize)
        newSize *= 2;

    newSize = align(newSize, m_nonCoherentAtomSize);

    // The size must be a power-of-two
    assert(newSize == (newSize & -newSize));

    if (m_circularBuffer && m_circularBuffer->isBusy())
        m_orphanedBuffers.push_back(m_circularBuffer);

    m_circularBuffer = nullptr;

    if (auto memory = m_allocator->allocateMemory(newSize, memoryTypeBits, m_optimalMemoryFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        uint8_t *data = nullptr;
        memory->map(0, (void **) &data);

        m_circularBuffer = std::make_shared<CircularBuffer>(memory, data, newSize);
    }
}



// --------------------------------------------------------------------


#define CASE(x) case (x): return QByteArrayLiteral(#x);

QByteArray enumToString(VkResult result)
{
    switch (result) {
    CASE(VK_SUCCESS)
    CASE(VK_NOT_READY)
    CASE(VK_TIMEOUT)
    CASE(VK_EVENT_SET)
    CASE(VK_EVENT_RESET)
    CASE(VK_INCOMPLETE)
    CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
    CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
    CASE(VK_ERROR_INITIALIZATION_FAILED)
    CASE(VK_ERROR_DEVICE_LOST)
    CASE(VK_ERROR_MEMORY_MAP_FAILED)
    CASE(VK_ERROR_LAYER_NOT_PRESENT)
    CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
    CASE(VK_ERROR_FEATURE_NOT_PRESENT)
    CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
    CASE(VK_ERROR_TOO_MANY_OBJECTS)
    CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
    CASE(VK_ERROR_FRAGMENTED_POOL)
    CASE(VK_ERROR_SURFACE_LOST_KHR)
    CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
    CASE(VK_SUBOPTIMAL_KHR)
    CASE(VK_ERROR_OUT_OF_DATE_KHR)
    CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
    CASE(VK_ERROR_VALIDATION_FAILED_EXT)
    CASE(VK_ERROR_INVALID_SHADER_NV)
    CASE(VK_ERROR_OUT_OF_POOL_MEMORY_KHR)
    CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR)
    default: return QByteArray::number(result);
    }
}

#undef CASE

QByteArray enumToString(VkFormat format)
{
    static const char * const formats[] = {
    	"VK_FORMAT_UNDEFINED",
        "VK_FORMAT_R4G4_UNORM_PACK8",
        "VK_FORMAT_R4G4B4A4_UNORM_PACK16",
        "VK_FORMAT_B4G4R4A4_UNORM_PACK16",
        "VK_FORMAT_R5G6B5_UNORM_PACK16",
        "VK_FORMAT_B5G6R5_UNORM_PACK16",
        "VK_FORMAT_R5G5B5A1_UNORM_PACK16",
        "VK_FORMAT_B5G5R5A1_UNORM_PACK16",
        "VK_FORMAT_A1R5G5B5_UNORM_PACK16",
        "VK_FORMAT_R8_UNORM",
        "VK_FORMAT_R8_SNORM",
        "VK_FORMAT_R8_USCALED",
        "VK_FORMAT_R8_SSCALED",
        "VK_FORMAT_R8_UINT",
        "VK_FORMAT_R8_SINT",
        "VK_FORMAT_R8_SRGB",
        "VK_FORMAT_R8G8_UNORM",
        "VK_FORMAT_R8G8_SNORM",
        "VK_FORMAT_R8G8_USCALED",
        "VK_FORMAT_R8G8_SSCALED",
        "VK_FORMAT_R8G8_UINT",
        "VK_FORMAT_R8G8_SINT",
        "VK_FORMAT_R8G8_SRGB",
        "VK_FORMAT_R8G8B8_UNORM",
        "VK_FORMAT_R8G8B8_SNORM",
        "VK_FORMAT_R8G8B8_USCALED",
        "VK_FORMAT_R8G8B8_SSCALED",
        "VK_FORMAT_R8G8B8_UINT",
        "VK_FORMAT_R8G8B8_SINT",
        "VK_FORMAT_R8G8B8_SRGB",
        "VK_FORMAT_B8G8R8_UNORM",
        "VK_FORMAT_B8G8R8_SNORM",
        "VK_FORMAT_B8G8R8_USCALED",
        "VK_FORMAT_B8G8R8_SSCALED",
        "VK_FORMAT_B8G8R8_UINT",
        "VK_FORMAT_B8G8R8_SINT",
        "VK_FORMAT_B8G8R8_SRGB",
        "VK_FORMAT_R8G8B8A8_UNORM",
        "VK_FORMAT_R8G8B8A8_SNORM",
        "VK_FORMAT_R8G8B8A8_USCALED",
        "VK_FORMAT_R8G8B8A8_SSCALED",
        "VK_FORMAT_R8G8B8A8_UINT",
        "VK_FORMAT_R8G8B8A8_SINT",
        "VK_FORMAT_R8G8B8A8_SRGB",
        "VK_FORMAT_B8G8R8A8_UNORM",
        "VK_FORMAT_B8G8R8A8_SNORM",
        "VK_FORMAT_B8G8R8A8_USCALED",
        "VK_FORMAT_B8G8R8A8_SSCALED",
        "VK_FORMAT_B8G8R8A8_UINT",
        "VK_FORMAT_B8G8R8A8_SINT",
        "VK_FORMAT_B8G8R8A8_SRGB",
        "VK_FORMAT_A8B8G8R8_UNORM_PACK32",
        "VK_FORMAT_A8B8G8R8_SNORM_PACK32",
        "VK_FORMAT_A8B8G8R8_USCALED_PACK32",
        "VK_FORMAT_A8B8G8R8_SSCALED_PACK32",
        "VK_FORMAT_A8B8G8R8_UINT_PACK32",
        "VK_FORMAT_A8B8G8R8_SINT_PACK32",
        "VK_FORMAT_A8B8G8R8_SRGB_PACK32",
        "VK_FORMAT_A2R10G10B10_UNORM_PACK32",
        "VK_FORMAT_A2R10G10B10_SNORM_PACK32",
        "VK_FORMAT_A2R10G10B10_USCALED_PACK32",
        "VK_FORMAT_A2R10G10B10_SSCALED_PACK32",
        "VK_FORMAT_A2R10G10B10_UINT_PACK32",
        "VK_FORMAT_A2R10G10B10_SINT_PACK32",
        "VK_FORMAT_A2B10G10R10_UNORM_PACK32",
        "VK_FORMAT_A2B10G10R10_SNORM_PACK32",
        "VK_FORMAT_A2B10G10R10_USCALED_PACK32",
        "VK_FORMAT_A2B10G10R10_SSCALED_PACK32",
        "VK_FORMAT_A2B10G10R10_UINT_PACK32",
        "VK_FORMAT_A2B10G10R10_SINT_PACK32",
        "VK_FORMAT_R16_UNORM",
        "VK_FORMAT_R16_SNORM",
        "VK_FORMAT_R16_USCALED",
        "VK_FORMAT_R16_SSCALED",
        "VK_FORMAT_R16_UINT",
        "VK_FORMAT_R16_SINT",
        "VK_FORMAT_R16_SFLOAT",
        "VK_FORMAT_R16G16_UNORM",
        "VK_FORMAT_R16G16_SNORM",
        "VK_FORMAT_R16G16_USCALED",
        "VK_FORMAT_R16G16_SSCALED",
        "VK_FORMAT_R16G16_UINT",
        "VK_FORMAT_R16G16_SINT",
        "VK_FORMAT_R16G16_SFLOAT",
        "VK_FORMAT_R16G16B16_UNORM",
        "VK_FORMAT_R16G16B16_SNORM",
        "VK_FORMAT_R16G16B16_USCALED",
        "VK_FORMAT_R16G16B16_SSCALED",
        "VK_FORMAT_R16G16B16_UINT",
        "VK_FORMAT_R16G16B16_SINT",
        "VK_FORMAT_R16G16B16_SFLOAT",
        "VK_FORMAT_R16G16B16A16_UNORM",
        "VK_FORMAT_R16G16B16A16_SNORM",
        "VK_FORMAT_R16G16B16A16_USCALED",
        "VK_FORMAT_R16G16B16A16_SSCALED",
        "VK_FORMAT_R16G16B16A16_UINT",
        "VK_FORMAT_R16G16B16A16_SINT",
        "VK_FORMAT_R16G16B16A16_SFLOAT",
        "VK_FORMAT_R32_UINT",
        "VK_FORMAT_R32_SINT",
        "VK_FORMAT_R32_SFLOAT",
        "VK_FORMAT_R32G32_UINT",
        "VK_FORMAT_R32G32_SINT",
        "VK_FORMAT_R32G32_SFLOAT",
        "VK_FORMAT_R32G32B32_UINT",
        "VK_FORMAT_R32G32B32_SINT",
        "VK_FORMAT_R32G32B32_SFLOAT",
        "VK_FORMAT_R32G32B32A32_UINT",
        "VK_FORMAT_R32G32B32A32_SINT",
        "VK_FORMAT_R32G32B32A32_SFLOAT",
        "VK_FORMAT_R64_UINT",
        "VK_FORMAT_R64_SINT",
        "VK_FORMAT_R64_SFLOAT",
        "VK_FORMAT_R64G64_UINT",
        "VK_FORMAT_R64G64_SINT",
        "VK_FORMAT_R64G64_SFLOAT",
        "VK_FORMAT_R64G64B64_UINT",
        "VK_FORMAT_R64G64B64_SINT",
        "VK_FORMAT_R64G64B64_SFLOAT",
        "VK_FORMAT_R64G64B64A64_UINT",
        "VK_FORMAT_R64G64B64A64_SINT",
        "VK_FORMAT_R64G64B64A64_SFLOAT",
        "VK_FORMAT_B10G11R11_UFLOAT_PACK32",
        "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32",
        "VK_FORMAT_D16_UNORM",
        "VK_FORMAT_X8_D24_UNORM_PACK32",
        "VK_FORMAT_D32_SFLOAT",
        "VK_FORMAT_S8_UINT",
        "VK_FORMAT_D16_UNORM_S8_UINT",
        "VK_FORMAT_D24_UNORM_S8_UINT",
        "VK_FORMAT_D32_SFLOAT_S8_UINT",
        "VK_FORMAT_BC1_RGB_UNORM_BLOCK",
        "VK_FORMAT_BC1_RGB_SRGB_BLOCK",
        "VK_FORMAT_BC1_RGBA_UNORM_BLOCK",
        "VK_FORMAT_BC1_RGBA_SRGB_BLOCK",
        "VK_FORMAT_BC2_UNORM_BLOCK",
        "VK_FORMAT_BC2_SRGB_BLOCK",
        "VK_FORMAT_BC3_UNORM_BLOCK",
        "VK_FORMAT_BC3_SRGB_BLOCK",
        "VK_FORMAT_BC4_UNORM_BLOCK",
        "VK_FORMAT_BC4_SNORM_BLOCK",
        "VK_FORMAT_BC5_UNORM_BLOCK",
        "VK_FORMAT_BC5_SNORM_BLOCK",
        "VK_FORMAT_BC6H_UFLOAT_BLOCK",
        "VK_FORMAT_BC6H_SFLOAT_BLOCK",
        "VK_FORMAT_BC7_UNORM_BLOCK",
        "VK_FORMAT_BC7_SRGB_BLOCK",
        "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK",
        "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK",
        "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK",
        "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK",
        "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK",
        "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK",
        "VK_FORMAT_EAC_R11_UNORM_BLOCK",
        "VK_FORMAT_EAC_R11_SNORM_BLOCK",
        "VK_FORMAT_EAC_R11G11_UNORM_BLOCK",
        "VK_FORMAT_EAC_R11G11_SNORM_BLOCK",
        "VK_FORMAT_ASTC_4x4_UNORM_BLOCK",
        "VK_FORMAT_ASTC_4x4_SRGB_BLOCK",
        "VK_FORMAT_ASTC_5x4_UNORM_BLOCK",
        "VK_FORMAT_ASTC_5x4_SRGB_BLOCK",
        "VK_FORMAT_ASTC_5x5_UNORM_BLOCK",
        "VK_FORMAT_ASTC_5x5_SRGB_BLOCK",
        "VK_FORMAT_ASTC_6x5_UNORM_BLOCK",
        "VK_FORMAT_ASTC_6x5_SRGB_BLOCK",
        "VK_FORMAT_ASTC_6x6_UNORM_BLOCK",
        "VK_FORMAT_ASTC_6x6_SRGB_BLOCK",
        "VK_FORMAT_ASTC_8x5_UNORM_BLOCK",
        "VK_FORMAT_ASTC_8x5_SRGB_BLOCK",
        "VK_FORMAT_ASTC_8x6_UNORM_BLOCK",
        "VK_FORMAT_ASTC_8x6_SRGB_BLOCK",
        "VK_FORMAT_ASTC_8x8_UNORM_BLOCK",
        "VK_FORMAT_ASTC_8x8_SRGB_BLOCK",
        "VK_FORMAT_ASTC_10x5_UNORM_BLOCK",
        "VK_FORMAT_ASTC_10x5_SRGB_BLOCK",
        "VK_FORMAT_ASTC_10x6_UNORM_BLOCK",
        "VK_FORMAT_ASTC_10x6_SRGB_BLOCK",
        "VK_FORMAT_ASTC_10x8_UNORM_BLOCK",
        "VK_FORMAT_ASTC_10x8_SRGB_BLOCK",
        "VK_FORMAT_ASTC_10x10_UNORM_BLOCK",
        "VK_FORMAT_ASTC_10x10_SRGB_BLOCK",
        "VK_FORMAT_ASTC_12x10_UNORM_BLOCK",
        "VK_FORMAT_ASTC_12x10_SRGB_BLOCK",
        "VK_FORMAT_ASTC_12x12_UNORM_BLOCK",
        "VK_FORMAT_ASTC_12x12_SRGB_BLOCK"
    };

    if (format >= VK_FORMAT_UNDEFINED &&
        format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK) {
        return formats[format];
    }

    static const char * const pvrtc_formats[] = {
        "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG",
        "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG",
        "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG",
        "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG",
        "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG",
        "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG",
        "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG",
        "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG"
    };

    if (format >= VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG &&
        format <= VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG) {
        return pvrtc_formats[format - VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG];
    }

    static const char * const ycbcr_formats[] = {
        "VK_FORMAT_G8B8G8R8_422_UNORM_KHR",
        "VK_FORMAT_B8G8R8G8_422_UNORM_KHR",
        "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR",
        "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR",
        "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM_KHR",
        "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM_KHR",
        "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM_KHR",
        "VK_FORMAT_R10X6_UNORM_PACK16_KHR",
        "VK_FORMAT_R10X6G10X6_UNORM_2PACK16_KHR",
        "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR",
        "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR",
        "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR",
        "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR",
        "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR",
        "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR",
        "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR",
        "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR",
        "VK_FORMAT_R12X4_UNORM_PACK16_KHR",
        "VK_FORMAT_R12X4G12X4_UNORM_2PACK16_KHR",
        "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR",
        "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR",
        "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR",
        "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR",
        "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR",
        "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR",
        "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR",
        "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR",
        "VK_FORMAT_G16B16G16R16_422_UNORM_KHR",
        "VK_FORMAT_B16G16R16G16_422_UNORM_KHR",
        "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM_KHR",
        "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM_KHR",
        "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM_KHR",
        "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM_KHR",
        "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR",
    };

    if (format >= VK_FORMAT_G8B8G8R8_422_UNORM_KHR &&
        format <= VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR) {
        return ycbcr_formats[format - VK_FORMAT_G8B8G8R8_422_UNORM_KHR];
    }

    return QByteArray::number(format);
}


QByteArray enumToString(VkPresentModeKHR mode)
{
    static const char * const modes[] = {
        "VK_PRESENT_MODE_IMMEDIATE_KHR",
        "VK_PRESENT_MODE_MAILBOX_KHR",
        "VK_PRESENT_MODE_FIFO_KHR",
        "VK_PRESENT_MODE_FIFO_RELAXED_KHR"
    };

    if (mode >= VK_PRESENT_MODE_IMMEDIATE_KHR && mode <= VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        return modes[mode];

    return QByteArray::number(mode);
}


QByteArray enumToString(VkPhysicalDeviceType physicalDeviceType)
{
    static const char * const types[] = {
        "VK_PHYSICAL_DEVICE_TYPE_OTHER",
        "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU",
        "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU",
        "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU",
        "VK_PHYSICAL_DEVICE_TYPE_CPU"
    };

    if (physicalDeviceType >= VK_PHYSICAL_DEVICE_TYPE_OTHER &&
        physicalDeviceType <= VK_PHYSICAL_DEVICE_TYPE_CPU)
        return types[physicalDeviceType];

    return QByteArray::number(physicalDeviceType);
}


QByteArray vendorName(uint32_t vendorID)
{
    switch (vendorID) {
    case 0x1002:
        return QByteArrayLiteral("0x1002 (Advanced Micro Devices, Inc.)");
    case 0x1010:
        return QByteArrayLiteral("0x1010 (Imagination Technologies)");
    case 0x10de:
        return QByteArrayLiteral("0x10de (NVIDIA Corporation)");
    case 0x13b5:
        return QByteArrayLiteral("0x13b5 (ARM Limited)");
    case 0x14e4:
        return QByteArrayLiteral("0x14e4 (Broadcom Corporation)");
    case 0x5143:
        return QByteArrayLiteral("0x5143 (Qualcomm Technologies, Inc.)");
    case 0x8086:
        return QByteArrayLiteral("0x8086 (Intel)");
    case 0x10001:
        return QByteArrayLiteral("0x10001 (Vivante Corporation)");
    case 0x10002:
        return QByteArrayLiteral("0x10002 (VeriSilicon Holdings Co., Ltd.)");
    default:
        return QByteArrayLiteral("0x") + QByteArray::number(vendorID, 16);
    }
}


QByteArray driverVersionString(uint32_t vendorID, uint32_t driverVersion)
{
    QString string;
    QTextStream stream(&string);

    switch (vendorID) {
    case 0x1002: // AMD
    case 0x8086: // Intel
        stream << VK_VERSION_MAJOR(driverVersion) << '.'
               << VK_VERSION_MINOR(driverVersion) << '.'
               << VK_VERSION_PATCH(driverVersion)
               << " (" << showbase << hex << driverVersion << ')';
        break;
    case 0x10de: // NVIDIA
        stream << (driverVersion >> 22)          << '.'
               << ((driverVersion >> 14) & 0xff) << '.'
               << ((driverVersion >> 6)  & 0xff) << '.'
               << (driverVersion & 0x3f)
               << " (" << showbase << hex << driverVersion << ')';
        break;
    default:
        stream << driverVersion << " (" << showbase << hex << driverVersion << ')';
        break;
    }

    stream.flush();
    return string.toUtf8();
}

} // namespace KWin
