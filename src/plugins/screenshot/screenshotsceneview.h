/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

namespace KWin
{

class ScreenShotSceneView : public SceneView
{
    Q_OBJECT

public:
    ScreenShotSceneView(Scene *scene, LogicalOutput *logicalOutput, OutputLayer *layer);

    std::chrono::nanoseconds nextPresentationTimestamp() const override;
    uint refreshRate() const override;

private:
    std::chrono::nanoseconds m_presentationTimestamp;
};

} // namespace KWin
