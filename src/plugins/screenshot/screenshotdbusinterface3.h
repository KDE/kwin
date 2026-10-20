/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screenshot.h"

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QVariantMap>

namespace KWin
{

class Rect;
class ScreenShotManager;

/**
 * The ScreenshotDBusInterface3 class provides a d-bus api to take screenshots. This implements
 * the org.kde.KWin.ScreenShot2 interface.
 */
class ScreenShotDBusInterface3 : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_PROPERTY(int Version READ version CONSTANT)

public:
    explicit ScreenShotDBusInterface3(ScreenShotManager *effect);
    ~ScreenShotDBusInterface3() override;

    int version() const;

public Q_SLOTS:
    QDBusUnixFileDescriptor CaptureWindow(const QString &handle, const QVariantMap &options, QVariantMap &results);
    QDBusUnixFileDescriptor CaptureActiveWindow(const QVariantMap &options, QVariantMap &results);
    QDBusUnixFileDescriptor CaptureArea(int x, int y, int width, int height, const QVariantMap &options, QVariantMap &results);
    QDBusUnixFileDescriptor CaptureScreen(const QString &name, const QVariantMap &options, QVariantMap &results);
    QDBusUnixFileDescriptor CaptureActiveScreen(const QVariantMap &options, QVariantMap &results);
    QDBusUnixFileDescriptor CaptureInteractive(uint kind, const QVariantMap &options, QVariantMap &results);
    QDBusUnixFileDescriptor CaptureWorkspace(const QVariantMap &options, QVariantMap &results);

private:
    std::optional<pid_t> determineCallerPid() const;
    bool checkPermissions(std::optional<pid_t> pid) const;

    ScreenShotManager *m_effect;
};

} // namespace KWin
