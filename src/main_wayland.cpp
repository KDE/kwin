/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main_wayland.h"

#include <config-kwin.h>

#include "backends/drm/drm_backend.h"
#include "backends/virtual/virtual_backend.h"
#include "backends/wayland/wayland_backend.h"
#include "backends/x11/windowed/x11_windowed_backend.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "core/session.h"
#include "effects.h"
#include "inputmethod.h"
#include "tabletmodemanager.h"
#include "utils/realtime.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xwayland/xwayland.h"
#include "xwayland/xwaylandlauncher.h"

// KDE
#include <KCrash>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KShell>
#include <KSignalHandler>

// Qt
#include <QCommandLineParser>
#include <QDBusInterface>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QWindow>
#include <qplatformdefs.h>

#include <sched.h>
#include <sys/resource.h>

#include <iomanip>
#include <iostream>

Q_IMPORT_PLUGIN(KWinIntegrationPlugin)
Q_IMPORT_PLUGIN(KGlobalAccelImpl)
Q_IMPORT_PLUGIN(KWindowSystemKWinPlugin)
Q_IMPORT_PLUGIN(KWinIdleTimePoller)
#if PipeWire_FOUND
Q_IMPORT_PLUGIN(ScreencastManagerFactory)
#endif

namespace KWin
{

static rlimit originalNofileLimit = {
    .rlim_cur = 0,
    .rlim_max = 0,
};

static bool bumpNofileLimit()
{
    if (getrlimit(RLIMIT_NOFILE, &originalNofileLimit) == -1) {
        std::cerr << "Failed to bump RLIMIT_NOFILE limit, getrlimit() failed: " << strerror(errno) << std::endl;
        return false;
    }

    rlimit limit = originalNofileLimit;
    limit.rlim_cur = limit.rlim_max;

    if (setrlimit(RLIMIT_NOFILE, &limit) == -1) {
        std::cerr << "Failed to bump RLIMIT_NOFILE limit, setrlimit() failed: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

static void restoreNofileLimit()
{
    if (setrlimit(RLIMIT_NOFILE, &originalNofileLimit) == -1) {
        std::cerr << "Failed to restore RLIMIT_NOFILE limit, legacy apps might be broken" << std::endl;
    }
}

void disableDrKonqi()
{
    KCrash::setDrKonqiEnabled(false);
}
// run immediately, before Q_CORE_STARTUP functions
// that would enable drkonqi
Q_CONSTRUCTOR_FUNCTION(disableDrKonqi)

//************************************
// ApplicationWayland
//************************************

ApplicationWayland::ApplicationWayland(int &argc, char **argv)
    : Application(OperationModeWaylandOnly, argc, argv)
{
}

ApplicationWayland::~ApplicationWayland()
{
    setTerminating();
    if (!waylandServer()) {
        return;
    }

    destroyPlugins();

    // need to unload all effects prior to destroying X connection as they might do X calls
    if (effects) {
        static_cast<EffectsHandlerImpl *>(effects)->unloadAllEffects();
    }
    m_xwayland.reset();
    destroyColorManager();
    destroyWorkspace();

    destroyInputMethod();
    destroyCompositor();
    destroyInput();
}

void ApplicationWayland::performStartup()
{
    if (m_startXWayland) {
        setOperationMode(OperationModeXwayland);
        setXwaylandScale(config()->group("Xwayland").readEntry("Scale", 1.0));
    }
    // first load options - done internally by a different thread
    createOptions();

    if (!outputBackend()->initialize()) {
        std::exit(1);
    }

    createInput();
    createInputMethod();
    createTabletModeManager();

    WaylandCompositor::create();

    connect(Compositor::self(), &Compositor::sceneCreated, outputBackend(), &OutputBackend::sceneInitialized);
    connect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithScene);
}

void ApplicationWayland::continueStartupWithScene()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithScene);

    // Note that we start accepting client connections after creating the Workspace.
    createWorkspace();
    createColorManager();
    createPlugins();

    if (!waylandServer()->start()) {
        qFatal("Failed to initialze the Wayland server, exiting now");
    }

    if (operationMode() == OperationModeWaylandOnly) {
        finalizeStartup();
        return;
    }

    m_xwayland = std::make_unique<Xwl::Xwayland>(this);
    m_xwayland->xwaylandLauncher()->setListenFDs(m_xwaylandListenFds);
    m_xwayland->xwaylandLauncher()->setDisplayName(m_xwaylandDisplay);
    m_xwayland->xwaylandLauncher()->setXauthority(m_xwaylandXauthority);
    connect(m_xwayland.get(), &Xwl::Xwayland::errorOccurred, this, &ApplicationWayland::finalizeStartup);
    connect(m_xwayland.get(), &Xwl::Xwayland::started, this, &ApplicationWayland::finalizeStartup);
    m_xwayland->start();
}

void ApplicationWayland::finalizeStartup()
{
    if (m_xwayland) {
        disconnect(m_xwayland.get(), &Xwl::Xwayland::errorOccurred, this, &ApplicationWayland::finalizeStartup);
        disconnect(m_xwayland.get(), &Xwl::Xwayland::started, this, &ApplicationWayland::finalizeStartup);
    }
    startSession();
    notifyStarted();
}

void ApplicationWayland::refreshSettings(const KConfigGroup &group, const QByteArrayList &names)
{
    if (group.name() == "Wayland" && names.contains("InputMethod")) {
        KDesktopFile file(group.readPathEntry("InputMethod", QString()));
        kwinApp()->inputMethod()->setInputMethodCommand(file.desktopGroup().readEntry("Exec", QString()));
    }

    if (m_startXWayland && group.name() == "Xwayland" && names.contains("Scale")) {
        setXwaylandScale(group.readEntry("Scale", 1.0));
    }
}

void ApplicationWayland::startSession()
{
    KSharedConfig::Ptr kwinSettings = kwinApp()->config();
    m_settingsWatcher = KConfigWatcher::create(kwinSettings);
    connect(m_settingsWatcher.data(), &KConfigWatcher::configChanged, this, &ApplicationWayland::refreshSettings);

    if (!m_inputMethodServerToStart.isEmpty()) {
        kwinApp()->inputMethod()->setInputMethodCommand(m_inputMethodServerToStart);
    } else {
        refreshSettings(kwinSettings->group("Wayland"), {"InputMethod"});
    }

    // start session
    if (!m_sessionArgument.isEmpty()) {
        QStringList arguments = KShell::splitArgs(m_sessionArgument);
        if (!arguments.isEmpty()) {
            QString program = arguments.takeFirst();
            QProcess *p = new QProcess(this);
            p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            p->setProcessEnvironment(processStartupEnvironment());
            connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [p](int code, QProcess::ExitStatus status) {
                p->deleteLater();
                if (status == QProcess::CrashExit) {
                    qWarning() << "Session process has crashed";
                    QCoreApplication::exit(-1);
                    return;
                }

                if (code) {
                    qWarning() << "Session process exited with code" << code;
                }

                QCoreApplication::exit(code);
            });
            p->setProgram(program);
            p->setArguments(arguments);
            p->start();
        } else {
            qWarning("Failed to launch the session process: %s is an invalid command",
                     qPrintable(m_sessionArgument));
        }
    }
    // start the applications passed to us as command line arguments
    if (!m_applicationsToStart.isEmpty()) {
        for (const QString &application : std::as_const(m_applicationsToStart)) {
            QStringList arguments = KShell::splitArgs(application);
            if (arguments.isEmpty()) {
                qWarning("Failed to launch application: %s is an invalid command",
                         qPrintable(application));
                continue;
            }
            QString program = arguments.takeFirst();
            // note: this will kill the started process when we exit
            // this is going to happen anyway as we are the wayland and X server the app connects to
            QProcess *p = new QProcess(this);
            p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            p->setProcessEnvironment(processStartupEnvironment());
            p->setProgram(program);
            p->setArguments(arguments);
            p->startDetached();
            p->deleteLater();
        }
    }
}

XwaylandInterface *ApplicationWayland::xwayland() const
{
    return m_xwayland.get();
}

} // namespace

int main(int argc, char *argv[])
{
    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();
    KWin::gainRealTime();

    signal(SIGPIPE, SIG_IGN);

    // It's easy to exceed the file descriptor limit because many things are backed using fds
    // nowadays, e.g. dmabufs, shm buffers, etc. Bump the RLIMIT_NOFILE limit to handle that.
    // Some apps may still use select(), so we reset the limit to its original value in fork().
    if (KWin::bumpNofileLimit()) {
        pthread_atfork(nullptr, nullptr, KWin::restoreNofileLimit);
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // enforce our internal qpa plugin, unfortunately command line switch has precedence
    setenv("QT_QPA_PLATFORM", "wayland-org.kde.kwin.qpa", true);

    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qputenv("QSG_RENDER_LOOP", "basic");
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    KWin::ApplicationWayland a(argc, argv);
    a.setupTranslator();
    // reset QT_QPA_PLATFORM so we don't propagate it to our children (e.g. apps launched from the overview effect)
    qunsetenv("QT_QPA_PLATFORM");

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    KSignalHandler::self()->watchSignal(SIGHUP);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived,
                     &a, &QCoreApplication::exit);

    KWin::Application::createAboutData();

    QCommandLineOption xwaylandOption(QStringLiteral("xwayland"),
                                      i18n("Start a rootless Xwayland server."));
    QCommandLineOption waylandSocketOption(QStringList{QStringLiteral("s"), QStringLiteral("socket")},
                                           i18n("Name of the Wayland socket to listen on. If not set \"wayland-0\" is used."),
                                           QStringLiteral("socket"));
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
                                         QStringLiteral("count"));
    outputCountOption.setDefaultValue(QString::number(1));

    QCommandLineOption waylandSocketFdOption(QStringLiteral("wayland-fd"),
                                             i18n("Wayland socket to use for incoming connections. This can be combined with --socket to name the socket"),
                                             QStringLiteral("wayland-fd"));

    QCommandLineOption xwaylandListenFdOption(QStringLiteral("xwayland-fd"),
                                              i18n("XWayland socket to use for Xwayland's incoming connections. This can be set multiple times"),
                                              QStringLiteral("xwayland-fds"));

    QCommandLineOption xwaylandDisplayOption(QStringLiteral("xwayland-display"),
                                             i18n("Name of the xwayland display that has been pre-set up"),
                                             "xwayland-display");

    QCommandLineOption xwaylandXAuthorityOption(QStringLiteral("xwayland-xauthority"),
                                                i18n("Name of the xauthority file "),
                                                "xwayland-xauthority");

    QCommandLineOption replaceOption(QStringLiteral("replace"),
                                     i18n("Exits this instance so it can be restarted by kwin_wayland_wrapper."));

    QCommandLineOption drmOption(QStringLiteral("drm"), i18n("Render through drm node."));
    QCommandLineOption locale1Option(QStringLiteral("locale1"), i18n("Extract locale information from locale1 rather than the user's configuration"));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(xwaylandOption);
    parser.addOption(waylandSocketOption);
    parser.addOption(waylandSocketFdOption);
    parser.addOption(xwaylandListenFdOption);
    parser.addOption(xwaylandDisplayOption);
    parser.addOption(xwaylandXAuthorityOption);
    parser.addOption(replaceOption);
    parser.addOption(x11DisplayOption);
    parser.addOption(waylandDisplayOption);
    parser.addOption(virtualFbOption);
    parser.addOption(widthOption);
    parser.addOption(heightOption);
    parser.addOption(scaleOption);
    parser.addOption(outputCountOption);
    parser.addOption(drmOption);
    parser.addOption(locale1Option);

    QCommandLineOption inputMethodOption(QStringLiteral("inputmethod"),
                                         i18n("Input method that KWin starts."),
                                         QStringLiteral("path/to/imserver"));
    parser.addOption(inputMethodOption);

#if KWIN_BUILD_SCREENLOCKER
    QCommandLineOption screenLockerOption(QStringLiteral("lockscreen"),
                                          i18n("Starts the session in locked mode."));
    parser.addOption(screenLockerOption);

    QCommandLineOption noScreenLockerOption(QStringLiteral("no-lockscreen"),
                                            i18n("Starts the session without lock screen support."));
    parser.addOption(noScreenLockerOption);
#endif

    QCommandLineOption noGlobalShortcutsOption(QStringLiteral("no-global-shortcuts"),
                                               i18n("Starts the session without global shortcuts support."));
    parser.addOption(noGlobalShortcutsOption);

#if KWIN_BUILD_ACTIVITIES
    QCommandLineOption noActivitiesOption(QStringLiteral("no-kactivities"),
                                          i18n("Disable KActivities integration."));
    parser.addOption(noActivitiesOption);
#endif

    QCommandLineOption exitWithSessionOption(QStringLiteral("exit-with-session"),
                                             i18n("Exit after the session application, which is started by KWin, closed."),
                                             QStringLiteral("/path/to/session"));
    parser.addOption(exitWithSessionOption);

    parser.addPositionalArgument(QStringLiteral("applications"),
                                 i18n("Applications to start once Wayland and Xwayland server are started"),
                                 QStringLiteral("[/path/to/application...]"));

    parser.process(a);
    a.processCommandLine(&parser);

#if KWIN_BUILD_ACTIVITIES
    if (parser.isSet(noActivitiesOption)) {
        a.setUseKActivities(false);
    }
#endif

    if (parser.isSet(replaceOption)) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"), QStringLiteral("replace"));
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
        return 0;
    }

    if (parser.isSet(exitWithSessionOption)) {
        a.setSessionArgument(parser.value(exitWithSessionOption));
    }

    enum class BackendType {
        Kms,
        X11,
        Wayland,
        Virtual,
    };

    BackendType backendType;
    QString pluginName;
    QSize initialWindowSize;
    int outputCount = 1;
    qreal outputScale = 1;

    // Decide what backend to use.
    if (parser.isSet(drmOption)) {
        backendType = BackendType::Kms;
    } else if (parser.isSet(x11DisplayOption)) {
        backendType = BackendType::X11;
    } else if (parser.isSet(waylandDisplayOption)) {
        backendType = BackendType::Wayland;
    } else if (parser.isSet(virtualFbOption)) {
        backendType = BackendType::Virtual;
    } else {
        if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
            qWarning("No backend specified, automatically choosing Wayland because WAYLAND_DISPLAY is set");
            backendType = BackendType::Wayland;
        } else if (qEnvironmentVariableIsSet("DISPLAY")) {
            qWarning("No backend specified, automatically choosing X11 because DISPLAY is set");
            backendType = BackendType::X11;
        } else {
            qWarning("No backend specified, automatically choosing drm");
            backendType = BackendType::Kms;
        }
    }

    if (parser.isSet(locale1Option)) {
        a.setFollowLocale1(true);
    }

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
    if (!ok || scale <= 0) {
        std::cerr << "FATAL ERROR incorrect value for scale" << std::endl;
        return 1;
    }

    outputScale = scale;
    initialWindowSize = QSize(width, height);

    const int count = parser.value(outputCountOption).toInt(&ok);
    if (ok) {
        outputCount = std::max(1, count);
    }

    // TODO: create backend without having the server running
    KWin::WaylandServer *server = KWin::WaylandServer::create(&a);

    KWin::WaylandServer::InitializationFlags flags;
#if KWIN_BUILD_SCREENLOCKER
    if (parser.isSet(screenLockerOption)) {
        flags = KWin::WaylandServer::InitializationFlag::LockScreen;
    } else if (parser.isSet(noScreenLockerOption)) {
        flags = KWin::WaylandServer::InitializationFlag::NoLockScreenIntegration;
    }
#endif
    if (parser.isSet(noGlobalShortcutsOption)) {
        flags |= KWin::WaylandServer::InitializationFlag::NoGlobalShortcuts;
    }

    const QString socketName = parser.value(waylandSocketOption);
    if (parser.isSet(waylandSocketFdOption)) {
        bool ok;
        int fd = parser.value(waylandSocketFdOption).toInt(&ok);
        if (ok) {
            // make sure we don't leak this FD to children
            fcntl(fd, F_SETFD, FD_CLOEXEC);
            server->display()->addSocketFileDescriptor(fd, socketName);
        } else {
            std::cerr << "FATAL ERROR: could not parse socket FD" << std::endl;
            return 1;
        }
    } else {
        // socketName empty is fine here, addSocketName will automatically pick one
        if (!server->display()->addSocketName(socketName)) {
            std::cerr << "FATAL ERROR: could not add wayland socket " << qPrintable(socketName) << std::endl;
            return 1;
        }
    }

    if (!server->init(flags)) {
        std::cerr << "FATAL ERROR: could not create Wayland server" << std::endl;
        return 1;
    }

    switch (backendType) {
    case BackendType::Kms:
        a.setSession(KWin::Session::create());
        if (!a.session()) {
            std::cerr << "FATAl ERROR: could not acquire a session" << std::endl;
            return 1;
        }
        a.setOutputBackend(std::make_unique<KWin::DrmBackend>(a.session()));
        break;
    case BackendType::Virtual: {
        auto outputBackend = std::make_unique<KWin::VirtualBackend>();
        for (int i = 0; i < outputCount; ++i) {
            outputBackend->addOutput(initialWindowSize, outputScale);
        }
        a.setSession(KWin::Session::create(KWin::Session::Type::Noop));
        a.setOutputBackend(std::move(outputBackend));
        break;
    }
    case BackendType::X11: {
        QString display = parser.value(x11DisplayOption);
        if (display.isEmpty()) {
            display = qgetenv("DISPLAY");
        }
        a.setSession(KWin::Session::create(KWin::Session::Type::Noop));
        a.setOutputBackend(std::make_unique<KWin::X11WindowedBackend>(KWin::X11WindowedBackendOptions{
            .display = display,
            .outputCount = outputCount,
            .outputScale = outputScale,
            .outputSize = initialWindowSize,
        }));
        break;
    }
    case BackendType::Wayland: {
        QString socketName = parser.value(waylandDisplayOption);
        if (socketName.isEmpty()) {
            socketName = qgetenv("WAYLAND_DISPLAY");
        }
        a.setSession(KWin::Session::create(KWin::Session::Type::Noop));
        a.setOutputBackend(std::make_unique<KWin::Wayland::WaylandBackend>(KWin::Wayland::WaylandBackendOptions{
            .socketName = socketName,
            .outputCount = outputCount,
            .outputScale = outputScale,
            .outputSize = initialWindowSize,
        }));
        break;
    }
    }

    QObject::connect(&a, &KWin::Application::workspaceCreated, server, &KWin::WaylandServer::initWorkspace);
    if (!server->socketName().isEmpty()) {
        environment.insert(QStringLiteral("WAYLAND_DISPLAY"), server->socketName());
        qputenv("WAYLAND_DISPLAY", server->socketName().toUtf8());
    }
    a.setProcessStartupEnvironment(environment);

    if (parser.isSet(xwaylandOption)) {
        a.setStartXwayland(true);

        if (parser.isSet(xwaylandListenFdOption)) {
            const QStringList fdStrings = parser.values(xwaylandListenFdOption);
            for (const QString &fdString : fdStrings) {
                bool ok;
                int fd = fdString.toInt(&ok);
                if (ok) {
                    // make sure we don't leak this FD to children
                    fcntl(fd, F_SETFD, FD_CLOEXEC);
                    a.addXwaylandSocketFileDescriptor(fd);
                }
            }
            if (parser.isSet(xwaylandDisplayOption)) {
                a.setXwaylandDisplay(parser.value(xwaylandDisplayOption));
            } else {
                std::cerr << "Using xwayland-fd without xwayland-display is undefined" << std::endl;
                return 1;
            }
            if (parser.isSet(xwaylandXAuthorityOption)) {
                a.setXwaylandXauthority(parser.value(xwaylandXAuthorityOption));
            }
        }
    }

    a.setApplicationsToStart(parser.positionalArguments());
    a.setInputMethodServerToStart(parser.value(inputMethodOption));
    a.start();

    return a.exec();
}
