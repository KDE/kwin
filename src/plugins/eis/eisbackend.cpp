/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisbackend.h"

#include "eiscontext.h"
#include "eisdevice.h"
#include "libeis_logging.h"

#include "core/output.h"
#include "input.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "workspace.h"
#include "xkb.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QFlags>
#include <QSocketNotifier>

#include <libeis.h>

#include <ranges>

namespace KWin
{

EisBackend::EisBackend(QObject *parent)
    : KWin::InputBackend(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &service) {
        std::erase_if(m_contexts, [&service](const std::unique_ptr<EisContext> &context) {
            return context->dbusService == service;
        });
        m_serviceWatcher->removeWatchedService(service);
    });
}

EisBackend::~EisBackend()
{
}

void EisBackend::initialize()
{
    const QByteArray keyMap = input()->keyboard()->xkb()->keymapContents();
    m_keymapFile = RamFile("eis keymap", keyMap.data(), keyMap.size(), RamFile::Flag::SealWrite);
    connect(input()->keyboard()->keyboardLayout(), &KeyboardLayout::layoutsReconfigured, this, [this] {
        const QByteArray keyMap = input()->keyboard()->xkb()->keymapContents();
        m_keymapFile = RamFile("eis keymap", keyMap.data(), keyMap.size(), RamFile::Flag::SealWrite);
        for (const auto &context : m_contexts) {
            context->updateKeymap();
        }
    });

    QDBusConnection::sessionBus().registerObject("/org/kde/KWin/EIS/RemoteDesktop", "org.kde.KWin.EIS.RemoteDesktop", this, QDBusConnection::ExportAllInvokables);
}

void EisBackend::updateScreens()
{
    for (const auto &context : m_contexts) {
        context->updateScreens();
    }
}

QDBusUnixFileDescriptor EisBackend::connectToEIS(const int &capabilities, int &cookie)
{
    constexpr int keyboardPortal = 1;
    constexpr int pointerPortal = 2;
    constexpr int touchPortal = 4;
    QFlags<eis_device_capability> eisCapabilities;
    if (capabilities & keyboardPortal) {
        eisCapabilities |= EIS_DEVICE_CAP_KEYBOARD;
    }
    if (capabilities & pointerPortal) {
        eisCapabilities |= EIS_DEVICE_CAP_POINTER;
        eisCapabilities |= EIS_DEVICE_CAP_POINTER_ABSOLUTE;
        eisCapabilities |= EIS_DEVICE_CAP_BUTTON;
        eisCapabilities |= EIS_DEVICE_CAP_SCROLL;
    }
    if (capabilities & touchPortal) {
        eisCapabilities |= EIS_DEVICE_CAP_TOUCH;
    }
    const QString dbusService = message().service();
    static int s_cookie = 0;
    cookie = ++s_cookie;
    m_contexts.push_back(std::make_unique<EisContext>(this, eisCapabilities, cookie, dbusService));
    m_serviceWatcher->addWatchedService(dbusService);
    return QDBusUnixFileDescriptor(m_contexts.back()->addClient());
}

void EisBackend::disconnect(int cookie)
{
    auto it = std::ranges::find(m_contexts, cookie, [](const std::unique_ptr<EisContext> &context) {
        return context->cookie;
    });
    if (it != std::ranges::end(m_contexts)) {
        m_contexts.erase(it);
    }
}

eis_device *createDevice(eis_seat *seat, const QByteArray &name)
{
    auto device = eis_seat_new_device(seat);

    auto client = eis_seat_get_client(seat);
    const char *clientName = eis_client_get_name(client);
    const QByteArray deviceName = clientName + (' ' + name);
    eis_device_configure_name(device, deviceName);
    return device;
}

eis_device *EisBackend::createPointer(eis_seat *seat)
{
    auto device = createDevice(seat, "eis pointer");
    eis_device_configure_capability(device, EIS_DEVICE_CAP_POINTER);
    eis_device_configure_capability(device, EIS_DEVICE_CAP_SCROLL);
    eis_device_configure_capability(device, EIS_DEVICE_CAP_BUTTON);
    return device;
}

eis_device *EisBackend::createAbsoluteDevice(eis_seat *seat)
{
    auto device = createDevice(seat, "eis absolute device");
    auto eisDevice = device;
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_SCROLL);
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_BUTTON);
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_TOUCH);

    const auto outputs = workspace()->outputs();
    for (const auto output : outputs) {
        auto region = eis_device_new_region(eisDevice);
        const QRect outputGeometry = output->geometry();
        eis_region_set_offset(region, outputGeometry.x(), outputGeometry.y());
        eis_region_set_size(region, outputGeometry.width(), outputGeometry.height());
        eis_region_set_physical_scale(region, output->scale());
        eis_region_add(region);
        eis_region_unref(region);
    };

    return device;
}

eis_device *EisBackend::createKeyboard(eis_seat *seat)
{
    auto device = createDevice(seat, "eis keyboard");
    eis_device_configure_capability(device, EIS_DEVICE_CAP_KEYBOARD);

    if (m_keymapFile.isValid()) {
        auto keymap = eis_device_new_keymap(device, EIS_KEYMAP_TYPE_XKB, m_keymapFile.fd(), m_keymapFile.size());
        eis_keymap_add(keymap);
        eis_keymap_unref(keymap);
    }

    return device;
}
}
