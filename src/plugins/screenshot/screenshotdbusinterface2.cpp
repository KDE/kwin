/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenshotdbusinterface2.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "screenshot2adaptor.h"
#include "screenshotlogging.h"
#include "utils/filedescriptor.h"
#include "utils/serviceutils.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QThreadPool>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

namespace KWin
{

class ScreenShotWriter2 : public QRunnable
{
public:
    ScreenShotWriter2(FileDescriptor &&fileDescriptor, const QImage &image)
        : m_fileDescriptor(std::move(fileDescriptor))
        , m_image(image)
    {
    }

    void run() override
    {
        const int flags = fcntl(m_fileDescriptor.get(), F_GETFL, 0);
        if (flags == -1) {
            qCWarning(KWIN_SCREENSHOT) << "failed to get screenshot fd flags:" << strerror(errno);
            return;
        }
        if (!(flags & O_NONBLOCK)) {
            if (fcntl(m_fileDescriptor.get(), F_SETFL, flags | O_NONBLOCK) == -1) {
                qCWarning(KWIN_SCREENSHOT) << "failed to make screenshot fd non blocking:" << strerror(errno);
                return;
            }
        }

        QFile file;
        if (!file.open(m_fileDescriptor.get(), QIODevice::WriteOnly)) {
            qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "failed to open pipe:" << file.errorString();
            return;
        }

        const QByteArrayView buffer(m_image.constBits(), m_image.sizeInBytes());
        qint64 remainingSize = buffer.size();

        pollfd pfds[1];
        pfds[0].fd = m_fileDescriptor.get();
        pfds[0].events = POLLOUT;

        while (true) {
            const int ready = poll(pfds, 1, 60000);
            if (ready < 0) {
                if (errno != EINTR) {
                    qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "poll() failed:" << strerror(errno);
                    return;
                }
            } else if (ready == 0) {
                qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "timed out writing to pipe";
                return;
            } else if (!(pfds[0].revents & POLLOUT)) {
                qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "pipe is broken";
                return;
            } else {
                const char *chunk = buffer.constData() + (buffer.size() - remainingSize);
                const qint64 writtenCount = file.write(chunk, remainingSize);

                if (writtenCount < 0) {
                    qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "write() failed:" << file.errorString();
                    return;
                }

                remainingSize -= writtenCount;
                if (writtenCount == 0 || remainingSize == 0) {
                    return;
                }
            }
        }
    }

protected:
    FileDescriptor m_fileDescriptor;
    QImage m_image;
};

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

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.ScreenShot2");
static const QString s_dbusInterface = QStringLiteral("org.kde.KWin.ScreenShot2");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot2");

static const QString s_errorNotAuthorized = QStringLiteral("org.kde.KWin.ScreenShot2.Error.NoAuthorized");
static const QString s_errorNotAuthorizedMessage = QStringLiteral("The process is not authorized to take a screenshot");
static const QString s_errorCancelled = QStringLiteral("org.kde.KWin.ScreenShot2.Error.Cancelled");
static const QString s_errorCancelledMessage = QStringLiteral("Screenshot got cancelled");
static const QString s_errorInvalidWindow = QStringLiteral("org.kde.KWin.ScreenShot2.Error.InvalidWindow");
static const QString s_errorInvalidWindowMessage = QStringLiteral("Invalid window requested");
static const QString s_errorInvalidArea = QStringLiteral("org.kde.KWin.ScreenShot2.Error.InvalidArea");
static const QString s_errorInvalidAreaMessage = QStringLiteral("Invalid area requested");
static const QString s_errorInvalidScreen = QStringLiteral("org.kde.KWin.ScreenShot2.Error.InvalidScreen");
static const QString s_errorInvalidScreenMessage = QStringLiteral("Invalid screen requested");
static const QString s_errorFileDescriptor = QStringLiteral("org.kde.KWin.ScreenShot2.Error.FileDescriptor");
static const QString s_errorFileDescriptorMessage = QStringLiteral("No valid file descriptor");

class ScreenShotSource2 : public QObject
{
    Q_OBJECT

public:
    explicit ScreenShotSource2(const QFuture<QImage> &future);

    void marshal(ScreenShotSinkPipe2 *sink);

    virtual QVariantMap attributes() const;

Q_SIGNALS:
    void cancelled();
    void completed();

private:
    QFuture<QImage> m_future;
    QFutureWatcher<QImage> *m_watcher;
};

class ScreenShotSourceScreen2 : public ScreenShotSource2
{
    Q_OBJECT

public:
    ScreenShotSourceScreen2(ScreenShotEffect *effect, Output *screen, ScreenShotFlags flags);

    QVariantMap attributes() const override;

private:
    QString m_name;
};

class ScreenShotSourceArea2 : public ScreenShotSource2
{
    Q_OBJECT

public:
    ScreenShotSourceArea2(ScreenShotEffect *effect, const QRect &area, ScreenShotFlags flags);
};

class ScreenShotSourceWindow2 : public ScreenShotSource2
{
    Q_OBJECT

public:
    ScreenShotSourceWindow2(ScreenShotEffect *effect, EffectWindow *window, ScreenShotFlags flags);

    QVariantMap attributes() const override;

private:
    QUuid m_internalId;
};

class ScreenShotSinkPipe2 : public QObject
{
    Q_OBJECT

public:
    ScreenShotSinkPipe2(int fileDescriptor, QDBusMessage replyMessage);

    void cancel();
    void flush(const QImage &image, const QVariantMap &attributes);

private:
    QDBusMessage m_replyMessage;
    FileDescriptor m_fileDescriptor;
};

ScreenShotSource2::ScreenShotSource2(const QFuture<QImage> &future)
    : m_future(future)
{
    m_watcher = new QFutureWatcher<QImage>(this);
    connect(m_watcher, &QFutureWatcher<QImage>::finished, this, [this]() {
        if (!m_future.isValid()) {
            Q_EMIT cancelled();
        } else {
            Q_EMIT completed();
        }
    });
    m_watcher->setFuture(m_future);
}

QVariantMap ScreenShotSource2::attributes() const
{
    return QVariantMap();
}

void ScreenShotSource2::marshal(ScreenShotSinkPipe2 *sink)
{
    sink->flush(m_future.result(), attributes());
}

ScreenShotSourceScreen2::ScreenShotSourceScreen2(ScreenShotEffect *effect,
                                                 Output *screen,
                                                 ScreenShotFlags flags)
    : ScreenShotSource2(effect->scheduleScreenShot(screen, flags))
    , m_name(screen->name())
{
}

QVariantMap ScreenShotSourceScreen2::attributes() const
{
    return QVariantMap{
        {QStringLiteral("screen"), m_name},
    };
}

ScreenShotSourceArea2::ScreenShotSourceArea2(ScreenShotEffect *effect,
                                             const QRect &area,
                                             ScreenShotFlags flags)
    : ScreenShotSource2(effect->scheduleScreenShot(area, flags))
{
}

ScreenShotSourceWindow2::ScreenShotSourceWindow2(ScreenShotEffect *effect,
                                                 EffectWindow *window,
                                                 ScreenShotFlags flags)
    : ScreenShotSource2(effect->scheduleScreenShot(window, flags))
    , m_internalId(window->internalId())
{
}

QVariantMap ScreenShotSourceWindow2::attributes() const
{
    return QVariantMap{
        {QStringLiteral("windowId"), m_internalId.toString()},
    };
}

ScreenShotSinkPipe2::ScreenShotSinkPipe2(int fileDescriptor, QDBusMessage replyMessage)
    : m_replyMessage(replyMessage)
    , m_fileDescriptor(fileDescriptor)
{
}

void ScreenShotSinkPipe2::cancel()
{
    QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(s_errorCancelled,
                                                                       s_errorCancelledMessage));
}

void ScreenShotSinkPipe2::flush(const QImage &image, const QVariantMap &attributes)
{
    if (!m_fileDescriptor.isValid()) {
        return;
    }

    // Note that the type of the data stored in the vardict matters. Be careful.
    QVariantMap results = attributes;
    results.insert(QStringLiteral("type"), QStringLiteral("raw"));
    results.insert(QStringLiteral("format"), quint32(image.format()));
    results.insert(QStringLiteral("width"), quint32(image.width()));
    results.insert(QStringLiteral("height"), quint32(image.height()));
    results.insert(QStringLiteral("stride"), quint32(image.bytesPerLine()));
    results.insert(QStringLiteral("scale"), double(image.devicePixelRatio()));
    QDBusConnection::sessionBus().send(m_replyMessage.createReply(results));

    auto writer = new ScreenShotWriter2(std::move(m_fileDescriptor), image);
    writer->setAutoDelete(true);
    QThreadPool::globalInstance()->start(writer);
}

ScreenShotDBusInterface2::ScreenShotDBusInterface2(ScreenShotEffect *effect)
    : QObject(effect)
    , m_effect(effect)
{
    new ScreenShot2Adaptor(this);

    QDBusConnection::sessionBus().registerObject(s_dbusObjectPath, this);
    QDBusConnection::sessionBus().registerService(s_dbusServiceName);
}

ScreenShotDBusInterface2::~ScreenShotDBusInterface2()
{
    QDBusConnection::sessionBus().unregisterService(s_dbusServiceName);
    QDBusConnection::sessionBus().unregisterObject(s_dbusObjectPath);
}

int ScreenShotDBusInterface2::version() const
{
    return 4;
}

bool ScreenShotDBusInterface2::checkPermissions() const
{
    if (!calledFromDBus()) {
        return false;
    }

    static bool permissionCheckDisabled = qEnvironmentVariableIntValue("KWIN_SCREENSHOT_NO_PERMISSION_CHECKS") == 1;
    if (permissionCheckDisabled) {
        return true;
    }

    const QDBusReply<uint> reply = connection().interface()->servicePid(message().service());
    if (reply.isValid()) {
        const uint pid = reply.value();
        const auto interfaces = KWin::fetchRestrictedDBusInterfacesFromPid(pid);
        if (!interfaces.contains(s_dbusInterface)) {
            sendErrorReply(s_errorNotAuthorized, s_errorNotAuthorizedMessage);
            return false;
        }
    } else {
        return false;
    }

    return true;
}

QVariantMap ScreenShotDBusInterface2::CaptureActiveWindow(const QVariantMap &options,
                                                          QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    EffectWindow *window = effects->activeWindow();
    if (!window) {
        sendErrorReply(s_errorInvalidWindow, s_errorInvalidWindowMessage);
        return QVariantMap();
    }

    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(window, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureWindow(const QString &handle,
                                                    const QVariantMap &options,
                                                    QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    EffectWindow *window = effects->findWindow(QUuid(handle));
    if (!window) {
        bool ok;
        const int winId = handle.toInt(&ok);
        if (ok) {
            window = effects->findWindow(winId);
        } else {
            qCWarning(KWIN_SCREENSHOT) << "Invalid handle:" << handle;
        }
    }
    if (!window) {
        sendErrorReply(s_errorInvalidWindow, s_errorInvalidWindowMessage);
        return QVariantMap();
    }

    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(window, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureArea(int x, int y, int width, int height,
                                                  const QVariantMap &options,
                                                  QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    const QRect area(x, y, width, height);
    if (area.isEmpty()) {
        sendErrorReply(s_errorInvalidArea, s_errorInvalidAreaMessage);
        return QVariantMap();
    }

    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(area, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureScreen(const QString &name,
                                                    const QVariantMap &options,
                                                    QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    Output *screen = effects->findScreen(name);
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMessage);
        return QVariantMap();
    }

    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(screen, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureActiveScreen(const QVariantMap &options,
                                                          QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    Output *screen = effects->activeScreen();
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMessage);
        return QVariantMap();
    }

    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(screen, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureInteractive(uint kind,
                                                         const QVariantMap &options,
                                                         QDBusUnixFileDescriptor pipe)
{
    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    const QDBusMessage replyMessage = message();

    if (kind == 0) {
        effects->startInteractiveWindowSelection([=, this](EffectWindow *window) {
            effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);

            if (!window) {
                close(fileDescriptor);

                QDBusConnection bus = QDBusConnection::sessionBus();
                bus.send(replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
            } else {
                takeScreenShot(window, screenShotFlagsFromOptions(options),
                               new ScreenShotSinkPipe2(fileDescriptor, replyMessage));
            }
        });
        effects->showOnScreenMessage(i18n("Select window to screen shot with left click or enter.\n"
                                          "Escape or right click to cancel."),
                                     QStringLiteral("spectacle"));
    } else {
        effects->startInteractivePositionSelection([=, this](const QPointF &point) {
            effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);

            if (point == QPoint(-1, -1)) {
                close(fileDescriptor);

                QDBusConnection bus = QDBusConnection::sessionBus();
                bus.send(replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
            } else {
                Output *screen = effects->screenAt(point.toPoint());
                takeScreenShot(screen, screenShotFlagsFromOptions(options),
                               new ScreenShotSinkPipe2(fileDescriptor, replyMessage));
            }
        });
        effects->showOnScreenMessage(i18n("Create screen shot with left click or enter.\n"
                                          "Escape or right click to cancel."),
                                     QStringLiteral("spectacle"));
    }

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureWorkspace(const QVariantMap &options, QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    const int fileDescriptor = fcntl(pipe.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(effects->virtualScreenGeometry(), screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

void ScreenShotDBusInterface2::bind(ScreenShotSinkPipe2 *sink, ScreenShotSource2 *source)
{
    connect(source, &ScreenShotSource2::cancelled, sink, [sink, source]() {
        sink->cancel();

        sink->deleteLater();
        source->deleteLater();
    });

    connect(source, &ScreenShotSource2::completed, sink, [sink, source]() {
        source->marshal(sink);

        sink->deleteLater();
        source->deleteLater();
    });
}

void ScreenShotDBusInterface2::takeScreenShot(Output *screen, ScreenShotFlags flags,
                                              ScreenShotSinkPipe2 *sink)
{
    bind(sink, new ScreenShotSourceScreen2(m_effect, screen, flags));
}

void ScreenShotDBusInterface2::takeScreenShot(const QRect &area, ScreenShotFlags flags,
                                              ScreenShotSinkPipe2 *sink)
{
    bind(sink, new ScreenShotSourceArea2(m_effect, area, flags));
}

void ScreenShotDBusInterface2::takeScreenShot(EffectWindow *window, ScreenShotFlags flags,
                                              ScreenShotSinkPipe2 *sink)
{
    bind(sink, new ScreenShotSourceWindow2(m_effect, window, flags));
}

} // namespace KWin

#include "screenshotdbusinterface2.moc"

#include "moc_screenshotdbusinterface2.cpp"
