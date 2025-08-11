/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "placementtracker.h"
#include "core/output.h"
#include "window.h"
#include "workspace.h"

namespace KWin
{

PlacementTracker::PlacementTracker(Workspace *workspace)
    : m_workspace(workspace)
{
}

PlacementTracker::WindowData PlacementTracker::dataForWindow(Window *window) const
{
    return WindowData{
        .outputUuid = window->moveResizeOutput()->uuid(),
        .geometry = window->moveResizeGeometry(),
        .maximize = window->requestedMaximizeMode(),
        .quickTile = window->requestedQuickTileMode(),
        .geometryRestore = window->geometryRestore(),
        .fullscreen = window->isRequestedFullScreen(),
        .fullscreenGeometryRestore = window->fullscreenGeometryRestore(),
        .interactiveMoveResizeCount = window->interactiveMoveResizeCount(),
    };
}

void PlacementTracker::add(Window *window)
{
    if (window->isUnmanaged() || window->isAppletPopup() || window->isSpecialWindow()) {
        return;
    }
    connect(window, &Window::frameGeometryChanged, this, [this, window]() {
        saveGeometry(window);
    });
    connect(window, &Window::maximizedChanged, this, [this, window]() {
        saveMaximize(window);
    });
    connect(window, &Window::quickTileModeChanged, this, [this, window]() {
        saveQuickTile(window);
    });
    connect(window, &Window::fullScreenChanged, this, [this, window]() {
        saveFullscreen(window);
    });
    connect(window, &Window::interactiveMoveResizeFinished, this, [this, window]() {
        saveInteractionCounter(window);
    });
    connect(window, &Window::maximizeGeometryRestoreChanged, this, [this, window]() {
        saveMaximizeGeometryRestore(window);
    });
    connect(window, &Window::fullscreenGeometryRestoreChanged, this, [this, window]() {
        saveFullscreenGeometryRestore(window);
    });
    WindowData data = dataForWindow(window);
    m_data[m_currentKey][window] = data;
    m_savedWindows.push_back(window);
}

void PlacementTracker::remove(Window *window)
{
    if (m_savedWindows.contains(window)) {
        disconnect(window, nullptr, this, nullptr);
        for (auto &dataMap : m_data) {
            dataMap.remove(window);
        }
        m_savedWindows.removeOne(window);
    }
}

void PlacementTracker::restore(const QString &key)
{
    if (key == m_currentKey) {
        return;
    }
    auto &dataMap = m_data[key];
    auto &oldDataMap = m_data[m_currentKey];
    const auto outputs = m_workspace->outputs();

    inhibit();
    for (const auto window : std::as_const(m_savedWindows)) {
        const auto it = dataMap.find(window);
        if (it != dataMap.end()) {
            const WindowData &newData = it.value();

            const auto checkQuickTileMode = [window](QuickTileMode oldMode) {
                if (window->requestedQuickTileMode() == oldMode) {
                    return true;
                }
                // custom-tiled windows lose their quick tile mode when the output they're on is unplugged
                // TODO find some nicer way of handling this?
                return window->requestedQuickTileMode() == QuickTileFlag::None && oldMode == QuickTileFlag::Custom;
            };

            // don't touch windows where the user intentionally changed their state
            bool restore = window->interactiveMoveResizeCount() == newData.interactiveMoveResizeCount
                && window->requestedMaximizeMode() == newData.maximize
                && checkQuickTileMode(newData.quickTile)
                && window->isFullScreen() == newData.fullscreen;
            if (!restore) {
                // the logic above can have false negatives if PlacementTracker changed the window state
                // to prevent that, also restore if the window still has the same state from that
                if (const auto it = m_lastRestoreData.find(window); it != m_lastRestoreData.end()) {
                    restore = window->interactiveMoveResizeCount() == it->interactiveMoveResizeCount
                        && window->requestedMaximizeMode() == it->maximize
                        && checkQuickTileMode(it->quickTile)
                        && window->isFullScreen() == it->fullscreen
                        && window->moveResizeOutput()->uuid() == it->outputUuid;
                }
            }
            if (!restore) {
                // restore anyways if the output the window was on got removed
                if (const auto oldData = oldDataMap.find(window); oldData != oldDataMap.end()) {
                    restore = std::none_of(outputs.begin(), outputs.end(), [&oldData](const auto output) {
                        return output->uuid() == oldData->outputUuid;
                    });
                }
            }
            if (restore) {
                window->setQuickTileMode(newData.quickTile, newData.geometry.center());
                window->setMaximize(newData.maximize & MaximizeMode::MaximizeVertical, newData.maximize & MaximizeMode::MaximizeHorizontal);
                window->setFullScreen(newData.fullscreen);
                window->moveResize(newData.geometry);
                window->setGeometryRestore(newData.geometryRestore);
                window->setFullscreenGeometryRestore(newData.fullscreenGeometryRestore);
                m_lastRestoreData[window] = dataForWindow(window);
            }
        }
        // ensure data in current map is always up to date
        dataMap[window] = dataForWindow(window);
    }
    uninhibit();
    m_currentKey = key;
}

void PlacementTracker::init(const QString &key)
{
    m_currentKey = key;
}

void PlacementTracker::saveGeometry(Window *window)
{
    if (m_inhibitCount == 0) {
        auto &data = m_data[m_currentKey][window];
        data.geometry = window->moveResizeGeometry();
        data.outputUuid = window->moveResizeOutput()->uuid();
    }
}

void PlacementTracker::saveInteractionCounter(Window *window)
{
    if (m_inhibitCount == 0) {
        m_data[m_currentKey][window].interactiveMoveResizeCount = window->interactiveMoveResizeCount();
    }
}

void PlacementTracker::saveMaximize(Window *window)
{
    if (m_inhibitCount == 0) {
        auto &data = m_data[m_currentKey][window];
        data.maximize = window->maximizeMode();
    }
}

void PlacementTracker::saveQuickTile(Window *window)
{
    if (m_inhibitCount == 0) {
        auto &data = m_data[m_currentKey][window];
        data.quickTile = window->quickTileMode();
    }
}

void PlacementTracker::saveFullscreen(Window *window)
{
    if (m_inhibitCount == 0) {
        auto &data = m_data[m_currentKey][window];
        data.fullscreen = window->isFullScreen();
    }
}

void PlacementTracker::saveMaximizeGeometryRestore(Window *window)
{
    if (m_inhibitCount == 0) {
        auto &data = m_data[m_currentKey][window];
        data.geometryRestore = window->geometryRestore();
    }
}

void PlacementTracker::saveFullscreenGeometryRestore(Window *window)
{
    if (m_inhibitCount == 0) {
        auto &data = m_data[m_currentKey][window];
        data.fullscreenGeometryRestore = window->fullscreenGeometryRestore();
    }
}

void PlacementTracker::inhibit()
{
    m_inhibitCount++;
}

void PlacementTracker::uninhibit()
{
    Q_ASSERT(m_inhibitCount > 0);
    m_inhibitCount--;
}
}

#include "moc_placementtracker.cpp"
