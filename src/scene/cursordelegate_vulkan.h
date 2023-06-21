/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "scene/scene.h"

namespace KWin
{

class Output;

class CursorDelegateVulkan : public SceneDelegate
{
public:
    CursorDelegateVulkan(Scene *scene, Output *output);
    ~CursorDelegateVulkan() override;

    void paint(const RenderTarget &renderTarget, const QRegion &region) override;

private:
    Output *const m_output;
};

}
