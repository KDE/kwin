/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/atlas.h"

namespace KWin
{

class GLTexture;
class EglContext;

class AtlasOpenGL : public Atlas
{
public:
    static std::unique_ptr<AtlasOpenGL> create(const std::shared_ptr<EglContext> &context, const QList<QImage> &images);

    explicit AtlasOpenGL(const std::shared_ptr<EglContext> &context);
    ~AtlasOpenGL() override;

    GLTexture *texture() const;

    Sprite sprite(uint spriteId) const override;
    bool update(uint spriteId, const QImage &image, const Rect &damage) override;
    bool reset(const QList<QImage> &images) override;

private:
    void upload(uint spriteId, const QImage &data, const Rect &damage);

    std::shared_ptr<EglContext> m_context;
    std::unique_ptr<GLTexture> m_texture;
    QList<Sprite> m_sprites;
};

} // namespace KWin
