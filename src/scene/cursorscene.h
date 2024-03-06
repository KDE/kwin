/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

namespace KWin
{

class CursorItem;
class Output;
class RootItem;

class KWIN_EXPORT CursorScene : public Scene
{
    Q_OBJECT

public:
    explicit CursorScene(std::unique_ptr<ItemRenderer> &&renderer);
    ~CursorScene() override;

    QRegion prePaint(SceneDelegate *delegate) override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;

private:
    std::unique_ptr<RootItem> m_rootItem;
    std::unique_ptr<CursorItem> m_cursorItem;
    Output *m_paintedOutput = nullptr;
};

} // namespace KWin
