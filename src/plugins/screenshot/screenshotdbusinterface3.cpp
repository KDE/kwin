/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenshotdbusinterface3.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "screenshot3adaptor.h"
#include "screenshotlogging.h"
#include "utils/filedescriptor.h"
#include "window.h"
#include "workspace.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace KWin
{

static ScreenShotFlags screenShotFlagsFromOptions(const QVariantMap &options)
{
    ScreenShotFlags flags = ScreenShotFlags();

    const QVariant includeDecoration = options.value(QStringLiteral("include-decoration"));
    if (includeDecoration.toBool()) {
        flags |= ScreenShotIncludeDecoration;
    }

    const QVariant includeShadow = options.value(QStringLiteral("include-shadow"), true);
    if (includeShadow.toBool()) {
        flags |= ScreenShotIncludeShadow;
    }

    const QVariant includeCursor = options.value(QStringLiteral("include-cursor"));
    if (includeCursor.toBool()) {
        flags |= ScreenShotIncludeCursor;
    }

    const QVariant nativeResolution = options.value(QStringLiteral("native-resolution"));
    if (nativeResolution.toBool()) {
        flags |= ScreenShotNativeResolution;
    }

    return flags;
}

static std::optional<pid_t> pidToHide(std::optional<pid_t> callerPid, const QVariantMap &options)
{
    if (!callerPid.has_value()) {
        return std::nullopt;
    }
    const bool hideCallerWindows = options.value(QStringLiteral("hide-caller-windows"), true).toBool();
    if (!hideCallerWindows) {
        return std::nullopt;
    }
    return callerPid;
}

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.ScreenShot3");
static const QString s_dbusInterface = QStringLiteral("org.kde.KWin.ScreenShot3");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot3");

static const QString s_errorCancelled = QStringLiteral("org.kde.KWin.ScreenShot3.Error.Cancelled");
static const QString s_errorCancelledMessage = QStringLiteral("Screenshot got cancelled");
static const QString s_errorInvalidWindow = QStringLiteral("org.kde.KWin.ScreenShot3.Error.InvalidWindow");
static const QString s_errorInvalidWindowMessage = QStringLiteral("Invalid window requested");
static const QString s_errorNoActiveWindow = QStringLiteral("org.kde.KWin.ScreenShot3.Error.NoActiveWindow");
static const QString s_errorNoActiveWindowMessage = QStringLiteral("No active window");
static const QString s_errorInvalidArea = QStringLiteral("org.kde.KWin.ScreenShot3.Error.InvalidArea");
static const QString s_errorInvalidAreaMessage = QStringLiteral("Invalid area requested");
static const QString s_errorInvalidScreen = QStringLiteral("org.kde.KWin.ScreenShot3.Error.InvalidScreen");
static const QString s_errorInvalidScreenMessage = QStringLiteral("Invalid screen requested");
static const QString s_errorFileDescriptor = QStringLiteral("org.kde.KWin.ScreenShot3.Error.FileDescriptor");
static const QString s_errorFileDescriptorMessage = QStringLiteral("No valid file descriptor");

ScreenShotDBusInterface3::ScreenShotDBusInterface3(ScreenShotManager *manager)
    : QObject(manager)
    , m_effect(manager)
{
    new ScreenShot3Adaptor(this);

    QDBusConnection::sessionBus().registerObject(s_dbusObjectPath, this);
    QDBusConnection::sessionBus().registerService(s_dbusServiceName);
}

ScreenShotDBusInterface3::~ScreenShotDBusInterface3()
{
    QDBusConnection::sessionBus().unregisterService(s_dbusServiceName);
    QDBusConnection::sessionBus().unregisterObject(s_dbusObjectPath);
}

int ScreenShotDBusInterface3::version() const
{
    return 1;
}

std::optional<pid_t> ScreenShotDBusInterface3::determineCallerPid() const
{
    if (!calledFromDBus()) {
        return std::nullopt;
    }
    const QDBusReply<uint> reply = connection().interface()->servicePid(message().service());
    if (reply.isValid()) {
        return reply.value();
    } else {
        return std::nullopt;
    }
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureActiveWindow(const QVariantMap &options, QVariantMap &results)
{
    Window *window = workspace()->activeWindow();
    if (!window) {
        sendErrorReply(s_errorNoActiveWindow, s_errorNoActiveWindowMessage);
        return QDBusUnixFileDescriptor();
    }

    setDelayedReply(true);
    takeScreenShot(window, screenShotFlagsFromOptions(options), message());

    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureWindow(const QString &handle, const QVariantMap &options, QVariantMap &results)
{
    Window *window = workspace()->findWindow(QUuid(handle));
    if (!window) {
        sendErrorReply(s_errorInvalidWindow, s_errorInvalidWindowMessage);
        return QDBusUnixFileDescriptor();
    }

    setDelayedReply(true);
    takeScreenShot(window, screenShotFlagsFromOptions(options), message());

    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureArea(int x, int y, int width, int height, const QVariantMap &options, QVariantMap &results)
{
    const auto pid = determineCallerPid();

    const Rect area(x, y, width, height);
    if (area.isEmpty()) {
        sendErrorReply(s_errorInvalidArea, s_errorInvalidAreaMessage);
        return QDBusUnixFileDescriptor();
    }

    setDelayedReply(true);
    takeScreenShot(area, screenShotFlagsFromOptions(options), message(), pid);

    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureScreen(const QString &name, const QVariantMap &options, QVariantMap &results)
{
    const auto pid = determineCallerPid();

    LogicalOutput *screen = workspace()->findOutput(name);
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMessage);
        return QDBusUnixFileDescriptor();
    }

    setDelayedReply(true);
    takeScreenShot(screen, screenShotFlagsFromOptions(options), message(), pid);

    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureActiveScreen(const QVariantMap &options, QVariantMap &results)
{
    const auto pid = determineCallerPid();

    LogicalOutput *screen = workspace()->activeOutput();
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMessage);
        return QDBusUnixFileDescriptor();
    }

    setDelayedReply(true);
    takeScreenShot(screen, screenShotFlagsFromOptions(options), message(), pid);

    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureInteractive(uint kind, const QVariantMap &options, QVariantMap &results)
{
    const auto pid = determineCallerPid();
    const QDBusMessage replyMessage = message();

    if (kind == 0) {
        kwinApp()->startInteractiveWindowSelection([=, this](Window *window) {
            effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);

            if (!window) {
                QDBusConnection bus = QDBusConnection::sessionBus();
                bus.send(replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
            } else {
                takeScreenShot(window, screenShotFlagsFromOptions(options), replyMessage);
            }
        });
        effects->showOnScreenMessage(i18n("Select window to screen shot with left click or enter.\n"
                                          "Escape or right click to cancel."),
                                     QStringLiteral("spectacle"));
    } else {
        kwinApp()->startInteractivePositionSelection([=, this](const QPointF &point) {
            effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);

            if (point == QPoint(-1, -1)) {
                QDBusConnection bus = QDBusConnection::sessionBus();
                bus.send(replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
            } else {
                LogicalOutput *screen = effects->screenAt(point.toPoint());
                takeScreenShot(screen, screenShotFlagsFromOptions(options), replyMessage, pidToHide(pid, options));
            }
        });
        effects->showOnScreenMessage(i18n("Create screen shot with left click or enter.\n"
                                          "Escape or right click to cancel."),
                                     QStringLiteral("spectacle"));
    }

    setDelayedReply(true);
    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor ScreenShotDBusInterface3::CaptureWorkspace(const QVariantMap &options, QVariantMap &results)
{
    return QDBusUnixFileDescriptor();
}

void ScreenShotDBusInterface3::takeScreenShot(LogicalOutput *screen, ScreenShotFlags flags, const QDBusMessage &message, std::optional<pid_t> pid)
{
    if (auto result = m_effect->takeScreenShot(screen, flags, pid)) {
        send(std::move(*result), QVariantMap{
                                     {QStringLiteral("screen"), screen->name()},
                                 },
             message);
    } else {
        cancel(message);
    }
}

void ScreenShotDBusInterface3::takeScreenShot(const Rect &area, ScreenShotFlags flags, const QDBusMessage &message, std::optional<pid_t> pid)
{
    if (auto result = m_effect->takeScreenShot(area, flags, pid)) {
        send(std::move(*result), {}, message);
    } else {
        cancel(message);
    }
}

void ScreenShotDBusInterface3::takeScreenShot(Window *window, ScreenShotFlags flags, const QDBusMessage &message)
{
    if (auto result = m_effect->takeScreenShot(window, flags)) {
        send(std::move(*result), QVariantMap{
                                     {QStringLiteral("windowId"), window->internalId().toString()},
                                 },
             message);
    } else {
        cancel(message);
    }
}

void ScreenShotDBusInterface3::send(ScreenShotBuffer &&buffer, const QVariantMap &options, const QDBusMessage &message)
{
    // Note that the type of the data stored in the vardict matters. Be careful.
    QVariantMap results = options;
    results.insert(QStringLiteral("type"), QStringLiteral("raw"));
    results.insert(QStringLiteral("format"), quint32(buffer.format));
    results.insert(QStringLiteral("width"), quint32(buffer.size.width()));
    results.insert(QStringLiteral("height"), quint32(buffer.size.height()));
    results.insert(QStringLiteral("stride"), quint32(buffer.bytesPerLine));
    results.insert(QStringLiteral("scale"), double(buffer.devicePixelRatio));
    QDBusConnection::sessionBus().send(message.createReply({QVariant::fromValue(QDBusUnixFileDescriptor(buffer.file.take())), results}));
}

void ScreenShotDBusInterface3::cancel(const QDBusMessage &message)
{
    QDBusConnection::sessionBus().send(message.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
}

} // namespace KWin

#include "screenshotdbusinterface3.moc"

#include "moc_screenshotdbusinterface3.cpp"
