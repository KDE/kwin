/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshotdbusinterface1.h"

#include <config-kwin.h>

#include "screenshotlogging.h"
#include "utils/serviceutils.h"

#include <KLocalizedString>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDir>
#include <QTemporaryFile>
#include <QtConcurrent>

#include <unistd.h>

namespace KWin
{

const static QString s_errorAlreadyTaking = QStringLiteral("org.kde.kwin.Screenshot.Error.AlreadyTaking");
const static QString s_errorAlreadyTakingMsg = QStringLiteral("A screenshot is already been taken");
const static QString s_errorNotAuthorized = QStringLiteral("org.kde.kwin.Screenshot.Error.NoAuthorized");
const static QString s_errorNotAuthorizedMsg = QStringLiteral("The process is not authorized to take a screenshot");
const static QString s_errorFd = QStringLiteral("org.kde.kwin.Screenshot.Error.FileDescriptor");
const static QString s_errorFdMsg = QStringLiteral("No valid file descriptor");
const static QString s_errorCancelled = QStringLiteral("org.kde.kwin.Screenshot.Error.Cancelled");
const static QString s_errorCancelledMsg = QStringLiteral("Screenshot got cancelled");
const static QString s_errorInvalidArea = QStringLiteral("org.kde.kwin.Screenshot.Error.InvalidArea");
const static QString s_errorInvalidAreaMsg = QStringLiteral("Invalid area requested");
const static QString s_errorInvalidScreen = QStringLiteral("org.kde.kwin.Screenshot.Error.InvalidScreen");
const static QString s_errorInvalidScreenMsg = QStringLiteral("Invalid screen requested");
const static QString s_dbusInterfaceName = QStringLiteral("org.kde.kwin.Screenshot");
const static QString s_errorScreenMissing = QStringLiteral("org.kde.kwin.Screenshot.Error.ScreenMissing");
const static QString s_errorScreenMissingMsg = QStringLiteral("Screen not found");

class ScreenShotSource1 : public QObject
{
    Q_OBJECT

public:
    explicit ScreenShotSource1(QObject *parent = nullptr);

    virtual QImage data() const = 0;
    virtual void marshal(ScreenShotSink1 *sink) = 0;

    virtual bool isCancelled() const = 0;
    virtual bool isCompleted() const = 0;

Q_SIGNALS:
    void cancelled();
    void completed();
};

class ScreenShotSourceBasic1 : public ScreenShotSource1
{
    Q_OBJECT

public:
    explicit ScreenShotSourceBasic1(const QFuture<QImage> &future);

    bool isCancelled() const override;
    bool isCompleted() const override;
    QImage data() const override;
    void marshal(ScreenShotSink1 *sink) override;

private:
    QFuture<QImage> m_future;
    QFutureWatcher<QImage> *m_watcher;
};

class ScreenShotSourceScreen1 : public ScreenShotSourceBasic1
{
    Q_OBJECT

public:
    ScreenShotSourceScreen1(ScreenShotEffect *effect, EffectScreen *screen, ScreenShotFlags flags);
};

class ScreenShotSourceArea1 : public ScreenShotSourceBasic1
{
    Q_OBJECT

public:
    ScreenShotSourceArea1(ScreenShotEffect *effect, const QRect &area, ScreenShotFlags flags);
};

class ScreenShotSourceWindow1 : public ScreenShotSourceBasic1
{
    Q_OBJECT

public:
    ScreenShotSourceWindow1(ScreenShotEffect *effect, EffectWindow *window, ScreenShotFlags flags);
};

class ScreenShotSourceMulti1 : public ScreenShotSource1
{
    Q_OBJECT

public:
    explicit ScreenShotSourceMulti1(const QList<ScreenShotSource1 *> &sources);

    bool isCancelled() const override;
    bool isCompleted() const override;
    QImage data() const override;
    void marshal(ScreenShotSink1 *sink) override;

private:
    void handleSourceCancelled();
    void handleSourceCompleted();

    QList<ScreenShotSource1 *> m_sources;
};

class ScreenShotSink1 : public QObject
{
    Q_OBJECT

public:
    explicit ScreenShotSink1(ScreenShotDBusInterface1 *interface,
                             QDBusMessage replyMessage = QDBusMessage());

    virtual void flush(const QImage &image);
    virtual void flushMulti(const QList<QImage> &images);

    virtual void cancel();

protected:
    ScreenShotDBusInterface1 *m_interface;
    QDBusMessage m_replyMessage;
};

class ScreenShotSinkPipe1 : public ScreenShotSink1
{
    Q_OBJECT

public:
    ScreenShotSinkPipe1(ScreenShotDBusInterface1 *interface, int fileDescriptor,
                        QDBusMessage replyMessage = QDBusMessage());
    ~ScreenShotSinkPipe1() override;

    void flush(const QImage &image) override;
    void flushMulti(const QList<QImage> &images) override;

private:
    int m_fileDescriptor;
};

class ScreenShotSinkXpixmap1 : public ScreenShotSink1
{
    Q_OBJECT

public:
    explicit ScreenShotSinkXpixmap1(ScreenShotDBusInterface1 *interface,
                                    QDBusMessage replyMessage = QDBusMessage());

    void flush(const QImage &image) override;
};

class ScreenShotSinkFile1 : public ScreenShotSink1
{
    Q_OBJECT

public:
    explicit ScreenShotSinkFile1(ScreenShotDBusInterface1 *interface,
                                 QDBusMessage replyMessage = QDBusMessage());

    void flush(const QImage &image) override;
};

ScreenShotSource1::ScreenShotSource1(QObject *parent)
    : QObject(parent)
{
}

ScreenShotSourceBasic1::ScreenShotSourceBasic1(const QFuture<QImage> &future)
    : m_future(future)
{
    m_watcher = new QFutureWatcher<QImage>(this);
    connect(m_watcher, &QFutureWatcher<QImage>::finished, this, &ScreenShotSource1::completed);
    connect(m_watcher, &QFutureWatcher<QImage>::canceled, this, &ScreenShotSource1::cancelled);
    m_watcher->setFuture(m_future);
}

bool ScreenShotSourceBasic1::isCancelled() const
{
    return m_future.isCanceled();
}

bool ScreenShotSourceBasic1::isCompleted() const
{
    return m_future.isFinished();
}

QImage ScreenShotSourceBasic1::data() const
{
    return m_future.result();
}

void ScreenShotSourceBasic1::marshal(ScreenShotSink1 *sink)
{
    sink->flush(data());
}

ScreenShotSourceScreen1::ScreenShotSourceScreen1(ScreenShotEffect *effect,
                                                 EffectScreen *screen,
                                                 ScreenShotFlags flags)
    : ScreenShotSourceBasic1(effect->scheduleScreenShot(screen, flags))
{
}

ScreenShotSourceArea1::ScreenShotSourceArea1(ScreenShotEffect *effect,
                                             const QRect &area,
                                             ScreenShotFlags flags)
    : ScreenShotSourceBasic1(effect->scheduleScreenShot(area, flags))
{
}

ScreenShotSourceWindow1::ScreenShotSourceWindow1(ScreenShotEffect *effect,
                                                 EffectWindow *window,
                                                 ScreenShotFlags flags)
    : ScreenShotSourceBasic1(effect->scheduleScreenShot(window, flags))
{
}

ScreenShotSourceMulti1::ScreenShotSourceMulti1(const QList<ScreenShotSource1 *> &sources)
    : m_sources(sources)
{
    for (ScreenShotSource1 *source : sources) {
        source->setParent(this);

        connect(source, &ScreenShotSource1::cancelled,
                this, &ScreenShotSourceMulti1::handleSourceCancelled);
        connect(source, &ScreenShotSource1::completed,
                this, &ScreenShotSourceMulti1::handleSourceCompleted);
    }
}

bool ScreenShotSourceMulti1::isCancelled() const
{
    return std::any_of(m_sources.begin(), m_sources.end(), [](const ScreenShotSource1 *source) {
        return source->isCancelled();
    });
}

bool ScreenShotSourceMulti1::isCompleted() const
{
    return std::all_of(m_sources.begin(), m_sources.end(), [](const ScreenShotSource1 *source) {
        return source->isCompleted();
    });
}

QImage ScreenShotSourceMulti1::data() const
{
    return QImage(); // We don't allow nesting multi screenshot sources.
}

void ScreenShotSourceMulti1::marshal(ScreenShotSink1 *sink)
{
    QList<QImage> images;
    images.reserve(m_sources.count());

    for (ScreenShotSource1 *source : std::as_const(m_sources)) {
        images.append(source->data());
    }

    sink->flushMulti(images);
}

void ScreenShotSourceMulti1::handleSourceCancelled()
{
    Q_EMIT cancelled();
}

void ScreenShotSourceMulti1::handleSourceCompleted()
{
    // If all sources are complete now, emit the completed signal.
    if (isCompleted()) {
        Q_EMIT completed();
    }
}

ScreenShotSink1::ScreenShotSink1(ScreenShotDBusInterface1 *interface, QDBusMessage message)
    : m_interface(interface)
    , m_replyMessage(message)
{
}

void ScreenShotSink1::flush(const QImage &image)
{
    qCWarning(KWIN_SCREENSHOT) << metaObject()->className() << "does not implement" << Q_FUNC_INFO;
}

void ScreenShotSink1::flushMulti(const QList<QImage> &images)
{
    qCWarning(KWIN_SCREENSHOT) << metaObject()->className() << "does not implement" << Q_FUNC_INFO;
}

void ScreenShotSink1::cancel()
{
    if (m_replyMessage.isDelayedReply()) {
        QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(s_errorCancelled,
                                                                           s_errorCancelledMsg));
    }
}

ScreenShotSinkPipe1::ScreenShotSinkPipe1(ScreenShotDBusInterface1 *interface, int fileDescriptor,
                                         QDBusMessage replyMessage)
    : ScreenShotSink1(interface, replyMessage)
    , m_fileDescriptor(fileDescriptor)
{
}

ScreenShotSinkPipe1::~ScreenShotSinkPipe1()
{
    if (m_fileDescriptor != -1) {
        close(m_fileDescriptor);
    }
}

void ScreenShotSinkPipe1::flush(const QImage &image)
{
    if (m_fileDescriptor == -1) {
        return;
    }

    QtConcurrent::run([](int fd, const QImage &image) {
        QFile file;
        if (file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
            QDataStream ds(&file);
            ds << image;
            file.close();
        } else {
            close(fd);
        }
    },
                      m_fileDescriptor, image);

    // The ownership of the pipe file descriptor has been moved to the worker thread.
    m_fileDescriptor = -1;
}

void ScreenShotSinkPipe1::flushMulti(const QList<QImage> &images)
{
    if (m_fileDescriptor == -1) {
        return;
    }

    QtConcurrent::run([](int fd, const QList<QImage> &images) {
        QFile file;
        if (file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
            QDataStream ds(&file);
            ds.setVersion(QDataStream::Qt_DefaultCompiledVersion);
            ds << images;
            file.close();
        } else {
            close(fd);
        }
    },
                      m_fileDescriptor, images);

    // The ownership of the pipe file descriptor has been moved to the worker thread.
    m_fileDescriptor = -1;
}

ScreenShotSinkXpixmap1::ScreenShotSinkXpixmap1(ScreenShotDBusInterface1 *interface,
                                               QDBusMessage replyMessage)
    : ScreenShotSink1(interface, replyMessage)
{
}

static QSize pickWindowSize(const QImage &image)
{
    xcb_connection_t *c = effects->xcbConnection();

    // This will implicitly enable BIG-REQUESTS extension.
    const uint32_t maximumRequestSize = xcb_get_maximum_request_length(c);
    const xcb_setup_t *setup = xcb_get_setup(c);

    uint32_t requestSize = sizeof(xcb_put_image_request_t);

    // With BIG-REQUESTS extension an additional 32-bit field is inserted into
    // the request so we better take it into account.
    if (setup->maximum_request_length < maximumRequestSize) {
        requestSize += 4;
    }

    const uint32_t maximumDataSize = 4 * maximumRequestSize - requestSize;
    const uint32_t bytesPerPixel = image.depth() >> 3;
    const uint32_t bytesPerLine = image.bytesPerLine();

    if (image.sizeInBytes() <= maximumDataSize) {
        return image.size();
    }

    if (maximumDataSize < bytesPerLine) {
        return QSize(maximumDataSize / bytesPerPixel, 1);
    }

    return QSize(image.width(), maximumDataSize / bytesPerLine);
}

static xcb_pixmap_t xpixmapFromImage(const QImage &image)
{
    xcb_connection_t *c = effects->xcbConnection();

    xcb_pixmap_t pixmap = xcb_generate_id(c);
    xcb_gcontext_t gc = xcb_generate_id(c);

    xcb_create_pixmap(c, image.depth(), pixmap, effects->x11RootWindow(),
                      image.width(), image.height());
    xcb_create_gc(c, gc, pixmap, 0, nullptr);

    const int bytesPerPixel = image.depth() >> 3;

    // Figure out how much data we can send with one invocation of xcb_put_image.
    // In contrast to XPutImage, xcb_put_image doesn't implicitly split the data.
    const QSize window = pickWindowSize(image);

    for (int i = 0; i < image.height(); i += window.height()) {
        const int targetHeight = std::min(image.height() - i, window.height());
        const uint8_t *line = image.scanLine(i);

        for (int j = 0; j < image.width(); j += window.width()) {
            const int targetWidth = std::min(image.width() - j, window.width());
            const uint8_t *bytes = line + j * bytesPerPixel;
            const uint32_t byteCount = targetWidth * targetHeight * bytesPerPixel;

            xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, gc,
                          targetWidth, targetHeight, j, i, 0, image.depth(),
                          byteCount, bytes);
        }
    }

    xcb_flush(c);
    xcb_free_gc(c, gc);

    return pixmap;
}

void ScreenShotSinkXpixmap1::flush(const QImage &image)
{
    const xcb_pixmap_t pixmap = xpixmapFromImage(image);
    Q_EMIT m_interface->screenshotCreated(pixmap);
}

ScreenShotSinkFile1::ScreenShotSinkFile1(ScreenShotDBusInterface1 *interface,
                                         QDBusMessage replyMessage)
    : ScreenShotSink1(interface, replyMessage)
{
}

static QString saveTempImage(const QImage &image)
{
    if (image.isNull()) {
        return QString();
    }
    QTemporaryFile temp(QDir::tempPath() + QDir::separator() + QLatin1String("kwin_screenshot_XXXXXX.png"));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return QString();
    }
    image.save(&temp);
    temp.close();
    qCInfo(KWIN_SCREENSHOT) << "Screenshot saved to" << temp.fileName();
#if KWIN_BUILD_NOTIFICATIONS
    KNotification::event(KNotification::Notification,
                         i18nc("Notification caption that a screenshot got saved to file", "Screenshot"),
                         i18nc("Notification with path to screenshot file", "Screenshot saved to %1", temp.fileName()),
                         QStringLiteral("spectacle"));
#endif
    return temp.fileName();
}

void ScreenShotSinkFile1::flush(const QImage &image)
{
    QDBusConnection::sessionBus().send(m_replyMessage.createReply(saveTempImage(image)));
}

ScreenShotDBusInterface1::ScreenShotDBusInterface1(ScreenShotEffect *effect, QObject *parent)
    : QObject(parent)
    , m_effect(effect)
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Screenshot"),
                                                 this,
                                                 QDBusConnection::ExportScriptableContents);
}

ScreenShotDBusInterface1::~ScreenShotDBusInterface1()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Screenshot"));
}

bool ScreenShotDBusInterface1::checkCall() const
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
        if (!interfaces.contains(s_dbusInterfaceName)) {
            sendErrorReply(s_errorNotAuthorized, s_errorNotAuthorizedMsg);
            qCWarning(KWIN_SCREENSHOT) << "Process" << pid << "tried to take a screenshot without being granted to DBus interface" << s_dbusInterfaceName;
            return false;
        }
    } else {
        return false;
    }

    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return false;
    }
    return true;
}

void ScreenShotDBusInterface1::screenshotWindowUnderCursor(int mask)
{
    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return;
    }

    EffectWindow *hoveredWindow = nullptr;

    const QPoint cursor = effects->cursorPos();
    EffectWindowList order = effects->stackingOrder();
    EffectWindowList::const_iterator it = order.constEnd(), first = order.constBegin();
    while (it != first) {
        hoveredWindow = *(--it);
        if (hoveredWindow->isOnCurrentDesktop()
            && !hoveredWindow->isMinimized()
            && !hoveredWindow->isDeleted()
            && hoveredWindow->frameGeometry().contains(cursor)) {
            break;
        }
        hoveredWindow = nullptr;
    }

    if (hoveredWindow) {
        takeScreenShot(hoveredWindow, ScreenShotFlags(mask),
                       new ScreenShotSinkXpixmap1(this, message()));
    }
}

void ScreenShotDBusInterface1::screenshotForWindow(qulonglong winId, int mask)
{
    EffectWindow *window = effects->findWindow(winId);
    if (!window || window->isMinimized() || window->isDeleted()) {
        return;
    }

    takeScreenShot(window, ScreenShotFlags(mask), new ScreenShotSinkXpixmap1(this, message()));
}

QString ScreenShotDBusInterface1::interactive(int mask)
{
    if (!calledFromDBus()) {
        return QString();
    }
    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return QString();
    }

    const QDBusMessage replyMessage = message();
    setDelayedReply(true);

    effects->startInteractiveWindowSelection([this, mask, replyMessage](EffectWindow *window) {
        hideInfoMessage();
        if (!window) {
            QDBusConnection::sessionBus().send(replyMessage.createErrorReply(s_errorCancelled,
                                                                             s_errorCancelledMsg));
            return;
        }
        takeScreenShot(window, ScreenShotFlags(mask), new ScreenShotSinkFile1(this, replyMessage));
    });

    showInfoMessage(InfoMessageMode::Window);
    return QString();
}

void ScreenShotDBusInterface1::interactive(QDBusUnixFileDescriptor fd, int mask)
{
    if (!calledFromDBus()) {
        return;
    }
    if (isTakingScreenshot()) {
        sendErrorReply(s_errorAlreadyTaking, s_errorAlreadyTakingMsg);
        return;
    }

    const int fileDescriptor = dup(fd.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }

    effects->startInteractiveWindowSelection([this, fileDescriptor, mask](EffectWindow *window) {
        hideInfoMessage();
        if (!window) {
            close(fileDescriptor);
            return;
        }
        takeScreenShot(window, ScreenShotFlags(mask),
                       new ScreenShotSinkPipe1(this, fileDescriptor));
    });

    showInfoMessage(InfoMessageMode::Window);
}

QString ScreenShotDBusInterface1::screenshotFullscreen(bool captureCursor)
{
    if (!checkCall()) {
        return QString();
    }

    ScreenShotFlags flags = ScreenShotFlags();
    if (captureCursor) {
        flags |= ScreenShotIncludeCursor;
    }

    takeScreenShot(effects->virtualScreenGeometry(), flags, new ScreenShotSinkFile1(this, message()));

    setDelayedReply(true);
    return QString();
}

void ScreenShotDBusInterface1::screenshotFullscreen(QDBusUnixFileDescriptor fd,
                                                    bool captureCursor, bool shouldReturnNativeSize)
{
    if (!checkCall()) {
        return;
    }

    const int fileDescriptor = dup(fd.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }

    ScreenShotFlags flags = ScreenShotFlags();
    if (captureCursor) {
        flags |= ScreenShotIncludeCursor;
    }
    if (shouldReturnNativeSize) {
        flags |= ScreenShotNativeResolution;
    }

    takeScreenShot(effects->virtualScreenGeometry(), flags,
                   new ScreenShotSinkPipe1(this, fileDescriptor));
}

QString ScreenShotDBusInterface1::screenshotScreen(int screenId, bool captureCursor)
{
    if (!checkCall()) {
        return QString();
    }

    EffectScreen *screen = effects->findScreen(screenId);
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMsg);
        return QString();
    }

    ScreenShotFlags flags = ScreenShotNativeResolution;
    if (captureCursor) {
        flags |= ScreenShotIncludeCursor;
    }

    takeScreenShot(screen, flags, new ScreenShotSinkFile1(this, message()));

    setDelayedReply(true);
    return QString();
}

void ScreenShotDBusInterface1::screenshotScreen(QDBusUnixFileDescriptor fd, bool captureCursor)
{
    if (!checkCall()) {
        return;
    }

    const int fileDescriptor = dup(fd.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }

    ScreenShotFlags flags = ScreenShotNativeResolution;
    if (captureCursor) {
        flags |= ScreenShotIncludeCursor;
    }

    effects->startInteractivePositionSelection([this, fileDescriptor, flags](const QPoint &p) {
        hideInfoMessage();
        if (p == QPoint(-1, -1)) {
            close(fileDescriptor);
        } else {
            EffectScreen *screen = effects->screenAt(p);
            if (!screen) {
                close(fileDescriptor);
                return;
            }
            takeScreenShot(screen, flags, new ScreenShotSinkPipe1(this, fileDescriptor));
        }
    });
    showInfoMessage(InfoMessageMode::Screen);
}

void ScreenShotDBusInterface1::screenshotScreens(QDBusUnixFileDescriptor fd,
                                                 const QStringList &screensNames,
                                                 bool captureCursor, bool shouldReturnNativeSize)
{
    if (!checkCall()) {
        return;
    }

    const int fileDescriptor = dup(fd.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFd, s_errorFdMsg);
        return;
    }

    ScreenShotFlags flags = ScreenShotFlags();
    if (captureCursor) {
        flags |= ScreenShotIncludeCursor;
    }
    if (shouldReturnNativeSize) {
        flags |= ScreenShotNativeResolution;
    }

    QList<EffectScreen *> screens;
    screens.reserve(screensNames.count());

    for (const QString &screenName : screensNames) {
        EffectScreen *screen = effects->findScreen(screenName);
        if (!screen) {
            close(fileDescriptor);
            sendErrorReply(s_errorScreenMissing, s_errorScreenMissingMsg + ": " + screenName);
            return;
        }
        screens.append(screen);
    }

    ScreenShotSinkPipe1 *sink = new ScreenShotSinkPipe1(this, fileDescriptor);
    takeScreenShot(screens, flags, sink);
}

QString ScreenShotDBusInterface1::screenshotArea(int x, int y, int width, int height, bool captureCursor)
{
    if (!checkCall()) {
        return QString();
    }

    const QRect area(x, y, width, height);
    if (area.isEmpty()) {
        sendErrorReply(s_errorInvalidArea, s_errorInvalidAreaMsg);
        return QString();
    }

    ScreenShotFlags flags = ScreenShotFlags();
    if (captureCursor) {
        flags |= ScreenShotIncludeCursor;
    }

    takeScreenShot(area, flags, new ScreenShotSinkFile1(this, message()));
    setDelayedReply(true);

    return QString();
}

bool ScreenShotDBusInterface1::isTakingScreenshot() const
{
    return m_source != nullptr;
}

void ScreenShotDBusInterface1::showInfoMessage(InfoMessageMode mode)
{
    QString text;
    switch (mode) {
    case InfoMessageMode::Window:
        text = i18n("Select window to screen shot with left click or enter.\nEscape or right click to cancel.");
        break;
    case InfoMessageMode::Screen:
        text = i18n("Create screen shot with left click or enter.\nEscape or right click to cancel.");
        break;
    }
    effects->showOnScreenMessage(text, QStringLiteral("spectacle"));
}

void ScreenShotDBusInterface1::hideInfoMessage()
{
    effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);
}

void ScreenShotDBusInterface1::handleSourceCancelled()
{
    m_sink->cancel();

    m_source.reset();
    m_sink.reset();
}

void ScreenShotDBusInterface1::handleSourceCompleted()
{
    m_source->marshal(m_sink.get());

    m_source.reset();
    m_sink.reset();
}

void ScreenShotDBusInterface1::bind(ScreenShotSink1 *sink, ScreenShotSource1 *source)
{
    m_sink.reset(sink);
    m_source.reset(source);

    connect(m_source.get(), &ScreenShotSource1::cancelled,
            this, &ScreenShotDBusInterface1::handleSourceCancelled);
    connect(m_source.get(), &ScreenShotSource1::completed,
            this, &ScreenShotDBusInterface1::handleSourceCompleted);
}

void ScreenShotDBusInterface1::takeScreenShot(EffectScreen *screen, ScreenShotFlags flags,
                                              ScreenShotSink1 *sink)
{
    bind(sink, new ScreenShotSourceScreen1(m_effect, screen, flags));
}

void ScreenShotDBusInterface1::takeScreenShot(const QList<EffectScreen *> &screens,
                                              ScreenShotFlags flags, ScreenShotSink1 *sink)
{
    QList<ScreenShotSource1 *> sources;
    sources.reserve(screens.count());
    for (EffectScreen *screen : screens) {
        sources.append(new ScreenShotSourceScreen1(m_effect, screen, flags));
    }

    bind(sink, new ScreenShotSourceMulti1(sources));
}

void ScreenShotDBusInterface1::takeScreenShot(const QRect &area, ScreenShotFlags flags,
                                              ScreenShotSink1 *sink)
{
    bind(sink, new ScreenShotSourceArea1(m_effect, area, flags));
}

void ScreenShotDBusInterface1::takeScreenShot(EffectWindow *window, ScreenShotFlags flags,
                                              ScreenShotSink1 *sink)
{
    bind(sink, new ScreenShotSourceWindow1(m_effect, window, flags));
}

} // namespace KWin

#include "screenshotdbusinterface1.moc"
