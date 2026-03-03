/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/screenshot/screenshotsceneview.h"

namespace KWin
{

ScreenShotSceneView::ScreenShotSceneView(Scene *scene, LogicalOutput *logicalOutput, OutputLayer *layer)
    : SceneView(scene, logicalOutput, nullptr, layer)
    , m_presentationTimestamp(std::chrono::steady_clock::now().time_since_epoch())
{
}

std::chrono::nanoseconds ScreenShotSceneView::nextPresentationTimestamp() const
{
    return m_presentationTimestamp;
}

uint ScreenShotSceneView::refreshRate() const
{
    return 60000;
}

} // namespace KWin

#include "moc_screenshotsceneview.cpp"
