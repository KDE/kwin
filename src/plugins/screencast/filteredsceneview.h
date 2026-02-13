/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

namespace KWin
{

class FilteredSceneView : public SceneView
{
    Q_OBJECT

public:
    FilteredSceneView(Scene *scene, LogicalOutput *output, OutputLayer *layer, std::optional<pid_t> pidToHide);

    std::chrono::nanoseconds nextPresentationTimestamp() const override;
    void prePaint() override;

private:
    std::chrono::nanoseconds m_presentationTimestamp = std::chrono::nanoseconds::zero();
};

} // namespace KWin
