/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"
#include "effectwindow.h"
#include "scene/transformitem.h"
#include "utils/filedescriptor.h"

#include <functional>

namespace KWin
{

class EglSwapchain;
class GLShader;
class Texture;
class EffectWindow;
class SyncReleasePoint;

class SnapshotItem : public Item
{
    Q_OBJECT

public:
    explicit SnapshotItem(EffectWindow *window, Item *parent, Item *toSnapshot,
                          const std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> &applyGeometryCallback = {});

    /**
     * Takes a single snapshot and prevents the item from being updated later on
     */
    void freeze();
    void setItem(Item *toSnapshot);

    void preprocess(ItemRenderer *renderer) override;
    RegionF shape() const override;
    RegionF opaque() const override;

    void scheduleUpdate();

    Texture *texture(RenderDevice *device) const;

    // TODO make this independent of the rendering API and device
    void setShader(GLShader *shader);
    GLShader *shader() const;

protected:
    WindowQuadList buildQuads(ItemRenderer *renderer) override;
    void releaseResources(RenderDevice *device) override;
    std::optional<bool> maybeRender(ItemRenderer *renderer);

    EffectWindow *const m_window;
    Item *m_snapshotItem = nullptr;
    std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> m_applyGeometryCallback;

    struct Resources
    {
        std::unique_ptr<Texture> texture;
        std::shared_ptr<EglSwapchain> m_swapchain;
        GraphicsBufferRef m_buffer;
        FileDescriptor m_bufferSync;
        std::shared_ptr<SyncReleasePoint> m_releasePoint;
        std::shared_ptr<ColorDescription> m_colorDescription = ColorDescription::sRGB;
        bool m_dirty = true;
    };
    std::unordered_map<RenderDevice *, Resources> m_resources;

    bool m_frozen = false;
    GLShader *m_shader = nullptr;
};

class KWIN_EXPORT OffscreenEffectItem : public TransformItem
{
    Q_OBJECT

public:
    explicit OffscreenEffectItem(EffectWindow *window, const std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> &applyGeometryCallback);
    ~OffscreenEffectItem() override;

    void setShader(GLShader *shader);

private:
    void updateClip();
    void handleGeometryChanged();

    std::unique_ptr<TransformItem> m_clip;
    std::unique_ptr<SnapshotItem> m_snapshot;
    std::unordered_map<RenderDevice *, std::unique_ptr<Texture>> m_textures;
    GLShader *m_shader = nullptr;
};

class KWIN_EXPORT CrossfadeItem : public SnapshotItem
{
    Q_OBJECT

public:
    explicit CrossfadeItem(EffectWindow *window);
    ~CrossfadeItem() override;

    void updateGeometry();

private:
    const RectF m_frameGeometryAtCapture;
    const RectF m_expandedGeometryAtCapture;
};

}
