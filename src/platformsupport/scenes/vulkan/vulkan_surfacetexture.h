#pragma once
#include "scene/surfaceitem.h"

namespace KWin
{

class SurfaceTextureVulkan : public SurfaceTexture
{
public:
    explicit SurfaceTextureVulkan();
    ~SurfaceTextureVulkan() override;

    bool create() override;
    void update(const QRegion &region) override;
    bool isValid() const override;
};

}
