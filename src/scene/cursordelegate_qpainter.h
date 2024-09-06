/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

#include <QImage>

namespace KWin
{

class Output;

class CursorDelegateQPainter final : public SceneDelegate
{
public:
    CursorDelegateQPainter(Scene *scene, Output *output);

    void paint(const RenderTarget &renderTarget, const QRegion &region) override;

private:
    Output *const m_output;
    QImage m_buffer;
};

} // namespace KWin
