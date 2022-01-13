/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "composite.h"
#include "effects.h"
#include "inputmethod.h"
#include "platform.h"
#include "pluginmanager.h"
#include "wayland_server.h"
#include "workspace.h"
#include "utils/xcbutils.h"
#include "xwl/xwayland.h"

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

Q_IMPORT_PLUGIN(KWinIntegrationPlugin)
Q_IMPORT_PLUGIN(KGlobalAccelImpl)
Q_IMPORT_PLUGIN(KWindowSystemKWinPlugin)
Q_IMPORT_PLUGIN(KWinIdleTimePoller)

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
    qputenv("XDG_CURRENT_DESKTOP", QByteArrayLiteral("KDE"));
    qunsetenv("XKB_DEFAULT_RULES");
    qunsetenv("XKB_DEFAULT_MODEL");
    qunsetenv("XKB_DEFAULT_LAYOUT");
    qunsetenv("XKB_DEFAULT_VARIANT");
    qunsetenv("XKB_DEFAULT_OPTIONS");

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup windowsGroup = config->group("Windows");
    windowsGroup.writeEntry("Placement", Placement::policyToString(Placement::Smart));
    windowsGroup.sync();
    setConfig(config);

    const auto ownPath = libraryPaths().last();
    removeLibraryPath(ownPath);
    addLibraryPath(ownPath);

    const KPluginMetaData plugin = KPluginMetaData::findPluginById(QStringLiteral("org.kde.kwin.waylandbackends"), "KWinWaylandVirtualBackend");
    if (!plugin.isValid()) {
        quit();
        return;
    }
    initPlatform(plugin);
    WaylandServer::create(this);
    setProcessStartupEnvironment(QProcessEnvironment::systemEnvironment());
}

WaylandTestApplication::~WaylandTestApplication()
{
    setTerminating();
    // need to unload all effects prior to destroying X connection as they might do X calls
    // also before destroy Workspace, as effects might call into Workspace
    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->unloadAllEffects();
    }
    delete m_xwayland;
    m_xwayland = nullptr;
    destroyWorkspace();
    if (QStyle *s = style()) {
        s->unpolish(this);
    }
    destroyInputMethod();
    destroyCompositor();
    destroyInput();
}

void WaylandTestApplication::performStartup()
{
    if (!m_inputMethodServerToStart.isEmpty()) {
        InputMethod::create();
        if (m_inputMethodServerToStart != QStringLiteral("internal")) {
            InputMethod::self()->setInputMethodCommand(m_inputMethodServerToStart);
            InputMethod::self()->setEnabled(true);
        }
    }

    // first load options - done internally by a different thread
    createOptions();
    if (!platform()->initialize()) {
        std::exit(1);
    }
    waylandServer()->initPlatform();
    createColorManager();

    // try creating the Wayland Backend
    createInput();
    createPlugins();

    if (!platform()->enabledOutputs().isEmpty()) {
        continueStartupWithScreens();
    } else {
        connect(platform(), &Platform::screensQueried, this, &WaylandTestApplication::continueStartupWithScreens);
    }
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
