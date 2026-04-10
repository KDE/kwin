/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "scene/surfaceitem.h"

namespace KWin
{

class EglSwapchain;
class GLTexture;

class KWIN_EXPORT EffectItem : public SurfaceItem
{
    Q_OBJECT

public:
    explicit EffectItem(Item *parentItem);

    bool needsBackgroundTexture() const;
    void setNeedsBackgroundTexture(bool background);

    // HACK Maybe we need a more abstract alternative (like Texture)
    void setBackgroundCache(std::shared_ptr<EglSwapchain> &&swapchain);
    EglSwapchain *backgroundCache() const;

    virtual Texture *prepareRendering(GLTexture *background) const;

private:
    std::shared_ptr<EglSwapchain> m_swapchain;
    bool m_needsBackgroundTexture = false;
};

}
