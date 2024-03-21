/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "inputbackend.h"

#include "device.h"
#include "libeis_logging.h"

#include "core/output.h"
#include "input.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "main.h"
#include "workspace.h"
#include "xkb.h"

#include <QDBusConnection>
#include <QFlags>
#include <QSocketNotifier>

#include <libeis.h>

#include <ranges>

namespace KWin
{
static void eis_log_handler(eis *eis, eis_log_priority priority, const char *message, eis_log_context *context)
{
    switch (priority) {
    case EIS_LOG_PRIORITY_DEBUG:
        qCDebug(KWIN_EIS) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_INFO:
        qCInfo(KWIN_EIS) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_WARNING:
        qCWarning(KWIN_EIS) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_ERROR:
        qCritical(KWIN_EIS) << "Libeis:" << message;
        break;
    }
}

class EisClient
{
public:
    EisClient(eis_client *client, eis_seat *seat)
        : handle(client)
        , seat(seat)
    {
        eis_seat_set_user_data(seat, this);
        eis_client_set_user_data(client, this);
    }
    ~EisClient()
    {
        eis_seat_unref(seat);
        eis_client_disconnect(handle);
    }
    eis_client *handle;
    eis_seat *seat;
    std::unique_ptr<EisDevice> absoluteDevice;
    std::unique_ptr<EisDevice> pointer;
    std::unique_ptr<EisDevice> keyboard;
};

struct EisContext
{
public:
    EisContext(QFlags<eis_device_capability> allowedCapabilities)
        : eis_context(eis_new(this))
        , allowedCapabilities(allowedCapabilities)
        , socketNotifier(eis_get_fd(eis_context), QSocketNotifier::Read)
    {
        eis_setup_backend_fd(eis_context);
    }
    ~EisContext()
    {
        eis_unref(eis_context);
    }
    eis *eis_context;
    QFlags<eis_device_capability> allowedCapabilities;
    int cookie;
    QSocketNotifier socketNotifier;
    std::vector<std::unique_ptr<EisClient>> clients;
};

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

EisBackend::EisBackend(QObject *parent)
    : KWin::InputBackend(parent)
{
}

EisBackend::~EisBackend()
{
}

void EisBackend::initialize()
{
    connect(workspace(), &Workspace::outputsChanged, this, [this] {
        for (const auto &context : m_contexts) {
            for (const auto &seat : context->clients) {
                if (seat->absoluteDevice) {
                    seat->absoluteDevice->changeDevice(createAbsoluteDevice(seat->seat));
                }
            }
        }
    });

    const QByteArray keyMap = input()->keyboard()->xkb()->keymapContents();
    m_keymapFile = RamFile("eis keymap", keyMap.data(), keyMap.size(), RamFile::Flag::SealWrite);
    connect(input()->keyboard()->keyboardLayout(), &KeyboardLayout::layoutsReconfigured, this, [this] {
        const QByteArray keyMap = input()->keyboard()->xkb()->keymapContents();
        m_keymapFile = RamFile("eis keymap", keyMap.data(), keyMap.size(), RamFile::Flag::SealWrite);
        for (const auto &context : m_contexts) {
            for (const auto &seat : context->clients) {
                if (seat->keyboard) {
                    seat->keyboard->changeDevice(createKeyboard(seat->seat));
                }
            }
        }
    });
    QDBusConnection::sessionBus().registerObject("/org/kde/KWin/EIS/RemoteDesktop", "org.kde.KWin.EIS.RemoteDesktop", this, QDBusConnection::ExportAllInvokables);
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
    m_contexts.push_back(std::make_unique<EisContext>(eisCapabilities));
    eis_log_set_priority(m_contexts.back()->eis_context, EIS_LOG_PRIORITY_DEBUG);
    eis_log_set_handler(m_contexts.back()->eis_context, eis_log_handler);
    static int s_cookie = 0;
    cookie = ++s_cookie;
    m_contexts.back()->cookie = cookie;
    QObject::connect(&m_contexts.back()->socketNotifier, &QSocketNotifier::activated, this, std::bind(&EisBackend::handleEvents, this, m_contexts.back().get()));
    return QDBusUnixFileDescriptor(eis_backend_fd_add_client(m_contexts.back()->eis_context));
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

static std::chrono::microseconds currentTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

void EisBackend::handleEvents(EisContext *context)
{
    eis_dispatch(context->eis_context);
    auto eventDevice = [](eis_event *event) {
        return static_cast<EisDevice *>(eis_device_get_user_data(eis_event_get_device(event)));
    };
    while (eis_event *const event = eis_get_event(context->eis_context)) {
        switch (eis_event_get_type(event)) {
        case EIS_EVENT_CLIENT_CONNECT: {
            auto client = eis_event_get_client(event);
            const char *clientName = eis_client_get_name(client);
            if (!eis_client_is_sender(client)) {
                qCDebug(KWIN_EIS) << "disconnecting receiving client" << clientName;
                eis_client_disconnect(client);
                return;
            }
            eis_client_connect(client);

            auto seat = eis_client_new_seat(client, QByteArrayLiteral(" seat").prepend(clientName));
            constexpr std::array allCapabilities{EIS_DEVICE_CAP_POINTER, EIS_DEVICE_CAP_POINTER_ABSOLUTE, EIS_DEVICE_CAP_KEYBOARD, EIS_DEVICE_CAP_TOUCH, EIS_DEVICE_CAP_SCROLL, EIS_DEVICE_CAP_BUTTON};
            for (auto capability : allCapabilities) {
                if (context->allowedCapabilities & capability) {
                    eis_seat_configure_capability(seat, capability);
                }
            }

            eis_seat_add(seat);
            context->clients.emplace_back(std::make_unique<EisClient>(client, seat));
            qCDebug(KWIN_EIS) << "New eis client" << clientName;
            break;
        }
        case EIS_EVENT_CLIENT_DISCONNECT: {
            auto client = eis_event_get_client(event);
            qCDebug(KWIN_EIS) << "Client disconnected" << eis_client_get_name(client);
            if (auto seat = static_cast<EisClient *>(eis_client_get_user_data(client))) {
                context->clients.erase(std::ranges::find(context->clients, seat, &std::unique_ptr<EisClient>::get));
            }
            break;
        }
        case EIS_EVENT_SEAT_BIND: {
            auto seat = eis_event_get_seat(event);
            auto clientSeat = static_cast<EisClient *>(eis_seat_get_user_data(seat));
            qCDebug(KWIN_EIS) << "Client" << eis_client_get_name(eis_event_get_client(event)) << "bound to seat" << eis_seat_get_name(seat);
            auto updateDevice = [event, this](std::unique_ptr<EisDevice> &device, auto &&createFunc, bool shouldHave) {
                if (shouldHave) {
                    if (!device) {
                        device = std::make_unique<EisDevice>(std::invoke(createFunc, this, (eis_event_get_seat(event))));
                        device->setEnabled(true);
                        Q_EMIT deviceAdded(device.get());
                    }
                } else if (device) {
                    Q_EMIT deviceRemoved(device.get());
                    device.reset();
                }
            };
            updateDevice(clientSeat->absoluteDevice, &EisBackend::createAbsoluteDevice, eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER_ABSOLUTE) || eis_event_seat_has_capability(event, EIS_DEVICE_CAP_TOUCH));
            updateDevice(clientSeat->pointer, &EisBackend::createPointer, eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER));
            updateDevice(clientSeat->keyboard, &EisBackend::createKeyboard, eis_event_seat_has_capability(event, EIS_DEVICE_CAP_KEYBOARD));
            break;
        }
        case EIS_EVENT_DEVICE_CLOSED: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Device" << device->name() << "closed by client";
            Q_EMIT deviceRemoved(device);
            auto seat = static_cast<EisClient *>(eis_seat_get_user_data(eis_device_get_seat(device->handle())));
            if (device == seat->absoluteDevice.get()) {
                seat->absoluteDevice.reset();
            } else if (device == seat->keyboard.get()) {
                seat->keyboard.reset();
            } else if (device == seat->pointer.get()) {
                seat->pointer.reset();
            }
            break;
        }
        case EIS_EVENT_FRAME: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Frame for device" << device->name();
            if (device->isTouch()) {
                Q_EMIT device->touchFrame(device);
            }
            if (device->isPointer()) {
                Q_EMIT device->pointerFrame(device);
            }
            break;
        }
        case EIS_EVENT_DEVICE_START_EMULATING: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Device" << device->name() << "starts emulating";
            break;
        }
        case EIS_EVENT_DEVICE_STOP_EMULATING: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Device" << device->name() << "stops emulating";
            break;
        }
        case EIS_EVENT_POINTER_MOTION: {
            auto device = eventDevice(event);
            const double x = eis_event_pointer_get_dx(event);
            const double y = eis_event_pointer_get_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "pointer motion" << x << y;
            const QPointF delta(x, y);
            Q_EMIT device->pointerMotion(delta, delta, currentTime(), device);
            break;
        }
        case EIS_EVENT_POINTER_MOTION_ABSOLUTE: {
            auto device = eventDevice(event);
            const double x = eis_event_pointer_get_absolute_x(event);
            const double y = eis_event_pointer_get_absolute_y(event);
            qCDebug(KWIN_EIS) << device->name() << "pointer motion absolute" << x << y;
            Q_EMIT device->pointerMotionAbsolute({x, y}, currentTime(), device);
            break;
        }
        case EIS_EVENT_BUTTON_BUTTON: {
            auto device = eventDevice(event);
            const quint32 button = eis_event_button_get_button(event);
            const bool press = eis_event_button_get_is_press(event);
            qCDebug(KWIN_EIS) << device->name() << "button" << button << press;
            Q_EMIT device->pointerButtonChanged(button, press ? InputRedirection::PointerButtonPressed : InputRedirection::PointerButtonReleased, currentTime(), device);
            break;
        }
        case EIS_EVENT_SCROLL_DELTA: {
            auto device = eventDevice(event);
            const auto x = eis_event_scroll_get_dx(event);
            const auto y = eis_event_scroll_get_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "scroll" << x << y;
            if (x != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisHorizontal, x, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            if (y != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisVertical, y, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_SCROLL_STOP:
        case EIS_EVENT_SCROLL_CANCEL: {
            auto device = eventDevice(event);
            if (eis_event_scroll_get_stop_x(event)) {
                qCDebug(KWIN_EIS) << device->name() << "scroll x" << (eis_event_get_type(event) == EIS_EVENT_SCROLL_STOP ? "stop" : "cancel");
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisHorizontal, 0, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            if (eis_event_scroll_get_stop_y(event)) {
                qCDebug(KWIN_EIS) << device->name() << "scroll y" << (eis_event_get_type(event) == EIS_EVENT_SCROLL_STOP ? "stop" : "cancel");
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisVertical, 0, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_SCROLL_DISCRETE: {
            auto device = eventDevice(event);
            const double x = eis_event_scroll_get_discrete_dx(event);
            const double y = eis_event_scroll_get_discrete_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "scroll discrete" << x << y;
            // otherwise no scroll event
            constexpr auto anglePer120Step = 15 / 120.0;
            if (x != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisHorizontal, x * anglePer120Step, x, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            if (y != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisVertical, y * anglePer120Step, y, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_KEYBOARD_KEY: {
            auto device = eventDevice(event);
            const quint32 key = eis_event_keyboard_get_key(event);
            const bool press = eis_event_keyboard_get_key_is_press(event);
            qCDebug(KWIN_EIS) << device->name() << "key" << key << press;
            Q_EMIT device->keyChanged(key, press ? InputRedirection::KeyboardKeyPressed : InputRedirection::KeyboardKeyReleased, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_DOWN: {
            auto device = eventDevice(event);
            const auto x = eis_event_touch_get_x(event);
            const auto y = eis_event_touch_get_y(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch down" << id << x << y;
            Q_EMIT device->touchDown(id, {x, y}, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_UP: {
            auto device = eventDevice(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch up" << id;
            Q_EMIT device->touchUp(id, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_MOTION: {
            auto device = eventDevice(event);
            const auto x = eis_event_touch_get_x(event);
            const auto y = eis_event_touch_get_y(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch move" << id << x << y;
            Q_EMIT device->touchMotion(id, {x, y}, currentTime(), device);
            break;
        }
        }
        eis_event_unref(event);
    }
}
}
