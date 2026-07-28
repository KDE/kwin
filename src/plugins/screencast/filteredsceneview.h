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
    FilteredSceneView(Scene *scene, LogicalOutput *output, OutputLayer *layer, RenderDevice *renderDevice, std::optional<pid_t> pidToHide);

    void prePaint(OutputFrame *frame = nullptr) override;

    void setRefreshRate(uint refreshRate);
};

} // namespace KWin
