/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "main_wayland.h"
#include "composite.h"
#include "virtualkeyboard.h"
#include "workspace.h"
#include <config-kwin.h>
// kwin
#include "platform.h"
#include "effects.h"
#include "wayland_server.h"
#include "xcbutils.h"

// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
// KDE
#include <KLocalizedString>
#include <KPluginLoader>
#include <KPluginMetaData>
// Qt
#include <qplatformdefs.h>
#include <QAbstractEventDispatcher>
#include <QCommandLineParser>
#include <QtConcurrentRun>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QSocketNotifier>
#include <QStyle>
#include <QThread>
#include <QDebug>
#include <QWindow>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#if HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#if HAVE_SYS_PROCCTL_H
#include <unistd.h>
#include <sys/procctl.h>
#endif

#include <iostream>
#include <iomanip>

namespace KWin
{

static void sighandler(int)
{
    QApplication::exit();
}

static void readDisplay(int pipe);

//************************************
// ApplicationWayland
//************************************

ApplicationWayland::ApplicationWayland(int &argc, char **argv)
    : Application(OperationModeWaylandAndX11, argc, argv)
{
}

ApplicationWayland::~ApplicationWayland()
{
    if (!waylandServer()) {
        return;
    }

    if (kwinApp()->platform()) {
        kwinApp()->platform()->setOutputsEnabled(false);
    }
    // need to unload all effects prior to destroying X connection as they might do X calls
    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->unloadAllEffects();
    }
    destroyWorkspace();
    waylandServer()->dispatch();
    disconnect(m_xwaylandFailConnection);
    if (x11Connection()) {
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
        destroyAtoms();
        emit x11ConnectionAboutToBeDestroyed();
        xcb_disconnect(x11Connection());
        setX11Connection(nullptr);
    }
    if (m_xwaylandProcess) {
        m_xwaylandProcess->terminate();
        while (m_xwaylandProcess->state() != QProcess::NotRunning) {
            processEvents(QEventLoop::WaitForMoreEvents);
        }
        waylandServer()->destroyXWaylandConnection();
    }
    if (QStyle *s = style()) {
        s->unpolish(this);
    }
    waylandServer()->terminateClientConnections();
    destroyCompositor();
}

void ApplicationWayland::performStartup()
{
    setOperationMode(m_startXWayland ? OperationModeXwayland : OperationModeWaylandAndX11);
    // first load options - done internally by a different thread
    createOptions();
    waylandServer()->createInternalConnection();

    // try creating the Wayland Backend
    createInput();
    VirtualKeyboard::create(this);
    createBackend();
}

void ApplicationWayland::createBackend()
{
    connect(platform(), &Platform::screensQueried, this, &ApplicationWayland::continueStartupWithScreens);
    connect(platform(), &Platform::initFailed, this,
        [] () {
            std::cerr <<  "FATAL ERROR: backend failed to initialize, exiting now" << std::endl;
            QCoreApplication::exit(1);
        }
    );
    platform()->init();
}

void ApplicationWayland::continueStartupWithScreens()
{
    disconnect(kwinApp()->platform(), &Platform::screensQueried, this, &ApplicationWayland::continueStartupWithScreens);
    createScreens();

    if (!m_startXWayland) {
        continueStartupWithX();
        return;
    }
    createCompositor();
    connect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::startXwaylandServer);
}

void ApplicationWayland::continueStartupWithX()
{
    createX11Connection();
    xcb_connection_t *c = x11Connection();
    if (!c) {
        // about to quit
        return;
    }
    QSocketNotifier *notifier = new QSocketNotifier(xcb_get_file_descriptor(c), QSocketNotifier::Read, this);
    auto processXcbEvents = [this, c] {
        while (auto event = xcb_poll_for_event(c)) {
            updateX11Time(event);
            long result = 0;
            if (QThread::currentThread()->eventDispatcher()->filterNativeEvent(QByteArrayLiteral("xcb_generic_event_t"), event, &result)) {
                free(event);
                continue;
            }
            if (Workspace::self()) {
                Workspace::self()->workspaceEvent(event);
            }
            free(event);
        }
        xcb_flush(c);
    };
    connect(notifier, &QSocketNotifier::activated, this, processXcbEvents);
    connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, processXcbEvents);
    connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::awake, this, processXcbEvents);

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    KSelectionOwner owner("WM_S0", c, x11RootWindow());
    owner.claim(true);

    createAtoms();

    setupEventFilters();

    // Check  whether another windowmanager is running
    const uint32_t maskValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
    ScopedCPointer<xcb_generic_error_t> redirectCheck(xcb_request_check(connection(),
                                                                        xcb_change_window_attributes_checked(connection(),
                                                                                                                rootWindow(),
                                                                                                                XCB_CW_EVENT_MASK,
                                                                                                                maskValues)));
    if (!redirectCheck.isNull()) {
        fputs(i18n("kwin_wayland: an X11 window manager is running on the X11 Display.\n").toLocal8Bit().constData(), stderr);
        ::exit(1);
    }

    if (!m_inputMethodServerToStart.isEmpty()) {
        int socket = dup(waylandServer()->createInputMethodConnection());
        if (socket >= 0) {
            QProcessEnvironment environment = m_environment;
            environment.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
            environment.remove("DISPLAY");
            environment.remove("WAYLAND_DISPLAY");
            QProcess *p = new Process(this);
            p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
            connect(p, finishedSignal, this,
                [this, p] {
                    if (waylandServer()) {
                        waylandServer()->destroyInputMethodConnection();
                    }
                    p->deleteLater();
                }
            );
            p->setProcessEnvironment(environment);
            p->start(m_inputMethodServerToStart);
            p->waitForStarted();
        }
    }

    m_environment.insert(QStringLiteral("DISPLAY"), QString::fromUtf8(qgetenv("DISPLAY")));
    // start session
    if (!m_sessionArgument.isEmpty()) {
        QProcess *p = new Process(this);
        p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        p->setProcessEnvironment(m_environment);
        auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
        connect(p, finishedSignal, this, &ApplicationWayland::quit);
        p->start(m_sessionArgument);
    }
    // start the applications passed to us as command line arguments
    if (!m_applicationsToStart.isEmpty()) {
        for (const QString &application: m_applicationsToStart) {
            // note: this will kill the started process when we exit
            // this is going to happen anyway as we are the wayland and X server the app connects to
            QProcess *p = new Process(this);
            p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            p->setProcessEnvironment(m_environment);
            p->start(application);
        }
    }

    createWorkspace();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort

    notifyKSplash();
}

void ApplicationWayland::createX11Connection()
{
    int screenNumber = 0;
    xcb_connection_t *c = nullptr;
    if (m_xcbConnectionFd == -1) {
        c = xcb_connect(nullptr, &screenNumber);
    } else {
        c = xcb_connect_to_fd(m_xcbConnectionFd, nullptr);
    }
    if (int error = xcb_connection_has_error(c)) {
        std::cerr << "FATAL ERROR: Creating connection to XServer failed: " << error << std::endl;
        exit(1);
        return;
    }
    setX11Connection(c);
    // we don't support X11 multi-head in Wayland
    setX11ScreenNumber(screenNumber);
    setX11RootWindow(defaultScreen()->root);
}

void ApplicationWayland::startXwaylandServer()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::startXwaylandServer);
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start Xwayland " << std::endl;
        exit(1);
        return;
    }
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        exit(1);
        return;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        exit(20);
        return;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        exit(1);
        return;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        exit(20);
        return;
    }

    m_xcbConnectionFd = sx[0];

    m_xwaylandProcess = new Process(kwinApp());
    m_xwaylandProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    QProcessEnvironment env = m_environment;
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    env.insert("EGL_PLATFORM", QByteArrayLiteral("DRM"));
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setArguments({QStringLiteral("-displayfd"),
                           QString::number(pipeFds[1]),
                           QStringLiteral("-rootless"),
                           QStringLiteral("-wm"),
                           QString::number(fd)});
    m_xwaylandFailConnection = connect(m_xwaylandProcess, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this,
        [] (QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                std::cerr << "FATAL ERROR: failed to start Xwayland" << std::endl;
            } else {
                std::cerr << "FATAL ERROR: Xwayland failed, going to exit now" << std::endl;
            }
            exit(1);
        }
    );
    const int xDisplayPipe = pipeFds[0];
    connect(m_xwaylandProcess, &QProcess::started, this,
        [this, xDisplayPipe] {
            QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
            QObject::connect(watcher, &QFutureWatcher<void>::finished, this, &ApplicationWayland::continueStartupWithX, Qt::QueuedConnection);
            QObject::connect(watcher, &QFutureWatcher<void>::finished, watcher, &QFutureWatcher<void>::deleteLater, Qt::QueuedConnection);
            watcher->setFuture(QtConcurrent::run(readDisplay, xDisplayPipe));
        }
    );
    m_xwaylandProcess->start();
    close(pipeFds[1]);
}

static void readDisplay(int pipe)
{
    QFile readPipe;
    if (!readPipe.open(pipe, QIODevice::ReadOnly)) {
        std::cerr << "FATAL ERROR failed to open pipe to start X Server" << std::endl;
        exit(1);
    }
    QByteArray displayNumber = readPipe.readLine();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() -1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData() << std::endl;

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipe);
}

static const QString s_waylandPlugin = QStringLiteral("KWinWaylandWaylandBackend");
static const QString s_x11Plugin = QStringLiteral("KWinWaylandX11Backend");
static const QString s_fbdevPlugin = QStringLiteral("KWinWaylandFbdevBackend");
#if HAVE_DRM
static const QString s_drmPlugin = QStringLiteral("KWinWaylandDrmBackend");
#endif
#if HAVE_LIBHYBRIS
static const QString s_hwcomposerPlugin = QStringLiteral("KWinWaylandHwcomposerBackend");
#endif
static const QString s_virtualPlugin = QStringLiteral("KWinWaylandVirtualBackend");

static QString automaticBackendSelection()
{
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        return s_waylandPlugin;
    }
    if (qEnvironmentVariableIsSet("DISPLAY")) {
        return s_x11Plugin;
    }
#if HAVE_LIBHYBRIS
    if (qEnvironmentVariableIsSet("ANDROID_ROOT")) {
        return s_hwcomposerPlugin;
    }
#endif
#if HAVE_DRM
    return s_drmPlugin;
#endif
    return s_fbdevPlugin;
}

static void disablePtrace()
{
#if HAVE_PR_SET_DUMPABLE
    // check whether we are running under a debugger
    const QFileInfo parent(QStringLiteral("/proc/%1/exe").arg(getppid()));
    if (parent.isSymLink() &&
            (parent.symLinkTarget().endsWith(QLatin1String("/gdb")) ||
             parent.symLinkTarget().endsWith(QLatin1String("/gdbserver")))) {
        // debugger, don't adjust
        return;
    }

    // disable ptrace in kwin_wayland
    prctl(PR_SET_DUMPABLE, 0);
#endif
#if HAVE_PROC_TRACE_CTL
    // FreeBSD's rudimentary procfs does not support /proc/<pid>/exe
    // We could use the P_TRACED flag of the process to find out
    // if the process is being debugged ond FreeBSD.
    int mode = PROC_TRACE_CTL_DISABLE;
    procctl(P_PID, getpid(), PROC_TRACE_CTL, &mode);
#endif

}

static void unsetDumpable(int sig)
{
#if HAVE_PR_SET_DUMPABLE
    prctl(PR_SET_DUMPABLE, 1);
#endif
    signal(sig, SIG_IGN);
    raise(sig);
    return;
}

} // namespace

int main(int argc, char * argv[])
{
    KWin::disablePtrace();
    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();

    if (signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWin::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);
    signal(SIGABRT, KWin::unsetDumpable);
    signal(SIGSEGV, KWin::unsetDumpable);
    signal(SIGPIPE, SIG_IGN);
    // ensure that no thread takes SIGUSR
    sigset_t userSignals;
    sigemptyset(&userSignals);
    sigaddset(&userSignals, SIGUSR1);
    sigaddset(&userSignals, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &userSignals, nullptr);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // enforce our internal qpa plugin, unfortunately command line switch has precedence
    setenv("QT_QPA_PLATFORM", "wayland-org.kde.kwin.qpa", true);

    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
    qputenv("QSG_RENDER_LOOP", "basic");
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    KWin::ApplicationWayland a(argc, argv);
    a.setupTranslator();
    // reset QT_QPA_PLATFORM to a sane value for any processes started from KWin
    setenv("QT_QPA_PLATFORM", "wayland", true);

    KWin::Application::createAboutData();

    const auto availablePlugins = KPluginLoader::findPlugins(QStringLiteral("org.kde.kwin.waylandbackends"));
    auto hasPlugin = [&availablePlugins] (const QString &name) {
        return std::any_of(availablePlugins.begin(), availablePlugins.end(),
            [name] (const KPluginMetaData &plugin) {
                return plugin.pluginId() == name;
            }
        );
    };
    const bool hasWindowedOption = hasPlugin(KWin::s_x11Plugin) || hasPlugin(KWin::s_waylandPlugin);
    const bool hasSizeOption = hasPlugin(KWin::s_x11Plugin) || hasPlugin(KWin::s_virtualPlugin);
    const bool hasOutputCountOption = hasPlugin(KWin::s_x11Plugin);
    const bool hasX11Option = hasPlugin(KWin::s_x11Plugin);
    const bool hasVirtualOption = hasPlugin(KWin::s_virtualPlugin);
    const bool hasWaylandOption = hasPlugin(KWin::s_waylandPlugin);
    const bool hasFramebufferOption = hasPlugin(KWin::s_fbdevPlugin);
#if HAVE_DRM
    const bool hasDrmOption = hasPlugin(KWin::s_drmPlugin);
#endif
#if HAVE_LIBHYBRIS
    const bool hasHwcomposerOption = hasPlugin(KWin::s_hwcomposerPlugin);
#endif

    QCommandLineOption xwaylandOption(QStringLiteral("xwayland"),
                                      i18n("Start a rootless Xwayland server."));
    QCommandLineOption waylandSocketOption(QStringList{QStringLiteral("s"), QStringLiteral("socket")},
                                           i18n("Name of the Wayland socket to listen on. If not set \"wayland-0\" is used."),
                                           QStringLiteral("socket"));
    QCommandLineOption windowedOption(QStringLiteral("windowed"),
                                      i18n("Use a nested compositor in windowed mode."));
    QCommandLineOption framebufferOption(QStringLiteral("framebuffer"),
                                         i18n("Render to framebuffer."));
    QCommandLineOption framebufferDeviceOption(QStringLiteral("fb-device"),
                                               i18n("The framebuffer device to render to."),
                                               QStringLiteral("fbdev"));
    framebufferDeviceOption.setDefaultValue(QStringLiteral("/dev/fb0"));
    QCommandLineOption x11DisplayOption(QStringLiteral("x11-display"),
                                        i18n("The X11 Display to use in windowed mode on platform X11."),
                                        QStringLiteral("display"));
    QCommandLineOption waylandDisplayOption(QStringLiteral("wayland-display"),
                                            i18n("The Wayland Display to use in windowed mode on platform Wayland."),
                                            QStringLiteral("display"));
    QCommandLineOption virtualFbOption(QStringLiteral("virtual"), i18n("Render to a virtual framebuffer."));
    QCommandLineOption widthOption(QStringLiteral("width"),
                                   i18n("The width for windowed mode. Default width is 1024."),
                                   QStringLiteral("width"));
    widthOption.setDefaultValue(QString::number(1024));
    QCommandLineOption heightOption(QStringLiteral("height"),
                                    i18n("The height for windowed mode. Default height is 768."),
                                    QStringLiteral("height"));
    heightOption.setDefaultValue(QString::number(768));

    QCommandLineOption scaleOption(QStringLiteral("scale"),
                                    i18n("The scale for windowed mode. Default value is 1."),
                                    QStringLiteral("scale"));
    scaleOption.setDefaultValue(QString::number(1));

    QCommandLineOption outputCountOption(QStringLiteral("output-count"),
                                    i18n("The number of windows to open as outputs in windowed mode. Default value is 1"),
                                    QStringLiteral("height"));
    outputCountOption.setDefaultValue(QString::number(1));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(xwaylandOption);
    parser.addOption(waylandSocketOption);
    if (hasWindowedOption) {
        parser.addOption(windowedOption);
    }
    if (hasX11Option) {
        parser.addOption(x11DisplayOption);
    }
    if (hasWaylandOption) {
        parser.addOption(waylandDisplayOption);
    }
    if (hasFramebufferOption) {
        parser.addOption(framebufferOption);
        parser.addOption(framebufferDeviceOption);
    }
    if (hasVirtualOption) {
        parser.addOption(virtualFbOption);
    }
    if (hasSizeOption) {
        parser.addOption(widthOption);
        parser.addOption(heightOption);
        parser.addOption(scaleOption);
    }
    if (hasOutputCountOption) {
        parser.addOption(outputCountOption);
    }
#if HAVE_LIBHYBRIS
    QCommandLineOption hwcomposerOption(QStringLiteral("hwcomposer"), i18n("Use libhybris hwcomposer"));
    if (hasHwcomposerOption) {
        parser.addOption(hwcomposerOption);
    }
#endif
#if HAVE_INPUT
    QCommandLineOption libinputOption(QStringLiteral("libinput"),
                                      i18n("Enable libinput support for input events processing. Note: never use in a nested session."));
    parser.addOption(libinputOption);
#endif
#if HAVE_DRM
    QCommandLineOption drmOption(QStringLiteral("drm"), i18n("Render through drm node."));
    if (hasDrmOption) {
        parser.addOption(drmOption);
    }
#endif

    QCommandLineOption inputMethodOption(QStringLiteral("inputmethod"),
                                         i18n("Input method that KWin starts."),
                                         QStringLiteral("path/to/imserver"));
    parser.addOption(inputMethodOption);

    QCommandLineOption listBackendsOption(QStringLiteral("list-backends"),
                                           i18n("List all available backends and quit."));
    parser.addOption(listBackendsOption);

    QCommandLineOption screenLockerOption(QStringLiteral("lockscreen"),
                                          i18n("Starts the session in locked mode."));
    parser.addOption(screenLockerOption);

    QCommandLineOption exitWithSessionOption(QStringLiteral("exit-with-session"),
                                             i18n("Exit after the session application, which is started by KWin, closed."),
                                             QStringLiteral("/path/to/session"));
    parser.addOption(exitWithSessionOption);

#ifdef KWIN_BUILD_ACTIVITIES
    QCommandLineOption noActivitiesOption(QStringLiteral("no-kactivities"),
                                        i18n("Disable KActivities integration."));
    parser.addOption(noActivitiesOption);
#endif

    parser.addPositionalArgument(QStringLiteral("applications"),
                                 i18n("Applications to start once Wayland and Xwayland server are started"),
                                 QStringLiteral("[/path/to/application...]"));

    parser.process(a);
    a.processCommandLine(&parser);

#ifdef KWIN_BUILD_ACTIVITIES
    if (parser.isSet(noActivitiesOption)) {
        a.setUseKActivities(false);
    }
#endif

    if (parser.isSet(listBackendsOption)) {
        for (const auto &plugin: availablePlugins) {
            std::cout << std::setw(40) << std::left << qPrintable(plugin.name()) << qPrintable(plugin.description()) << std::endl;
        }
        return 0;
    }

    if (parser.isSet(exitWithSessionOption)) {
        a.setSessionArgument(parser.value(exitWithSessionOption));
    }

#if HAVE_INPUT
    KWin::Application::setUseLibinput(parser.isSet(libinputOption));
#endif

    QString pluginName;
    QSize initialWindowSize;
    QByteArray deviceIdentifier;
    int outputCount = 1;
    qreal outputScale = 1;

#if HAVE_DRM
    if (hasDrmOption && parser.isSet(drmOption)) {
        pluginName = KWin::s_drmPlugin;
    }
#endif

    if (hasSizeOption) {
        bool ok = false;
        const int width = parser.value(widthOption).toInt(&ok);
        if (!ok) {
            std::cerr << "FATAL ERROR incorrect value for width" << std::endl;
            return 1;
        }
        const int height = parser.value(heightOption).toInt(&ok);
        if (!ok) {
            std::cerr << "FATAL ERROR incorrect value for height" << std::endl;
            return 1;
        }
        const qreal scale = parser.value(scaleOption).toDouble(&ok);
        if (!ok || scale < 1) {
            std::cerr << "FATAL ERROR incorrect value for scale" << std::endl;
            return 1;
        }

        outputScale = scale;
        initialWindowSize = QSize(width, height);
    }

    if (hasOutputCountOption) {
        bool ok = false;
        const int count = parser.value(outputCountOption).toInt(&ok);
        if (ok) {
            outputCount = qMax(1, count);
        }
    }

    if (hasWindowedOption && parser.isSet(windowedOption)) {
        if (hasX11Option && parser.isSet(x11DisplayOption)) {
            deviceIdentifier = parser.value(x11DisplayOption).toUtf8();
        } else if (!(hasWaylandOption && parser.isSet(waylandDisplayOption))) {
            deviceIdentifier = qgetenv("DISPLAY");
        }
        if (!deviceIdentifier.isEmpty()) {
            pluginName = KWin::s_x11Plugin;
        } else if (hasWaylandOption) {
            if (parser.isSet(waylandDisplayOption)) {
                deviceIdentifier = parser.value(waylandDisplayOption).toUtf8();
            } else {
                deviceIdentifier = qgetenv("WAYLAND_DISPLAY");
            }
            if (!deviceIdentifier.isEmpty()) {
                pluginName = KWin::s_waylandPlugin;
            }
        }
    }
    if (hasFramebufferOption && parser.isSet(framebufferOption)) {
        pluginName = KWin::s_fbdevPlugin;
        deviceIdentifier = parser.value(framebufferDeviceOption).toUtf8();
    }
#if HAVE_LIBHYBRIS
    if (hasHwcomposerOption && parser.isSet(hwcomposerOption)) {
        pluginName = KWin::s_hwcomposerPlugin;
    }
#endif
    if (hasVirtualOption && parser.isSet(virtualFbOption)) {
        pluginName = KWin::s_virtualPlugin;
    }

    if (pluginName.isEmpty()) {
        std::cerr << "No backend specified through command line argument, trying auto resolution" << std::endl;
        pluginName = KWin::automaticBackendSelection();
    }

    auto pluginIt = std::find_if(availablePlugins.begin(), availablePlugins.end(),
        [&pluginName] (const KPluginMetaData &plugin) {
            return plugin.pluginId() == pluginName;
        }
    );
    if (pluginIt == availablePlugins.end()) {
        std::cerr << "FATAL ERROR: could not find a backend" << std::endl;
        return 1;
    }

    // TODO: create backend without having the server running
    KWin::WaylandServer *server = KWin::WaylandServer::create(&a);

    KWin::WaylandServer::InitalizationFlags flags;
    if (parser.isSet(screenLockerOption)) {
        flags = KWin::WaylandServer::InitalizationFlag::LockScreen;
    }
    if (!server->init(parser.value(waylandSocketOption).toUtf8(), flags)) {
        std::cerr << "FATAL ERROR: could not create Wayland server" << std::endl;
        return 1;
    }

    a.initPlatform(*pluginIt);
    if (!a.platform()) {
        std::cerr << "FATAL ERROR: could not instantiate a backend" << std::endl;
        return 1;
    }
    if (!deviceIdentifier.isEmpty()) {
        a.platform()->setDeviceIdentifier(deviceIdentifier);
    }
    if (initialWindowSize.isValid()) {
        a.platform()->setInitialWindowSize(initialWindowSize);
    }
    a.platform()->setInitialOutputScale(outputScale);
    a.platform()->setInitialOutputCount(outputCount);

    QObject::connect(&a, &KWin::Application::workspaceCreated, server, &KWin::WaylandServer::initWorkspace);
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), server->display()->socketName());
    a.setProcessStartupEnvironment(environment);
    a.setStartXwayland(parser.isSet(xwaylandOption));
    a.setApplicationsToStart(parser.positionalArguments());
    a.setInputMethodServerToStart(parser.value(inputMethodOption));
    a.start();

    return a.exec();
}
