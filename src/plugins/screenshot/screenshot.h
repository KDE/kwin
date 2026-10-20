/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "anonymousfile.h"
#include "plugin.h"
#include "utils/memorymap.h"

#include <QImage>

namespace KWin
{

/**
 * This enum type is used to specify how a screenshot needs to be taken.
 */
enum ScreenShotFlag {
    ScreenShotIncludeDecoration = 0x1, ///< Include window titlebar and borders
    ScreenShotIncludeCursor = 0x2, ///< Include the cursor
    ScreenShotNativeResolution = 0x4, ///< Take the screenshot at the native resolution
    ScreenShotIncludeShadow = 0x8, ///< Include the window shadow
};
Q_DECLARE_FLAGS(ScreenShotFlags, ScreenShotFlag)

class LogicalOutput;
class Rect;
class ScreenShotDBusInterface2;
class Window;

struct ScreenShotBuffer
{
    FileDescriptor file;
    MemoryMap map;

    QSize size;
    int bytesPerLine;
    QImage::Format format;
    qreal devicePixelRatio;

    static std::optional<ScreenShotBuffer> allocate(const QSize &size, qreal devicePixelRatio, QImage::Format format);
};

/**
 * The ScreenShotManager provides a convenient way to capture the contents of a given window,
 * screen or an area in the global coordinates.
 */
class ScreenShotManager : public Plugin
{
    Q_OBJECT

public:
    ScreenShotManager();
    ~ScreenShotManager() override;

    std::optional<ScreenShotBuffer> takeScreenShot(LogicalOutput *screen, ScreenShotFlags flags, std::optional<pid_t> pidToHide);
    std::optional<ScreenShotBuffer> takeScreenShot(const Rect &area, ScreenShotFlags flags, std::optional<pid_t> pidToHide);
    std::optional<ScreenShotBuffer> takeScreenShot(Window *window, ScreenShotFlags flags = {});

private:
    std::unique_ptr<ScreenShotDBusInterface2> m_dbusInterface2;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScreenShotFlags)
