/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

namespace KWin
{

class CursorItem;

class KWIN_EXPORT CursorScene : public Scene
{
    Q_OBJECT

public:
    explicit CursorScene(std::unique_ptr<ItemRenderer> &&renderer);
    ~CursorScene() override;

    void initialize();

    void prePaint(SceneDelegate *delegate) override;
    void postPaint() override;
    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    std::unique_ptr<CursorItem> m_rootItem;
};

} // namespace KWin
