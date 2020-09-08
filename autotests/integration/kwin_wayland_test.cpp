/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "../../platform.h"
#include "../../composite.h"
#include "../../effects.h"
#include "../../wayland_server.h"
#include "../../workspace.h"
#include "../../xcbutils.h"
#include "../../xwl/xwayland.h"
#include "../../virtualkeyboard.h"

#include <KPluginMetaData>

#include <QAbstractEventDispatcher>
#include <QPluginLoader>
#include <QSocketNotifier>
#include <QStyle>
#include <QThread>
#include <QtConcurrentRun>

// system
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

namespace KWin
{

WaylandTestApplication::WaylandTestApplication(OperationMode mode, int &argc, char **argv)
    : ApplicationWaylandAbstract(mode, argc, argv)
{
    QStandardPaths::setTestModeEnabled(true);
    // TODO: add a test move to kglobalaccel instead?
    QFile{QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kglobalshortcutsrc"))}.remove();
    QIcon::setThemeName(QStringLiteral("breeze"));
#ifdef KWIN_BUILD_ACTIVITIES
    setUseKActivities(false);
#endif
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));
    qunsetenv("XKB_DEFAULT_RULES");
    qunsetenv("XKB_DEFAULT_MODEL");
    qunsetenv("XKB_DEFAULT_LAYOUT");
    qunsetenv("XKB_DEFAULT_VARIANT");
    qunsetenv("XKB_DEFAULT_OPTIONS");

    const auto ownPath = libraryPaths().last();
    removeLibraryPath(ownPath);
    addLibraryPath(ownPath);

    const auto plugins = KPluginLoader::findPluginsById(QStringLiteral("org.kde.kwin.waylandbackends"), "KWinWaylandVirtualBackend");
    if (plugins.empty()) {
        quit();
        return;
    }
    initPlatform(plugins.first());
    WaylandServer::create(this);
    setProcessStartupEnvironment(QProcessEnvironment::systemEnvironment());
}

WaylandTestApplication::~WaylandTestApplication()
{
    setTerminating();
    kwinApp()->platform()->setOutputsEnabled(false);
    // need to unload all effects prior to destroying X connection as they might do X calls
    // also before destroy Workspace, as effects might call into Workspace
    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->unloadAllEffects();
    }
    delete m_xwayland;
    m_xwayland = nullptr;
    destroyWorkspace();
    waylandServer()->dispatch();
    if (QStyle *s = style()) {
        s->unpolish(this);
    }
    waylandServer()->terminateClientConnections();
    destroyCompositor();
}

void WaylandTestApplication::performStartup()
{
    if (!m_inputMethodServerToStart.isEmpty()) {
        VirtualKeyboard::create();
        if (m_inputMethodServerToStart != QStringLiteral("internal")) {
            int socket = dup(waylandServer()->createInputMethodConnection());
            if (socket >= 0) {
                QProcessEnvironment environment = processStartupEnvironment();
                environment.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
                environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
                environment.remove("DISPLAY");
                environment.remove("WAYLAND_DISPLAY");
                QProcess *p = new Process(this);
                p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
                connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                    [p] {
                        if (waylandServer()) {
                            waylandServer()->destroyInputMethodConnection();
                        }
                        p->deleteLater();
                    }
                );
                p->setProcessEnvironment(environment);
                p->setProgram(m_inputMethodServerToStart);
            //  p->setArguments(arguments);
                p->start();
                connect(waylandServer(), &WaylandServer::terminatingInternalClientConnection, p, [p] {
                    p->kill();
                    p->waitForFinished();
                });
            }
        }
    }

    // first load options - done internally by a different thread
    createOptions();
    waylandServer()->createInternalConnection();

    // try creating the Wayland Backend
    createInput();
    createBackend();
}

void WaylandTestApplication::createBackend()
{
    Platform *platform = kwinApp()->platform();
    connect(platform, &Platform::screensQueried, this, &WaylandTestApplication::continueStartupWithScreens);
    connect(platform, &Platform::initFailed, this,
        [] () {
            std::cerr <<  "FATAL ERROR: backend failed to initialize, exiting now" << std::endl;
            ::exit(1);
        }
    );
    platform->init();
}

void WaylandTestApplication::continueStartupWithScreens()
{
    disconnect(kwinApp()->platform(), &Platform::screensQueried, this, &WaylandTestApplication::continueStartupWithScreens);
    createScreens();
    WaylandCompositor::create();
    connect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::continueStartupWithScene);
}

void WaylandTestApplication::finalizeStartup()
{
    if (m_xwayland) {
        disconnect(m_xwayland, &Xwl::Xwayland::errorOccurred, this, &WaylandTestApplication::finalizeStartup);
        disconnect(m_xwayland, &Xwl::Xwayland::started, this, &WaylandTestApplication::finalizeStartup);
    }
    notifyStarted();
}

void WaylandTestApplication::continueStartupWithScene()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::continueStartupWithScene);

    createWorkspace();

    if (!waylandServer()->start()) {
        qFatal("Failed to initialize the Wayland server, exiting now");
    }

    if (operationMode() == OperationModeWaylandOnly) {
        finalizeStartup();
        return;
    }

    m_xwayland = new Xwl::Xwayland(this);
    connect(m_xwayland, &Xwl::Xwayland::errorOccurred, this, &WaylandTestApplication::finalizeStartup);
    connect(m_xwayland, &Xwl::Xwayland::started, this, &WaylandTestApplication::finalizeStartup);
    m_xwayland->start();
}

}
