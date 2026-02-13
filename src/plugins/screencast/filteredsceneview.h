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
    uint refreshRate() const override;
    void prePaint() override;

    void setRefreshRate(uint refreshRate);

private:
    std::chrono::nanoseconds m_presentationTimestamp = std::chrono::nanoseconds::zero();
    uint m_refreshRate = 60000;
};

} // namespace KWin
