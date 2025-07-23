/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "backends/virtual/virtual_backend.h"
#include "compositor.h"
#include "core/outputconfiguration.h"
#include "core/session.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "inputmethod.h"
#include "placement.h"
#include "pluginmanager.h"
#include "wayland_server.h"
#include "workspace.h"
#include "backends/drm/drm_backend.h"

#include <KWayland/Client/seat.h>

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
#include <ranges>
#include <sys/mman.h>
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

WaylandTestApplication::WaylandTestApplication(int &argc, char **argv, bool runOnKMS)
    : Application(argc, argv)
{
    QStandardPaths::setTestModeEnabled(true);

    const QStringList configs{
        QStringLiteral("kaccessrc"),
        QStringLiteral("kglobalshortcutsrc"),
        QStringLiteral("kcminputrc"),
        QStringLiteral("kxkbrc"),
        QStringLiteral("kwinoutputconfig.json"),
        QStringLiteral("kwinsessionrc"),
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
    if (!runOnKMS) {
        qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));
    }
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

    if (runOnKMS) {
        // in order to allow running the test manually on a tty,
        // we need the real session. This doesn't work in CI though,
        // so manually check for that and use the noop session instead
        if (qEnvironmentVariable("CI") == "true") {
            setSession(Session::create(Session::Type::Noop));
        } else {
            setSession(Session::create());
        }
        setOutputBackend(std::make_unique<DrmBackend>(session()));
    } else {
        setSession(Session::create(Session::Type::Noop));
        setOutputBackend(std::make_unique<VirtualBackend>());
    }
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

    auto compositor = Compositor::create();
    compositor->createRenderer();
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
    setOutputConfig(geometries | std::views::transform([](const QRect &geometry) {
        return OutputInfo{
            .geometry = geometry,
        };
    }) | std::ranges::to<QList>());
}

void Test::setOutputConfig(const QList<OutputInfo> &infos)
{
    Q_ASSERT(qobject_cast<VirtualBackend *>(kwinApp()->outputBackend()));
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

    const auto outputs = kwinApp()->outputBackend()->outputs();
    OutputConfiguration config;
    for (int i = 0; i < outputs.size(); i++) {
        const auto &info = infos[i];
        *config.changeSet(outputs[i]) = OutputChangeSet{
            .desiredModeSize = info.geometry.size() * info.scale,
            .enabled = true,
            .pos = info.geometry.topLeft(),
            .scale = info.scale,
        };
    }
    workspace()->applyOutputConfiguration(config, outputs);
}

Test::SimpleKeyboard::SimpleKeyboard(QObject *parent)
    : QObject(parent)
    , m_keyboard(Test::waylandSeat()->createKeyboard(parent))
{
    static const int EVDEV_OFFSET = 8;

    connect(m_keyboard, &KWayland::Client::Keyboard::keymapChanged, this, [this](int fd, uint32_t size) {
        char *map_shm = static_cast<char *>(
            mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
        close(fd);

        Q_ASSERT(map_shm != MAP_FAILED);

        m_keymap = XkbKeymapPtr(
            xkb_keymap_new_from_string(
                m_ctx.get(),
                map_shm,
                XKB_KEYMAP_FORMAT_TEXT_V1,
                XKB_KEYMAP_COMPILE_NO_FLAGS),
            &xkb_keymap_unref);

        munmap(map_shm, size);
        Q_ASSERT(m_keymap);

        m_state = XkbStatePtr(xkb_state_new(m_keymap.get()), &xkb_state_unref);
        Q_ASSERT(m_state);
    });

    connect(m_keyboard, &KWayland::Client::Keyboard::modifiersChanged, this, [this](quint32 depressed, quint32 latched, quint32 locked, quint32 group) {
        if (!m_state) {
            return;
        }
        xkb_state_update_mask(
            m_state.get(),
            depressed,
            latched,
            locked,
            0, 0,
            group);
    });

    connect(m_keyboard, &KWayland::Client::Keyboard::keyChanged, this, [this](quint32 key, KWayland::Client::Keyboard::KeyState state, quint32 time) {
        if (!m_state) {
            return;
        }

        xkb_keycode_t kc = key + EVDEV_OFFSET;

        if (state == KWayland::Client::Keyboard::KeyState::Pressed) {
            const xkb_keysym_t *syms;
            int nsyms = xkb_state_key_get_syms(m_state.get(), kc, &syms);
            for (int i = 0; i < nsyms; i++) {
                char buf[64];
                int len = xkb_keysym_to_utf8(syms[i], buf, sizeof(buf));
                if (len > 1) {
                    m_receviedText.append(QString::fromUtf8(buf, len - 1)); // xkb_keysym_to_utf8 contains terminating byte
                    Q_EMIT receviedTextChanged();
                }
                Q_EMIT keySymRecevied(syms[i]);
            }
        }
    });
}

KWayland::Client::Keyboard *Test::SimpleKeyboard::keyboard()
{
    return m_keyboard;
}

QString Test::SimpleKeyboard::receviedText()
{
    return m_receviedText;
}
}

#include "moc_kwin_wayland_test.cpp"
