/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "backends/virtual/virtual_backend.h"
#include "compositor_wayland.h"
#include "core/session.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "inputmethod.h"
#include "placement.h"
#include "pluginmanager.h"
#include "wayland_server.h"
#include "workspace.h"

#if KWIN_BUILD_X11
#include "utils/xcbutils.h"
#include "xwayland/xwayland.h"
#include "xwayland/xwaylandlauncher.h"
#endif

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
#if KWIN_BUILD_GLOBALSHORTCUTS
Q_IMPORT_PLUGIN(KGlobalAccelImpl)
#endif
Q_IMPORT_PLUGIN(KWindowSystemKWinPlugin)
Q_IMPORT_PLUGIN(KWinIdleTimePoller)

namespace KWin
{

WaylandTestApplication::WaylandTestApplication(OperationMode mode, int &argc, char **argv)
    : Application(mode, argc, argv)
{
    QStandardPaths::setTestModeEnabled(true);

    const QStringList configs{
        QStringLiteral("kaccessrc"),
        QStringLiteral("kglobalshortcutsrc"),
        QStringLiteral("kcminputrc"),
    };
    for (const QString &config : configs) {
        if (const QString &fileName = QStandardPaths::locate(QStandardPaths::ConfigLocation, config); !fileName.isEmpty()) {
            QFile::remove(fileName);
        }
    }

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
    KConfigGroup windowsGroup = config->group(QStringLiteral("Windows"));
    windowsGroup.writeEntry("Placement", Placement::policyToString(PlacementSmart));
    windowsGroup.sync();
    KConfigGroup edgeBarrierGroup = config->group(QStringLiteral("EdgeBarrier"));
    edgeBarrierGroup.writeEntry("EdgeBarrier", 0);
    edgeBarrierGroup.writeEntry("CornerBarrier", false);
    edgeBarrierGroup.sync();
    setConfig(config);

    const auto ownPath = libraryPaths().last();
    removeLibraryPath(ownPath);
    addLibraryPath(ownPath);

    setSession(Session::create(Session::Type::Noop));
    setOutputBackend(std::make_unique<VirtualBackend>());
    m_waylandServer.reset(WaylandServer::create());
    setProcessStartupEnvironment(QProcessEnvironment::systemEnvironment());
}

WaylandTestApplication::~WaylandTestApplication()
{
    setTerminating();
    // need to unload all effects prior to destroying X connection as they might do X calls
    // also before destroy Workspace, as effects might call into Workspace
    if (effects) {
        effects->unloadAllEffects();
    }
#if KWIN_BUILD_X11
    m_xwayland.reset();
#endif
    destroyVirtualInputDevices();
    destroyColorManager();
    destroyWorkspace();
    destroyInputMethod();
    destroyCompositor();
    destroyInput();
    m_waylandServer.reset();
}

void WaylandTestApplication::createVirtualInputDevices()
{
    m_virtualKeyboard = std::make_unique<Test::VirtualInputDevice>();
    m_virtualKeyboard->setName(QStringLiteral("Virtual Keyboard 1"));
    m_virtualKeyboard->setKeyboard(true);

    m_virtualPointer = std::make_unique<Test::VirtualInputDevice>();
    m_virtualPointer->setName(QStringLiteral("Virtual Pointer 1"));
    m_virtualPointer->setPointer(true);

    m_virtualTouch = std::make_unique<Test::VirtualInputDevice>();
    m_virtualTouch->setName(QStringLiteral("Virtual Touch 1"));
    m_virtualTouch->setTouch(true);

    m_virtualTabletPad = std::make_unique<Test::VirtualInputDevice>();
    m_virtualTabletPad->setName(QStringLiteral("Virtual Tablet Pad 1"));
    m_virtualTabletPad->setTabletPad(true);
    m_virtualTabletPad->setGroup(0xdeadbeef);

    m_virtualTablet = std::make_unique<Test::VirtualInputDevice>();
    m_virtualTablet->setName(QStringLiteral("Virtual Tablet Tool 1"));
    m_virtualTablet->setTabletTool(true);
    m_virtualTablet->setGroup(0xdeadbeef);

    m_virtualTabletTool = std::make_unique<Test::VirtualInputDeviceTabletTool>();
    m_virtualTabletTool->setSerialId(42);
    m_virtualTabletTool->setUniqueId(42);
    m_virtualTabletTool->setType(InputDeviceTabletTool::Pen);
    m_virtualTabletTool->setCapabilities({});

    input()->addInputDevice(m_virtualPointer.get());
    input()->addInputDevice(m_virtualTouch.get());
    input()->addInputDevice(m_virtualKeyboard.get());
    input()->addInputDevice(m_virtualTabletPad.get());
    input()->addInputDevice(m_virtualTablet.get());
}

void WaylandTestApplication::destroyVirtualInputDevices()
{
    if (m_virtualPointer) {
        input()->removeInputDevice(m_virtualPointer.get());
    }
    if (m_virtualTouch) {
        input()->removeInputDevice(m_virtualTouch.get());
    }
    if (m_virtualKeyboard) {
        input()->removeInputDevice(m_virtualKeyboard.get());
    }
    if (m_virtualTabletPad) {
        input()->removeInputDevice(m_virtualTabletPad.get());
    }
    if (m_virtualTabletTool) {
        input()->removeInputDevice(m_virtualTablet.get());
    }
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
    createTabletModeManager();

    auto compositor = WaylandCompositor::create();
    createWorkspace();
    createColorManager();
    createPlugins();

    compositor->start();

    waylandServer()->initWorkspace();

    if (!waylandServer()->start()) {
        qFatal("Failed to initialize the Wayland server, exiting now");
    }

#if KWIN_BUILD_X11
    m_xwayland = std::make_unique<Xwl::Xwayland>(this);
    m_xwayland->init();
#endif
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

Test::VirtualInputDevice *WaylandTestApplication::virtualTabletPad() const
{
    return m_virtualTabletPad.get();
}

Test::VirtualInputDevice *WaylandTestApplication::virtualTablet() const
{
    return m_virtualTablet.get();
}

Test::VirtualInputDeviceTabletTool *WaylandTestApplication::virtualTabletTool() const
{
    return m_virtualTabletTool.get();
}

#if KWIN_BUILD_X11
XwaylandInterface *WaylandTestApplication::xwayland() const
{
    return m_xwayland.get();
}
#endif

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
    if (m_preferredScale == scale) {
        return;
    }
    m_preferredScale = scale;
    Q_EMIT preferredScaleChanged();
}

void Test::setOutputConfig(const QList<QRect> &geometries)
{
    QList<VirtualBackend::OutputInfo> converted;
    std::transform(geometries.begin(), geometries.end(), std::back_inserter(converted), [](const auto &geometry) {
        return VirtualBackend::OutputInfo{
            .geometry = geometry,
        };
    });
    static_cast<VirtualBackend *>(kwinApp()->outputBackend())->setVirtualOutputs(converted);
}

void Test::setOutputConfig(const QList<OutputInfo> &infos)
{
    QList<VirtualBackend::OutputInfo> converted;
    std::transform(infos.begin(), infos.end(), std::back_inserter(converted), [](const auto &info) {
        return VirtualBackend::OutputInfo{
            .geometry = info.geometry,
            .scale = info.scale,
            .internal = info.internal,
            .physicalSizeInMM = info.physicalSizeInMM,
            .modes = info.modes,
            .panelOrientation = info.panelOrientation,
            .edid = info.edid,
            .edidIdentifierOverride = info.edidIdentifierOverride,
            .connectorName = info.connectorName,
            .mstPath = info.mstPath,
        };
    });
    static_cast<VirtualBackend *>(kwinApp()->outputBackend())->setVirtualOutputs(converted);
}
}

#include "moc_kwin_wayland_test.cpp"
