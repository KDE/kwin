/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class NinePatch;
class Shadow;
class Window;

/**
 * The ShadowItem class represents a nine-tile patch server-side drop-shadow.
 */
class KWIN_EXPORT ShadowItem : public Item
{
    Q_OBJECT

public:
    explicit ShadowItem(Shadow *shadow, Window *window, Item *parent = nullptr);
    ~ShadowItem() override;

    Shadow *shadow() const;
    NinePatch *ninePatch() const;

protected:
    WindowQuadList buildQuads() const override;
    void preprocess() override;
    void releaseResources() override;

private Q_SLOTS:
    void handleTextureChanged();
    void updateGeometry();

private:
    Window *m_window;
    Shadow *m_shadow = nullptr;
    std::shared_ptr<NinePatch> m_ninePatch;
    bool m_textureDirty = true;
};

} // namespace KWin
