/*
    SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

// This script sets all windows on non-primary screens to "on all desktops",
// effectively making it so only the primary screen has virtual desktops.

let primaryScreen = workspace.screenOrder[0];

function processWindow(window) {
    if (window.output === primaryScreen) {
        window.onAllDesktops = false;
    } else {
        window.onAllDesktops = true;
    }

    window.outputChanged.connect((_) => processWindow(window));
}

function processAllWindows() {
    workspace.windowList().forEach((window) => {
        if (!window.normalWindow) return;
        processWindow(window);
    });
}

function onScreensChanged() {
    // Update the primary screen.
    primaryScreen = workspace.screenOrder[0];
    // Reprocess all windows.
    processAllWindows();
}

function main() {
    // Process existing windows on startup.
    processAllWindows();

    // When the screens change…
    workspace.screensChanged.connect(onScreensChanged);
    workspace.screenOrderChanged.connect(onScreensChanged);

    // When a new window is added…
    workspace.windowAdded.connect((window) => {
        if (!window.normalWindow) return;
        processWindow(window);
    });
}

main();
