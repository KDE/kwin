#pragma once

#include "platformsupport/scenes/vulkan/vulkan_backend.h"
#include "scene/workspacescene.h"

namespace KWin
{

class KWIN_EXPORT WorkspaceSceneVulkan : public WorkspaceScene
{
    Q_OBJECT
public:
    explicit WorkspaceSceneVulkan(VulkanBackend *backend);
    ~WorkspaceSceneVulkan() override;

    std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedClientImpl *) override;
    std::unique_ptr<ShadowTextureProvider> createShadowTextureProvider(Shadow *shadow) override;

    bool animationsSupported() const override;

private:
    VulkanBackend *const m_backend;
};

}
