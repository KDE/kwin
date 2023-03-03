/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screenshot/screenshot.h"

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QVariantMap>

namespace KWin
{

class ScreenShotEffect;
class ScreenShotSinkPipe2;
class ScreenShotSource2;

/**
 * The ScreenshotDBusInterface2 class provides a d-bus api to take screenshots. This implements
 * the org.kde.KWin.ScreenShot2 interface.
 *
 * An application that requests a screenshot must have "org.kde.KWin.ScreenShot2" listed in its
 * X-KDE-DBUS-Restricted-Interfaces desktop file field.
 */
class ScreenShotDBusInterface2 : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_PROPERTY(int Version READ version CONSTANT)

public:
    explicit ScreenShotDBusInterface2(ScreenShotEffect *effect);
    ~ScreenShotDBusInterface2() override;

    int version() const;

public Q_SLOTS:
    QVariantMap CaptureWindow(const QString &handle, const QVariantMap &options,
                              QDBusUnixFileDescriptor pipe);
    QVariantMap CaptureActiveWindow(const QVariantMap &options,
                                    QDBusUnixFileDescriptor pipe);
    QVariantMap CaptureArea(int x, int y, int width, int height,
                            const QVariantMap &options,
                            QDBusUnixFileDescriptor pipe);
    QVariantMap CaptureScreen(const QString &name, const QVariantMap &options,
                              QDBusUnixFileDescriptor pipe);
    QVariantMap CaptureActiveScreen(const QVariantMap &options,
                                    QDBusUnixFileDescriptor pipe);
    QVariantMap CaptureInteractive(uint kind, const QVariantMap &options,
                                   QDBusUnixFileDescriptor pipe);
    QVariantMap CaptureWorkspace(const QVariantMap &options,
                                 QDBusUnixFileDescriptor pipe);

private:
    void takeScreenShot(EffectScreen *screen, ScreenShotFlags flags, ScreenShotSinkPipe2 *sink);
    void takeScreenShot(const QRect &area, ScreenShotFlags flags, ScreenShotSinkPipe2 *sink);
    void takeScreenShot(EffectWindow *window, ScreenShotFlags flags, ScreenShotSinkPipe2 *sink);

    void bind(ScreenShotSinkPipe2 *sink, ScreenShotSource2 *source);
    bool checkPermissions() const;

    ScreenShotEffect *m_effect;
};

} // namespace KWin
