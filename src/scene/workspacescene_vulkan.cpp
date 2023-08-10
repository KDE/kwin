#include "workspacescene_vulkan.h"
#include "decorationitem.h"
#include "itemrenderer_vulkan.h"
#include "shadowitem.h"

namespace KWin
{

WorkspaceSceneVulkan::WorkspaceSceneVulkan(VulkanBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererVulkan>(backend))
    , m_backend(backend)
{
}

WorkspaceSceneVulkan::~WorkspaceSceneVulkan()
{
}

std::unique_ptr<DecorationRenderer> WorkspaceSceneVulkan::createDecorationRenderer(Decoration::DecoratedClientImpl *)
{
    return nullptr;
}

std::unique_ptr<ShadowTextureProvider> WorkspaceSceneVulkan::createShadowTextureProvider(Shadow *shadow)
{
    return nullptr;
}

bool WorkspaceSceneVulkan::animationsSupported() const
{
    return true;
}
}
