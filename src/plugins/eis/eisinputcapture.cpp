/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisinputcapture.h"

#include "eisinputcapturemanager.h"
#include "inputcapture_logging.h"

#include "core/output.h"
#include "cursor.h"
#include "input.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "workspace.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace KWin
{

static void eis_log_handler(eis *eis, eis_log_priority priority, const char *message, eis_log_context *context)
{
    switch (priority) {
    case EIS_LOG_PRIORITY_DEBUG:
        qCDebug(KWIN_INPUTCAPTURE) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_INFO:
        qCInfo(KWIN_INPUTCAPTURE) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_WARNING:
        qCWarning(KWIN_INPUTCAPTURE) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_ERROR:
        qCritical(KWIN_INPUTCAPTURE) << "Libeis:" << message;
        break;
    }
}

static eis_device *createDevice(eis_seat *seat, const QByteArray &name)
{
    auto device = eis_seat_new_device(seat);

    auto client = eis_seat_get_client(seat);
    const char *clientName = eis_client_get_name(client);
    const QByteArray deviceName = clientName + (' ' + name);
    eis_device_configure_name(device, qPrintable(deviceName));
    return device;
}

static eis_device *createPointer(eis_seat *seat)
{
    auto device = createDevice(seat, "capture pointer");
    eis_device_configure_capability(device, EIS_DEVICE_CAP_POINTER);
    eis_device_configure_capability(device, EIS_DEVICE_CAP_SCROLL);
    eis_device_configure_capability(device, EIS_DEVICE_CAP_BUTTON);
    eis_device_add(device);
    eis_device_resume(device);
    return device;
}

static eis_device *createAbsoluteDevice(eis_seat *seat)
{
    auto device = createDevice(seat, "capture absolute device");
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
    eis_device_add(device);
    eis_device_resume(device);
    return device;
}

static eis_device *createKeyboard(eis_seat *seat, const RamFile &keymap)
{
    auto device = createDevice(seat, "capture keyboard");
    eis_device_configure_capability(device, EIS_DEVICE_CAP_KEYBOARD);

    if (keymap.isValid()) {
        auto eisKeymap = eis_device_new_keymap(device, EIS_KEYMAP_TYPE_XKB, keymap.fd(), keymap.size());
        eis_keymap_add(eisKeymap);
        eis_keymap_unref(eisKeymap);
    }

    eis_device_add(device);
    eis_device_resume(device);
    return device;
}

EisInputCapture::EisInputCapture(EisInputCaptureManager *manager, const QString &dbusService, QFlags<eis_device_capability> allowedCapabilities)
    : dbusService(dbusService)
    , m_manager(manager)
    , m_allowedCapabilities(allowedCapabilities)
    , m_eis(eis_new(this))
    , m_socketNotifier(eis_get_fd(m_eis), QSocketNotifier::Read)
{
    eis_setup_backend_fd(m_eis);
    eis_log_set_priority(m_eis, EIS_LOG_PRIORITY_DEBUG);
    eis_log_set_handler(m_eis, eis_log_handler);
    connect(&m_socketNotifier, &QSocketNotifier::activated, this, &EisInputCapture::handleEvents);
    static int counter = 0;
    m_dbusPath = QStringLiteral("/org/kde/KWin/EIS/InputCapture/%1").arg(++counter);
    QDBusConnection::sessionBus().registerObject(m_dbusPath, "org.kde.KWin.EIS.InputCapture", this, QDBusConnection::ExportAllInvokables | QDBusConnection::ExportAllSignals);
}

EisInputCapture::~EisInputCapture()
{
    if (m_absoluteDevice) {
        eis_device_unref(m_absoluteDevice);
    }
    if (m_pointer) {
        eis_device_unref(m_pointer);
    }
    if (m_keyboard) {
        eis_device_unref(m_keyboard);
    }
    if (m_seat) {
        eis_seat_unref(m_seat);
    }
    if (m_client) {
        eis_client_disconnect(m_client);
    }
    eis_unref(m_eis);
}

QString EisInputCapture::dbusPath() const
{
    return m_dbusPath;
}

QList<EisInputCaptureBarrier> EisInputCapture::barriers() const
{
    return m_barriers;
}

eis_device *EisInputCapture::pointer() const
{
    return m_pointer;
}

eis_device *EisInputCapture::keyboard() const
{
    return m_keyboard;
}

eis_device *EisInputCapture::absoluteDevice() const
{
    return m_absoluteDevice;
}

void EisInputCapture::activate(const QPointF &position)
{
    Q_EMIT activated(++m_activationId, position);
    if (m_pointer) {
        eis_device_start_emulating(m_pointer, m_activationId);
    }
    if (m_keyboard) {
        eis_device_start_emulating(m_keyboard, m_activationId);
    }
    if (m_absoluteDevice) {
        eis_device_start_emulating(m_absoluteDevice, m_activationId);
    }
}

void EisInputCapture::deactivate()
{
    Q_EMIT deactivated(m_activationId);
    if (m_pointer) {
        eis_device_stop_emulating(m_pointer);
    }
    if (m_keyboard) {
        eis_device_stop_emulating(m_keyboard);
    }
    if (m_absoluteDevice) {
        eis_device_stop_emulating(m_absoluteDevice);
    }
}

void EisInputCapture::enable(const QList<QPair<QPoint, QPoint>> &barriers)
{
    m_barriers.clear();
    m_barriers.reserve(barriers.size());
    for (const auto &[p1, p2] : barriers) {
        if (p1.x() == p2.x()) {
            m_barriers.push_back({.orientation = Qt::Vertical, .position = p1.x(), .start = p1.y(), .end = p2.y()});
        } else if (p1.y() == p2.y()) {
            m_barriers.push_back({.orientation = Qt::Horizontal, .position = p1.y(), .start = p1.x(), .end = p2.x()});
        }
    }
}

void EisInputCapture::disable()
{
    if (m_manager->activeCapture() == this) {
        deactivate();
    }
    m_barriers.clear();
    Q_EMIT disabled();
}

void EisInputCapture::release(const QPointF &cursorPosition, bool applyPosition)
{
    if (m_manager->activeCapture() != this) {
        return;
    }
    if (applyPosition) {
        Cursors::self()->mouse()->setPos(cursorPosition);
    }
    deactivate();
}

QDBusUnixFileDescriptor EisInputCapture::connectToEIS()
{
    return QDBusUnixFileDescriptor(eis_backend_fd_add_client(m_eis));
}

void EisInputCapture::handleEvents()
{
    eis_dispatch(m_eis);
    while (eis_event *const event = eis_get_event(m_eis)) {
        switch (eis_event_get_type(event)) {
        case EIS_EVENT_CLIENT_CONNECT: {
            auto client = eis_event_get_client(event);
            const char *clientName = eis_client_get_name(client);
            if (eis_client_is_sender(client)) {
                qCWarning(KWIN_INPUTCAPTURE) << "disconnecting receiving client" << clientName;
                eis_client_disconnect(client);
                break;
            }
            if (m_client) {
                qCWarning(KWIN_INPUTCAPTURE) << "unexpected client connection" << clientName;
                eis_client_disconnect(client);
                break;
            }
            eis_client_connect(client);

            m_client = client;
            m_seat = eis_client_new_seat(client, QByteArrayLiteral(" input capture seat").prepend(clientName));
            constexpr std::array allCapabilities{EIS_DEVICE_CAP_POINTER, EIS_DEVICE_CAP_POINTER_ABSOLUTE, EIS_DEVICE_CAP_KEYBOARD, EIS_DEVICE_CAP_TOUCH, EIS_DEVICE_CAP_SCROLL, EIS_DEVICE_CAP_BUTTON};
            for (auto capability : allCapabilities) {
                if (m_allowedCapabilities & capability) {
                    eis_seat_configure_capability(m_seat, capability);
                }
            }

            eis_seat_add(m_seat);
            qCDebug(KWIN_INPUTCAPTURE) << "New eis client" << clientName;
            break;
        }
        case EIS_EVENT_CLIENT_DISCONNECT: {
            auto client = eis_event_get_client(event);
            if (client != m_client) {
                break;
            }
            qCDebug(KWIN_INPUTCAPTURE) << "Client disconnected" << eis_client_get_name(client);
            eis_seat_unref(std::exchange(m_seat, nullptr));
            eis_client_unref(std::exchange(m_client, nullptr));
            break;
        }
        case EIS_EVENT_SEAT_BIND: {
            auto seat = eis_event_get_seat(event);
            qCDebug(KWIN_INPUTCAPTURE) << "Client" << eis_client_get_name(eis_event_get_client(event)) << "bound to seat" << eis_seat_get_name(seat);
            if (eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER_ABSOLUTE) || eis_event_seat_has_capability(event, EIS_DEVICE_CAP_TOUCH)) {
                if (!m_absoluteDevice) {
                    m_absoluteDevice = createAbsoluteDevice(seat);
                }
            } else if (m_absoluteDevice) {
                eis_device_remove(m_absoluteDevice);
                eis_device_unref(std::exchange(m_absoluteDevice, nullptr));
            }
            if (eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER)) {
                if (!m_pointer) {
                    m_pointer = createPointer(seat);
                }
            } else if (m_pointer) {
                eis_device_remove(m_pointer);
                eis_device_unref(std::exchange(m_pointer, nullptr));
            }
            if (eis_event_seat_has_capability(event, EIS_DEVICE_CAP_KEYBOARD)) {
                if (!m_keyboard) {
                    m_keyboard = createKeyboard(seat, m_manager->keyMap());
                }
            } else if (m_keyboard) {
                eis_device_remove(m_keyboard);
                eis_device_unref(std::exchange(m_keyboard, nullptr));
            }
            break;
        }
        case EIS_EVENT_DEVICE_CLOSED: {
            auto device = eis_event_get_device(event);
            qCDebug(KWIN_INPUTCAPTURE) << "Device" << eis_device_get_name(device) << "closed by client";
            if (device == m_pointer) {
                m_pointer = nullptr;
            } else if (device == m_keyboard) {
                m_keyboard = nullptr;
            } else if (device == m_absoluteDevice) {
                m_absoluteDevice = nullptr;
            }
            eis_device_remove(device);
            eis_device_unref(device);
            break;
        }
        default:
            qCDebug(KWIN_INPUTCAPTURE) << "Unexpected event" << eis_event_get_type(event);
            break;
        }
        eis_event_unref(event);
    }
}

}
