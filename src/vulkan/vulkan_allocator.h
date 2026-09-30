#pragma once
#include "core/graphicsbufferallocator.h"

namespace KWin
{

class VulkanDevice;

class VulkanGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    explicit VulkanGraphicsBufferAllocator(VulkanDevice *device);

    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

private:
    VulkanDevice *const m_device;
};

}
