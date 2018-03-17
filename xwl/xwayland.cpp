/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwayland.h"
#include "databridge.h"

#include "main_wayland.h"
#include "options.h"
#include "utils.h"
#include "wayland_server.h"
#include "xcbutils.h"
#include "xwayland_logging.h"

#include <KLocalizedString>
#include <KNotification>
#include <KSelectionOwner>

#include <QAbstractEventDispatcher>
#include <QFile>
#include <QTimer>
#include <QtConcurrentRun>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_PROCCTL_H
#include <unistd.h>
#endif

#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <X11/Xauth.h>

static QByteArray readDisplay(int pipe)
{
    QByteArray displayName;
    QFile readPipe;

    if (!readPipe.open(pipe, QIODevice::ReadOnly)) {
        qCWarning(KWIN_XWL) << "Failed to open X11 display name pipe:" << readPipe.errorString();
    } else {
        displayName = readPipe.readLine();
        displayName.prepend(QByteArrayLiteral(":"));
        displayName.remove(displayName.size() - 1, 1);
    }

    // close our pipe
    close(pipe);
    return displayName;
}

namespace KWin
{
namespace Xwl
{

Xwayland::Xwayland(ApplicationWaylandAbstract *app, QObject *parent)
    : XwaylandInterface(parent)
    , m_app(app)
{
    m_resetCrashCountTimer = new QTimer(this);
    m_resetCrashCountTimer->setSingleShot(true);
    connect(m_resetCrashCountTimer, &QTimer::timeout, this, &Xwayland::resetCrashCount);
}

Xwayland::~Xwayland()
{
    stop();
}

static QByteArray getRandomData(qint64 bytes)
{
    QFile random(QStringLiteral("/dev/urandom"));
    if (!random.open(QIODevice::ReadOnly))
        return {};

    QByteArray data;
    data.resize(bytes);
    while (bytes) {
        auto bytesRead = random.read(data.data() + data.size() - bytes, bytes);
        if (bytesRead == -1)
            return {};

        bytes -= bytesRead;
    }

    return data;
}

static bool addCookieToFile(QString filename, QString display, QString &hostname)
{
    QByteArray cookie = getRandomData(16);
    QByteArray displayUtf8 = display.toUtf8();

    if(displayUtf8.size() < 2 || displayUtf8[0] != ':' || cookie.count() != 16) {
        return false;
    }

    FILE *authFp = fopen(qPrintable(filename), "wb");
    if (authFp == nullptr) {
        return false;
    }

    char localhost[HOST_NAME_MAX + 1] = "";
    if (gethostname(localhost, HOST_NAME_MAX) < 0) {
        strcpy(localhost, "localhost");
    }

    hostname = QString::fromUtf8(localhost);

    Xauth auth = {};
    char cookieName[] = "MIT-MAGIC-COOKIE-1";

    auth.family = FamilyLocal;
    auth.address = localhost;
    auth.address_length = strlen(auth.address);
    auth.number = displayUtf8.data() + 1;
    auth.number_length = strlen(auth.number);
    auth.name = cookieName;
    auth.name_length = sizeof(cookieName) - 1;
    auth.data = cookie.data();
    auth.data_length = cookie.count();

    if (XauWriteAuth(authFp, &auth) == 0) {
        fclose(authFp);
        return false;
    }

    auth.family = FamilyWild;
    auth.address_length = 0;
    bool success = XauWriteAuth(authFp, &auth) != 0 && fflush(authFp) != EOF;

    fclose(authFp);

    return success;
}

QProcess *Xwayland::process() const
{
    return m_xwaylandProcess;
}

void Xwayland::start()
{
    if (m_xwaylandProcess) {
        return;
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!dir.isEmpty()) {
        m_xwaylandAuthority.setFileTemplate(dir + QStringLiteral("/xauth_XXXXXX"));
        m_xwaylandAuthority.open();
    }

    if (m_xwaylandAuthority.fileName().isEmpty()) {
        std::cerr << "Warning: Could not create a Xauthority file for Xwayland." << std::endl;
    }

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        qCWarning(KWIN_XWL, "Failed to create pipe to start Xwayland: %s", strerror(errno));
        emit errorOccurred();
        return;
    }
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        emit errorOccurred();
        return;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        emit errorOccurred();
        return;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        emit errorOccurred();
        return;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        emit errorOccurred();
        return;
    }

    m_xcbConnectionFd = sx[0];
    m_displayFileDescriptor = pipeFds[0];

    m_xwaylandProcess = new Process(this);
    m_xwaylandProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    QProcessEnvironment env = m_app->processStartupEnvironment();
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    env.insert("EGL_PLATFORM", QByteArrayLiteral("DRM"));
    if (qEnvironmentVariableIsSet("KWIN_XWAYLAND_DEBUG")) {
        env.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
    }
    m_xwaylandProcess->setProcessEnvironment(env);
    QStringList args{QStringLiteral("-displayfd"),
                           QString::number(pipeFds[1]),
                           QStringLiteral("-rootless"),
                           QStringLiteral("-wm"),
                           QString::number(fd)};
    if (!m_xwaylandAuthority.fileName().isEmpty()) {
        args << QStringLiteral("-auth") << m_xwaylandAuthority.fileName();
    }
    m_xwaylandProcess->setArguments(args);
    connect(m_xwaylandProcess, &QProcess::errorOccurred, this, &Xwayland::handleXwaylandError);
    connect(m_xwaylandProcess, &QProcess::started, this, &Xwayland::handleXwaylandStarted);
    connect(m_xwaylandProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Xwayland::handleXwaylandFinished);
    m_xwaylandProcess->start();
    close(pipeFds[1]);
}

void Xwayland::stop()
{
    if (!m_xwaylandProcess) {
        return;
    }

    m_app->setClosingX11Connection(true);

    // If Xwayland has crashed, we must deactivate the socket notifier and ensure that no X11
    // events will be dispatched before blocking; otherwise we will simply hang...
    uninstallSocketNotifier();

    DataBridge::destroy();
    m_selectionOwner.reset();

    destroyX11Connection();

    // When the Xwayland process is finally terminated, the finished() signal will be emitted,
    // however we don't actually want to process it anymore. Furthermore, we also don't really
    // want to handle any errors that may occur during the teardown.
    if (m_xwaylandProcess->state() != QProcess::NotRunning) {
        disconnect(m_xwaylandProcess, nullptr, this, nullptr);
        m_xwaylandProcess->terminate();
        m_xwaylandProcess->waitForFinished(5000);
    }
    delete m_xwaylandProcess;
    m_xwaylandProcess = nullptr;

    waylandServer()->destroyXWaylandConnection(); // This one must be destroyed last!

    m_app->setClosingX11Connection(false);
}

void Xwayland::restart()
{
    stop();
    start();
}

void Xwayland::dispatchEvents()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        qCWarning(KWIN_XWL, "Attempting to dispatch X11 events with no connection");
        return;
    }

    const int connectionError = xcb_connection_has_error(connection);
    if (connectionError) {
        qCWarning(KWIN_XWL, "The X11 connection broke (error %d)", connectionError);
        stop();
        return;
    }

    while (xcb_generic_event_t *event = xcb_poll_for_event(connection)) {
        long result = 0;
        QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
        dispatcher->filterNativeEvent(QByteArrayLiteral("xcb_generic_event_t"), event, &result);
        free(event);
    }

    xcb_flush(connection);
}

void Xwayland::installSocketNotifier()
{
    const int fileDescriptor = xcb_get_file_descriptor(kwinApp()->x11Connection());

    m_socketNotifier = new QSocketNotifier(fileDescriptor, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &Xwayland::dispatchEvents);

    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &Xwayland::dispatchEvents);
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, &Xwayland::dispatchEvents);
}

void Xwayland::uninstallSocketNotifier()
{
    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    disconnect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &Xwayland::dispatchEvents);
    disconnect(dispatcher, &QAbstractEventDispatcher::awake, this, &Xwayland::dispatchEvents);

    delete m_socketNotifier;
    m_socketNotifier = nullptr;
}

void Xwayland::handleXwaylandStarted()
{
    m_watcher = new QFutureWatcher<QByteArray>(this);
    connect(m_watcher, &QFutureWatcher<QByteArray>::finished, this, &Xwayland::handleXwaylandReady);
    m_watcher->setFuture(QtConcurrent::run(readDisplay, m_displayFileDescriptor));
}

void Xwayland::handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(KWIN_XWL) << "Xwayland process has quit with exit code" << exitCode;

    switch (exitStatus) {
    case QProcess::NormalExit:
        stop();
        break;
    case QProcess::CrashExit:
        handleXwaylandCrashed();
        break;
    }
}

void Xwayland::handleXwaylandCrashed()
{
    KNotification::event(QStringLiteral("xwaylandcrash"), i18n("Xwayland has crashed"));
    m_resetCrashCountTimer->stop();

    switch (options->xwaylandCrashPolicy()) {
    case XwaylandCrashPolicy::Restart:
        if (++m_crashCount <= options->xwaylandMaxCrashCount()) {
            restart();
            m_resetCrashCountTimer->start(std::chrono::minutes(10));
        } else {
            qCWarning(KWIN_XWL, "Stopping Xwayland server because it has crashed %d times "
                      "over the past 10 minutes", m_crashCount);
            stop();
        }
        break;
    case XwaylandCrashPolicy::Stop:
        stop();
        break;
    }
}

void Xwayland::resetCrashCount()
{
    qCDebug(KWIN_XWL) << "Resetting the crash counter, its current value is" << m_crashCount;
    m_crashCount = 0;
}

void Xwayland::handleXwaylandError(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        qCWarning(KWIN_XWL) << "Xwayland process failed to start";
        return;
    case QProcess::Crashed:
        qCWarning(KWIN_XWL) << "Xwayland process crashed";
        break;
    case QProcess::Timedout:
        qCWarning(KWIN_XWL) << "Xwayland operation timed out";
        break;
    case QProcess::WriteError:
    case QProcess::ReadError:
        qCWarning(KWIN_XWL) << "An error occurred while communicating with Xwayland";
        break;
    case QProcess::UnknownError:
        qCWarning(KWIN_XWL) << "An unknown error has occurred in Xwayland";
        break;
    }
    emit errorOccurred();
}

void Xwayland::handleXwaylandReady()
{
    m_displayName = m_watcher->result();

    m_watcher->deleteLater();
    m_watcher = nullptr;

    if (!createX11Connection()) {
        emit errorOccurred();
        return;
    }

    qCInfo(KWIN_XWL) << "Xwayland server started on display" << m_displayName;
    qputenv("DISPLAY", m_displayName);

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    m_selectionOwner.reset(new KSelectionOwner("WM_S0", kwinApp()->x11Connection(), kwinApp()->x11RootWindow()));
    m_selectionOwner->claim(true);

    DataBridge::create(this);

    auto env = m_app->processStartupEnvironment();
    env.insert(QStringLiteral("DISPLAY"), m_displayName);
    m_app->setProcessStartupEnvironment(env);

    emit started();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort
}

bool Xwayland::createX11Connection()
{
    if (!m_xwaylandAuthority.fileName().isEmpty()) {
        QString hostname;
        if (addCookieToFile(m_xwaylandAuthority.fileName(), m_displayName, hostname)) {
            setenv("XAUTHORITY", qPrintable(m_xwaylandAuthority.fileName()), 1);
            setenv("XAUTHLOCALHOSTNAME", qPrintable(hostname), 1);
            auto env = m_app->processStartupEnvironment();
            env.insert(QStringLiteral("XAUTHORITY"), m_xwaylandAuthority.fileName());
            env.insert(QStringLiteral("XAUTHLOCALHOSTNAME"), hostname);
            m_app->setProcessStartupEnvironment(env);
        }
        else {
            qCWarning(KWIN_XWL) << "Could not generate Xauthority entry";
            // We can't authenticate using it so the server must not see any entries either
            m_xwaylandAuthority.resize(0);
        }
    }

    xcb_connection_t *connection = xcb_connect_to_fd(m_xcbConnectionFd, nullptr);

    const int errorCode = xcb_connection_has_error(connection);
    if (errorCode) {
        qCDebug(KWIN_XWL, "Failed to establish the XCB connection (error %d)", errorCode);
        return false;
    }

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    Q_ASSERT(screen);

    m_app->setX11Connection(connection);
    m_app->setX11DefaultScreen(screen);
    m_app->setX11ScreenNumber(0);
    m_app->setX11RootWindow(screen->root);

    m_app->createAtoms();
    m_app->installNativeX11EventFilter();

    installSocketNotifier();

    // Note that it's very important to have valid x11RootWindow(), x11ScreenNumber(), and
    // atoms when the rest of kwin is notified about the new X11 connection.
    emit m_app->x11ConnectionChanged();

    return true;
}

void Xwayland::destroyX11Connection()
{
    if (!m_app->x11Connection()) {
        return;
    }

    emit m_app->x11ConnectionAboutToBeDestroyed();

    Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
    m_app->destroyAtoms();
    m_app->removeNativeX11EventFilter();

    xcb_disconnect(m_app->x11Connection());
    m_xcbConnectionFd = -1;

    m_app->setX11Connection(nullptr);
    m_app->setX11DefaultScreen(nullptr);
    m_app->setX11ScreenNumber(-1);
    m_app->setX11RootWindow(XCB_WINDOW_NONE);

    emit m_app->x11ConnectionChanged();
}

DragEventReply Xwayland::dragMoveFilter(Toplevel *target, const QPoint &pos)
{
    DataBridge *bridge = DataBridge::self();
    if (!bridge) {
        return DragEventReply::Wayland;
    }
    return bridge->dragMoveFilter(target, pos);
}

} // namespace Xwl
} // namespace KWin
