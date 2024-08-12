/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QInputEvent>
#include <array>

namespace KWin
{

static constexpr std::array s_mediaKeys = {
    Qt::Key::Key_MediaLast,
    Qt::Key::Key_MediaNext,
    Qt::Key::Key_MediaPause,
    Qt::Key::Key_MediaPlay,
    Qt::Key::Key_MediaPrevious,
    Qt::Key::Key_MediaRecord,
    Qt::Key::Key_MediaStop,
    Qt::Key::Key_MediaTogglePlayPause,
    Qt::Key::Key_VolumeUp,
    Qt::Key::Key_VolumeDown,
    Qt::Key::Key_VolumeMute,
    Qt::Key::Key_MicVolumeUp,
    Qt::Key::Key_MicVolumeDown,
    Qt::Key::Key_MicMute,
};
static inline bool isMediaKey(int key)
{
    return std::ranges::find(s_mediaKeys, key) != s_mediaKeys.end();
}

}
