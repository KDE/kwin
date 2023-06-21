#include "workspacescene_vulkan.h"
#include "decorationitem.h"
#include "itemrenderer_vulkan.h"
#include "shadowitem.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

WorkspaceSceneVulkan::WorkspaceSceneVulkan()
    : WorkspaceScene(std::make_unique<ItemRendererVulkan>())
{
}

WorkspaceSceneVulkan::~WorkspaceSceneVulkan()
{
}

std::unique_ptr<DecorationRenderer> WorkspaceSceneVulkan::createDecorationRenderer(Decoration::DecoratedClientImpl *client)
{
    return std::make_unique<DecorationRendererVulkan>(client);
}

std::unique_ptr<ShadowTextureProvider> WorkspaceSceneVulkan::createShadowTextureProvider(Shadow *shadow)
{
    return std::make_unique<ShadowTextureProviderVulkan>(shadow);
}

bool WorkspaceSceneVulkan::animationsSupported() const
{
    return false;
}

DecorationRendererVulkan::DecorationRendererVulkan(Decoration::DecoratedClientImpl *client)
    : DecorationRenderer(client)
{
}

DecorationRendererVulkan::~DecorationRendererVulkan()
{
}

void DecorationRendererVulkan::render(const QRegion &region)
{
    // TODO X
}

ShadowTextureProviderVulkan::ShadowTextureProviderVulkan(Shadow *shadow)
    : ShadowTextureProvider(shadow)
{
}

ShadowTextureProviderVulkan::~ShadowTextureProviderVulkan()
{
}

void ShadowTextureProviderVulkan::update()
{
    // TODO X
}
}
