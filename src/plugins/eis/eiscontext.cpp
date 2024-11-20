/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eiscontext.h"
#include "eisbackend.h"
#include "eisdevice.h"
#include "libeis_logging.h"

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

struct EisClient
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

EisContext::EisContext(KWin::EisBackend *backend, QFlags<eis_device_capability> allowedCapabilities, int cookie, const QString &dbusService)
    : cookie(cookie)
    , dbusService(dbusService)
    , m_backend(backend)
    , m_eisContext(eis_new(this))
    , m_allowedCapabilities(allowedCapabilities)
    , m_socketNotifier(eis_get_fd(m_eisContext), QSocketNotifier::Read)
{
    eis_setup_backend_fd(m_eisContext);
    eis_log_set_priority(m_eisContext, EIS_LOG_PRIORITY_DEBUG);
    eis_log_set_handler(m_eisContext, eis_log_handler);
    QObject::connect(&m_socketNotifier, &QSocketNotifier::activated, [this] {
        handleEvents();
    });
}

EisContext::~EisContext()
{
    for (const auto &client : m_clients) {
        if (client->absoluteDevice) {
            Q_EMIT m_backend->deviceRemoved(client->absoluteDevice.get());
        }
        if (client->pointer) {
            Q_EMIT m_backend->deviceRemoved(client->pointer.get());
        }
        if (client->keyboard) {
            Q_EMIT m_backend->deviceRemoved(client->keyboard.get());
        }
    }
}

void EisContext::updateScreens()
{
    for (const auto &client : m_clients) {
        if (client->absoluteDevice) {
            client->absoluteDevice->changeDevice(m_backend->createAbsoluteDevice(client->seat));
        }
    }
}

void EisContext::updateKeymap()
{
    for (const auto &client : m_clients) {
        if (client->keyboard) {
            client->keyboard->changeDevice(m_backend->createKeyboard(client->seat));
        }
    }
}

int EisContext::addClient()
{
    return eis_backend_fd_add_client(m_eisContext);
}

static std::chrono::microseconds currentTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

void EisContext::handleEvents()
{
    auto eventDevice = [](eis_event *event) {
        return static_cast<EisDevice *>(eis_device_get_user_data(eis_event_get_device(event)));
    };

    eis_dispatch(m_eisContext);

    while (eis_event *const event = eis_get_event(m_eisContext)) {
        switch (eis_event_get_type(event)) {
        case EIS_EVENT_CLIENT_CONNECT: {
            auto client = eis_event_get_client(event);
            const char *clientName = eis_client_get_name(client);
            if (!eis_client_is_sender(client)) {
                qCDebug(KWIN_EIS) << "disconnecting receiving client" << clientName;
                eis_client_disconnect(client);
                break;
            }
            eis_client_connect(client);

            auto seat = eis_client_new_seat(client, QByteArrayLiteral(" seat").prepend(clientName));
            constexpr std::array allCapabilities{EIS_DEVICE_CAP_POINTER, EIS_DEVICE_CAP_POINTER_ABSOLUTE, EIS_DEVICE_CAP_KEYBOARD, EIS_DEVICE_CAP_TOUCH, EIS_DEVICE_CAP_SCROLL, EIS_DEVICE_CAP_BUTTON};
            for (auto capability : allCapabilities) {
                if (m_allowedCapabilities & capability) {
                    eis_seat_configure_capability(seat, capability);
                }
            }

            eis_seat_add(seat);
            m_clients.emplace_back(std::make_unique<EisClient>(client, seat));
            qCDebug(KWIN_EIS) << "New eis client" << clientName;
            break;
        }
        case EIS_EVENT_CLIENT_DISCONNECT: {
            auto client = eis_event_get_client(event);
            qCDebug(KWIN_EIS) << "Client disconnected" << eis_client_get_name(client);
            if (auto seat = static_cast<EisClient *>(eis_client_get_user_data(client))) {
                m_clients.erase(std::ranges::find(m_clients, seat, &std::unique_ptr<EisClient>::get));
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
                        device = std::make_unique<EisDevice>(std::invoke(createFunc, m_backend, (eis_event_get_seat(event))));
                        device->setEnabled(true);
                        Q_EMIT m_backend->deviceAdded(device.get());
                    }
                } else if (device) {
                    Q_EMIT m_backend->deviceRemoved(device.get());
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
            Q_EMIT m_backend->deviceRemoved(device);
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
            if (press) {
                if (device->pressedButtons.contains(button)) {
                    continue;
                }
                device->pressedButtons.insert(button);
            } else {
                if (!device->pressedButtons.remove(button)) {
                    continue;
                }
            }
            Q_EMIT device->pointerButtonChanged(button, press ? PointerButtonState::Pressed : PointerButtonState::Released, currentTime(), device);
            break;
        }
        case EIS_EVENT_SCROLL_DELTA: {
            auto device = eventDevice(event);
            const auto x = eis_event_scroll_get_dx(event);
            const auto y = eis_event_scroll_get_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "scroll" << x << y;
            if (x != 0) {
                Q_EMIT device->pointerAxisChanged(PointerAxis::Horizontal, x, 0, PointerAxisSource::Unknown, false, currentTime(), device);
            }
            if (y != 0) {
                Q_EMIT device->pointerAxisChanged(PointerAxis::Vertical, y, 0, PointerAxisSource::Unknown, false, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_SCROLL_STOP:
        case EIS_EVENT_SCROLL_CANCEL: {
            auto device = eventDevice(event);
            if (eis_event_scroll_get_stop_x(event)) {
                qCDebug(KWIN_EIS) << device->name() << "scroll x" << (eis_event_get_type(event) == EIS_EVENT_SCROLL_STOP ? "stop" : "cancel");
                Q_EMIT device->pointerAxisChanged(PointerAxis::Horizontal, 0, 0, PointerAxisSource::Unknown, false, currentTime(), device);
            }
            if (eis_event_scroll_get_stop_y(event)) {
                qCDebug(KWIN_EIS) << device->name() << "scroll y" << (eis_event_get_type(event) == EIS_EVENT_SCROLL_STOP ? "stop" : "cancel");
                Q_EMIT device->pointerAxisChanged(PointerAxis::Vertical, 0, 0, PointerAxisSource::Unknown, false, currentTime(), device);
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
                Q_EMIT device->pointerAxisChanged(PointerAxis::Horizontal, x * anglePer120Step, x, PointerAxisSource::Unknown, false, currentTime(), device);
            }
            if (y != 0) {
                Q_EMIT device->pointerAxisChanged(PointerAxis::Vertical, y * anglePer120Step, y, PointerAxisSource::Unknown, false, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_KEYBOARD_KEY: {
            auto device = eventDevice(event);
            const quint32 key = eis_event_keyboard_get_key(event);
            const bool press = eis_event_keyboard_get_key_is_press(event);
            qCDebug(KWIN_EIS) << device->name() << "key" << key << press;
            if (press) {
                if (device->pressedKeys.contains(key)) {
                    continue;
                }
                device->pressedKeys.insert(key);
            } else {
                if (!device->pressedKeys.remove(key)) {
                    continue;
                }
            }
            Q_EMIT device->keyChanged(key, press ? KeyboardKeyState::Pressed : KeyboardKeyState::Released, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_DOWN: {
            auto device = eventDevice(event);
            const auto x = eis_event_touch_get_x(event);
            const auto y = eis_event_touch_get_y(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch down" << id << x << y;
            device->activeTouches.push_back(id);
            Q_EMIT device->touchDown(id, {x, y}, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_UP: {
            auto device = eventDevice(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch up" << id;
            std::erase(device->activeTouches, id);
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
