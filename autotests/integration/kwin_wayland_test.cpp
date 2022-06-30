/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "backends/virtual/virtual_backend.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "core/session.h"
#include "effects.h"
#include "inputmethod.h"
#include "placement.h"
#include "pluginmanager.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xwayland/xwayland.h"

#include <KPluginMetaData>

#include <QAbstractEventDispatcher>
#include <QPluginLoader>
#include <QSocketNotifier>
#include <QThread>
#include <QtConcurrentRun>

// system
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Q_IMPORT_PLUGIN(KWinIntegrationPlugin)
Q_IMPORT_PLUGIN(KGlobalAccelImpl)
Q_IMPORT_PLUGIN(KWindowSystemKWinPlugin)
Q_IMPORT_PLUGIN(KWinIdleTimePoller)

namespace KWin
{

WaylandTestApplication::WaylandTestApplication(OperationMode mode, int &argc, char **argv)
    : Application(mode, argc, argv)
{
    QStandardPaths::setTestModeEnabled(true);
    // TODO: add a test move to kglobalaccel instead?
    QFile{QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kglobalshortcutsrc"))}.remove();
    QIcon::setThemeName(QStringLiteral("breeze"));
#if KWIN_BUILD_ACTIVITIES
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
    windowsGroup.writeEntry("Placement", Placement::policyToString(PlacementSmart));
    windowsGroup.sync();
    setConfig(config);

    const auto ownPath = libraryPaths().last();
    removeLibraryPath(ownPath);
    addLibraryPath(ownPath);

    setSession(Session::create(Session::Type::Noop));
    setOutputBackend(std::make_unique<VirtualBackend>());
    WaylandServer::create(this);
    setProcessStartupEnvironment(QProcessEnvironment::systemEnvironment());
}

WaylandTestApplication::~WaylandTestApplication()
{
    setTerminating();
    // need to unload all effects prior to destroying X connection as they might do X calls
    // also before destroy Workspace, as effects might call into Workspace
    if (effects) {
        static_cast<EffectsHandlerImpl *>(effects)->unloadAllEffects();
    }
    m_xwayland.reset();
    destroyVirtualInputDevices();
    destroyColorManager();
    destroyWorkspace();
    destroyInputMethod();
    destroyCompositor();
    destroyInput();
}

void WaylandTestApplication::createVirtualInputDevices()
{
    m_virtualKeyboard.reset(new Test::VirtualInputDevice());
    m_virtualKeyboard->setName(QStringLiteral("Virtual Keyboard 1"));
    m_virtualKeyboard->setKeyboard(true);

    m_virtualPointer.reset(new Test::VirtualInputDevice());
    m_virtualPointer->setName(QStringLiteral("Virtual Pointer 1"));
    m_virtualPointer->setPointer(true);

    m_virtualTouch.reset(new Test::VirtualInputDevice());
    m_virtualTouch->setName(QStringLiteral("Virtual Touch 1"));
    m_virtualTouch->setTouch(true);

    input()->addInputDevice(m_virtualPointer.get());
    input()->addInputDevice(m_virtualTouch.get());
    input()->addInputDevice(m_virtualKeyboard.get());
}

void WaylandTestApplication::destroyVirtualInputDevices()
{
    input()->removeInputDevice(m_virtualPointer.get());
    input()->removeInputDevice(m_virtualTouch.get());
    input()->removeInputDevice(m_virtualKeyboard.get());
}

void WaylandTestApplication::performStartup()
{
    if (!m_inputMethodServerToStart.isEmpty()) {
        createInputMethod();
        if (m_inputMethodServerToStart != QStringLiteral("internal")) {
            inputMethod()->setInputMethodCommand(m_inputMethodServerToStart);
            inputMethod()->setEnabled(true);
        }
    }

    // first load options - done internally by a different thread
    createOptions();
    if (!outputBackend()->initialize()) {
        std::exit(1);
    }

    // try creating the Wayland Backend
    createInput();
    createVirtualInputDevices();

    WaylandCompositor::create();
    connect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::continueStartupWithScene);
}

void WaylandTestApplication::finalizeStartup()
{
    if (m_xwayland) {
        disconnect(m_xwayland.get(), &Xwl::Xwayland::errorOccurred, this, &WaylandTestApplication::finalizeStartup);
        disconnect(m_xwayland.get(), &Xwl::Xwayland::started, this, &WaylandTestApplication::finalizeStartup);
    }
    notifyStarted();
}

void WaylandTestApplication::continueStartupWithScene()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::continueStartupWithScene);

    createWorkspace();
    createColorManager();
    createPlugins();

    waylandServer()->initWorkspace();

    if (!waylandServer()->start()) {
        qFatal("Failed to initialize the Wayland server, exiting now");
    }

    if (operationMode() == OperationModeWaylandOnly) {
        finalizeStartup();
        return;
    }

    m_xwayland = std::make_unique<Xwl::Xwayland>(this);
    connect(m_xwayland.get(), &Xwl::Xwayland::errorOccurred, this, &WaylandTestApplication::finalizeStartup);
    connect(m_xwayland.get(), &Xwl::Xwayland::started, this, &WaylandTestApplication::finalizeStartup);
    m_xwayland->start();
}

Test::VirtualInputDevice *WaylandTestApplication::virtualPointer() const
{
    return m_virtualPointer.get();
}

Test::VirtualInputDevice *WaylandTestApplication::virtualKeyboard() const
{
    return m_virtualKeyboard.get();
}

Test::VirtualInputDevice *WaylandTestApplication::virtualTouch() const
{
    return m_virtualTouch.get();
}

XwaylandInterface *WaylandTestApplication::xwayland() const
{
    return m_xwayland.get();
}

Test::FractionalScaleManagerV1::~FractionalScaleManagerV1()
{
    destroy();
}

Test::FractionalScaleV1::~FractionalScaleV1()
{
    destroy();
}

int Test::FractionalScaleV1::preferredScale()
{
    return m_preferredScale;
}

void Test::FractionalScaleV1::wp_fractional_scale_v1_preferred_scale(uint32_t scale)
{
    m_preferredScale = scale;
}
}
