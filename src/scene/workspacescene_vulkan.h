#pragma once

#include "decorationitem.h"
#include "shadowitem.h"
#include "workspacescene.h"

namespace KWin
{
class VulkanTexture;

class KWIN_EXPORT WorkspaceSceneVulkan : public WorkspaceScene
{
    Q_OBJECT
public:
    explicit WorkspaceSceneVulkan();
    ~WorkspaceSceneVulkan() override;

    std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedClientImpl *) override;
    std::unique_ptr<ShadowTextureProvider> createShadowTextureProvider(Shadow *shadow) override;

    bool animationsSupported() const override;
};

class DecorationRendererVulkan : public DecorationRenderer
{
    Q_OBJECT
public:
    explicit DecorationRendererVulkan(Decoration::DecoratedClientImpl *client);
    ~DecorationRendererVulkan() override;

    void render(const QRegion &region) override;

private:
    std::unique_ptr<VulkanTexture> m_texture;
};

class ShadowTextureProviderVulkan : public ShadowTextureProvider
{
public:
    explicit ShadowTextureProviderVulkan(Shadow *shadow);
    ~ShadowTextureProviderVulkan() override;

    void update() override;

private:
    std::unique_ptr<VulkanTexture> m_texture;
};
}
