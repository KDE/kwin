/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/atlas.h"

namespace KWin
{

class AtlasQPainter : public Atlas
{
public:
    static std::unique_ptr<AtlasQPainter> create(const QList<QImage> &images);

    QImage image(uint spriteId) const;

    Sprite sprite(uint spriteId) const override;
    bool update(uint spriteId, const QImage &image, const Rect &damage) override;
    bool reset(const QList<QImage> &images) override;

private:
    QList<QImage> m_images;
    QList<Sprite> m_sprites;
};

} // namespace KWin
