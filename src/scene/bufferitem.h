/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "scene/item.h"

#include <QImage>

namespace KWin
{

class Texture;

class KWIN_EXPORT BufferItem : public Item
{
    Q_OBJECT

public:
    explicit BufferItem(Item *parent = nullptr);
    ~BufferItem() override;

    Texture *texture() const;
    void setBuffer(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    bool hasAlphaChannel() const;

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;
    void releaseResources() override;

    std::unique_ptr<Texture> m_texture;
    GraphicsBufferRef m_buffer;
    std::shared_ptr<SyncReleasePoint> m_releasePoint;
    bool m_bufferDirty = false;
};

} // namespace KWin
