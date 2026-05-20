/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2025 Stanislav Aleksandrov <lightofmysoul@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "filteredsceneview.h"
#include "window.h"
#include "workspace.h"

namespace KWin
{

FilteredSceneView::FilteredSceneView(Scene *scene, LogicalOutput *output, OutputLayer *layer, std::optional<pid_t> pidToHide)
    : SceneView(scene, output, nullptr, layer)
{
    addWindowFilter([pidToHide](Window *window) {
        if (pidToHide.has_value() && *pidToHide == window->pid()) {
            return true;
        }

        return window->excludeFromCapture();
    });
    addWindowFilter([](Window *window) {
        return window->isLockScreen() || window->isLockScreenOverlay();
    });

    auto setupRepaintOnExcludedFromCapture = [this](Window *window) {
        // When a window becomes hidden or visible from screencast
        // the region where the window is located will not be damaged, so
        // by calling addDeviceRepaint() we will redraw everything
        connect(window, &Window::excludeFromCaptureChanged, this, [this] {
            addDeviceRepaint(Region::infinite());
        });
    };
    connect(workspace(), &Workspace::windowAdded, this, [setupRepaintOnExcludedFromCapture](Window *window) {
        setupRepaintOnExcludedFromCapture(window);
    });
    const auto windows = workspace()->windows();
    for (auto window : windows) {
        setupRepaintOnExcludedFromCapture(window);
    }
}

void FilteredSceneView::prePaint(OutputFrame *frame)
{
    m_nextPresentationTimestamp = std::chrono::steady_clock::now().time_since_epoch();
    SceneView::prePaint(frame);
}

void FilteredSceneView::setRefreshRate(uint refreshRate)
{
    m_refreshRate = refreshRate;
}

} // namespace KWin

#include "moc_filteredsceneview.cpp"
