/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "connection.h"
#include "context.h"
#include "dbusproperties_interface.h"
#include "device.h"
#include "events.h"

// TODO: Make it compile also in testing environment
#ifndef KWIN_BUILD_TESTING
#include "core/output.h"
#include "core/outputbackend.h"
#include "main.h"
#include "window.h"
#include "workspace.h"
#endif

#include "core/session.h"
#include "input_event.h"
#include "libinput_logging.h"
#include "utils/realtime.h"
#include "utils/udev.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QMutexLocker>
#include <QSocketNotifier>

#include <Solid/Battery>
#include <Solid/Device>

#include <cmath>
#include <libinput.h>

namespace KWin
{
namespace LibInput
{

class ConnectionAdaptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDeviceManager")
    Q_PROPERTY(QStringList devicesSysNames READ devicesSysNames CONSTANT)

private:
    Connection *m_con;

public:
    ConnectionAdaptor(Connection *con)
        : QObject(con)
        , m_con(con)
    {
        connect(con, &Connection::deviceAdded, this, [this](LibInput::Device *inputDevice) {
            Q_EMIT deviceAdded(inputDevice->sysName());
        });
        connect(con, &Connection::deviceRemoved, this, [this](LibInput::Device *inputDevice) {
            Q_EMIT deviceRemoved(inputDevice->sysName());
        });

        QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/InputDevice"),
                                                     QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                                     this,
                                                     QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals);
    }

    ~ConnectionAdaptor() override
    {
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/kde/KWin/InputDeviceManager"));
    }

    QStringList devicesSysNames()
    {
        return m_con->devicesSysNames();
    }

Q_SIGNALS:
    void deviceAdded(QString sysName);
    void deviceRemoved(QString sysName);
};

Connection *Connection::create(Session *session)
{
    std::unique_ptr<Udev> udev = std::make_unique<Udev>();
    if (!udev->isValid()) {
        qCWarning(KWIN_LIBINPUT) << "Failed to initialize udev";
        return nullptr;
    }
    std::unique_ptr<Context> context = std::make_unique<Context>(session, std::move(udev));
    if (!context->isValid()) {
        qCWarning(KWIN_LIBINPUT) << "Failed to create context from udev";
        return nullptr;
    }
    if (!context->initialize()) {
        qCWarning(KWIN_LIBINPUT) << "Failed to initialize context";
        return nullptr;
    }
    return new Connection(std::move(context));
}

Connection::Connection(std::unique_ptr<Context> &&input)
    : m_notifier(nullptr)
    , m_connectionAdaptor(std::make_unique<ConnectionAdaptor>(this))
    , m_input(std::move(input))
{
    Q_ASSERT(m_input);
    // need to connect to KGlobalSettings as the mouse KCM does not emit a dedicated signal
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                          QStringLiteral("notifyChange"), this, SLOT(slotKGlobalSettingsNotifyChange(int, int)));
}

Connection::~Connection()
{
    for (Device *device : std::as_const(m_devices)) {
        Q_EMIT deviceRemoved(device);
    }

    m_eventQueue.clear();
    qDeleteAll(m_devices);
    qDeleteAll(m_tools);
}

void Connection::setup()
{
    QMetaObject::invokeMethod(this, &Connection::doSetup, Qt::QueuedConnection);
}

bool batteriesPluggedIn()
{
    const QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    if (devices.isEmpty()) {
        return true;
    }

    bool ret = false;
    for (const Solid::Device &device : devices) {
        auto battery = device.as<Solid::Battery>();
        if (battery && battery->type() == Solid::Battery::PrimaryBattery) {
            ret |= battery->chargeState() != Solid::Battery::Discharging;
            if (ret) {
                break;
            }
        }
    }
    return ret;
}

void Connection::doSetup()
{
    Q_ASSERT(!m_notifier);

    gainRealTime();

    m_notifier = std::make_unique<QSocketNotifier>(m_input->fileDescriptor(), QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &Connection::handleEvent);

    reconsiderThrottle();
    const QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    for (const Solid::Device &device : devices) {
        auto battery = device.as<Solid::Battery>();
        if (battery && battery->type() == Solid::Battery::PrimaryBattery) {
            connect(battery, &Solid::Battery::chargeStateChanged, this, &Connection::reconsiderThrottle);
            connect(battery, &Solid::Battery::powerSupplyStateChanged, this, &Connection::reconsiderThrottle);
        }
    }

    m_throttleScheduler = new QTimer(this);
    connect(m_throttleScheduler, &QTimer::timeout, this, &Connection::handleEvent);

    connect(m_input->session(), &Session::activeChanged, this, [this](bool active) {
        if (active) {
            if (!m_input->isSuspended()) {
                return;
            }
            m_input->resume();
        } else {
            deactivate();
        }
    });
    handleEvent();
}

void Connection::reconsiderThrottle()
{
    const QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    if (devices.isEmpty()) {
        m_throttleEvents = false;
        return;
    }

    m_throttleEvents = false;
    for (const Solid::Device &device : devices) {
        auto battery = device.as<Solid::Battery>();
        if (battery && battery->type() == Solid::Battery::PrimaryBattery) {
            if (battery->chargeState() == Solid::Battery::Discharging && battery->isPowerSupply()) {
                m_throttleEvents = true;
            }
        }
    }
}

void Connection::deactivate()
{
    if (m_input->isSuspended()) {
        return;
    }
    m_input->suspend();
    handleEvent();
}

void Connection::handleEvent()
{
    QMutexLocker locker(&m_mutex);
    const bool wasEmpty = m_eventQueue.empty();

    if (m_throttleEvents) {
        using namespace std::chrono_literals;
        const auto now = std::chrono::system_clock::now();

        const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last);
        constexpr std::chrono::milliseconds threshold = 16ms; // 60 Hz
        if (diff < threshold) {
            if (!m_throttleScheduler->isActive()) {
                m_throttleScheduler->start(threshold - diff);
            }
            return;
        }
        m_last = now;
        m_throttleScheduler->stop();
    }

    do {
        m_input->dispatch();
        std::unique_ptr<Event> event = m_input->event();
        if (!event) {
            break;
        }
        m_eventQueue.push_back(std::move(event));
    } while (true);
    if (wasEmpty && !m_eventQueue.empty()) {
        Q_EMIT eventsRead();
    }
}

#ifndef KWIN_BUILD_TESTING
QPointF devicePointToGlobalPosition(const QPointF &devicePos, const Output *output)
{
    QPointF pos = devicePos;
    // TODO: Do we need to handle the flipped cases differently?
    switch (output->transform().kind()) {
    case OutputTransform::Normal:
    case OutputTransform::FlipX:
        break;
    case OutputTransform::Rotate90:
    case OutputTransform::FlipX90:
        pos = QPointF(output->modeSize().height() - devicePos.y(), devicePos.x());
        break;
    case OutputTransform::Rotate180:
    case OutputTransform::FlipX180:
        pos = QPointF(output->modeSize().width() - devicePos.x(),
                      output->modeSize().height() - devicePos.y());
        break;
    case OutputTransform::Rotate270:
    case OutputTransform::FlipX270:
        pos = QPointF(devicePos.y(), output->modeSize().width() - devicePos.x());
        break;
    default:
        Q_UNREACHABLE();
    }
    return output->geometry().topLeft() + pos / output->scale();
}
#endif

static QPointF tabletToolPosition(TabletToolEvent *event)
{
#ifndef KWIN_BUILD_TESTING
    if (event->device()->isMapToWorkspace()) {
        return workspace()->geometry().topLeft() + event->transformedPosition(workspace()->geometry().size());
    } else {
        Output *output = event->device()->output();
        if (!output) {
            output = workspace()->activeOutput();
        }
        return devicePointToGlobalPosition(event->transformedPosition(output->modeSize()), output);
    }
#else
    return QPointF();
#endif
}

TabletTool *Connection::getOrCreateTool(libinput_tablet_tool *handle)
{
    for (TabletTool *tool : std::as_const(m_tools)) {
        if (tool->handle() == handle) {
            return tool;
        }
    }

    auto tool = new TabletTool(handle);
    tool->moveToThread(thread());
    m_tools.append(tool);

    return tool;
}

void Connection::processEvents()
{
    QMutexLocker locker(&m_mutex);
    while (m_eventQueue.size() != 0) {
        std::unique_ptr<Event> event = std::move(m_eventQueue.front());
        m_eventQueue.pop_front();
        switch (event->type()) {
        case LIBINPUT_EVENT_DEVICE_ADDED: {
            auto device = new Device(event->nativeDevice());
            device->moveToThread(thread());
            m_devices << device;

            applyDeviceConfig(device);
            applyScreenToDevice(device);

            connect(device, &Device::outputNameChanged, this, [this, device] {
                // If the output name changes from something to empty we need to
                // re-run the assignment heuristic so that an output is assinged
                if (device->outputName().isEmpty()) {
                    applyScreenToDevice(device);
                }
            });

            Q_EMIT deviceAdded(device);
            break;
        }
        case LIBINPUT_EVENT_DEVICE_REMOVED: {
            auto it = std::ranges::find_if(std::as_const(m_devices), [&event](Device *d) {
                return event->device() == d;
            });
            if (it == m_devices.cend()) {
                // we don't know this device
                break;
            }
            auto device = *it;
            m_devices.erase(it);
            Q_EMIT deviceRemoved(device);
            device->deleteLater();
            break;
        }
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            KeyEvent *ke = static_cast<KeyEvent *>(event.get());
            const int seatKeyCount = libinput_event_keyboard_get_seat_key_count(*ke);
            const int keyState = libinput_event_keyboard_get_key_state(*ke);
            if ((keyState == LIBINPUT_KEY_STATE_PRESSED && seatKeyCount != 1) ||
                (keyState == LIBINPUT_KEY_STATE_RELEASED && seatKeyCount != 0)) {
                break;
            }
            Q_EMIT ke->device()->keyChanged(ke->key(), ke->state(), ke->time(), ke->device());
            break;
        }
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
            const PointerEvent *pointerEvent = static_cast<PointerEvent *>(event.get());
            const auto axes = pointerEvent->axis();
            for (const PointerAxis &axis : axes) {
                Q_EMIT pointerEvent->device()->pointerAxisChanged(axis,
                                                                  pointerEvent->scrollValue(axis),
                                                                  pointerEvent->scrollValueV120(axis),
                                                                  PointerAxisSource::Wheel,
                                                                  pointerEvent->device()->isNaturalScroll(),
                                                                  pointerEvent->time(),
                                                                  pointerEvent->device());
            }
            Q_EMIT pointerEvent->device()->pointerFrame(pointerEvent->device());
            break;
        }
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER: {
            const PointerEvent *pointerEvent = static_cast<PointerEvent *>(event.get());
            const auto axes = pointerEvent->axis();
            for (const PointerAxis &axis : axes) {
                Q_EMIT pointerEvent->device()->pointerAxisChanged(axis,
                                                                  pointerEvent->scrollValue(axis),
                                                                  0,
                                                                  PointerAxisSource::Finger,
                                                                  pointerEvent->device()->isNaturalScroll(),
                                                                  pointerEvent->time(),
                                                                  pointerEvent->device());
            }
            Q_EMIT pointerEvent->device()->pointerFrame(pointerEvent->device());
            break;
        }
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS: {
            const PointerEvent *pointerEvent = static_cast<PointerEvent *>(event.get());
            const auto axes = pointerEvent->axis();
            for (const PointerAxis &axis : axes) {
                Q_EMIT pointerEvent->device()->pointerAxisChanged(axis,
                                                                  pointerEvent->scrollValue(axis),
                                                                  0,
                                                                  PointerAxisSource::Continuous,
                                                                  pointerEvent->device()->isNaturalScroll(),
                                                                  pointerEvent->time(),
                                                                  pointerEvent->device());
            }
            Q_EMIT pointerEvent->device()->pointerFrame(pointerEvent->device());
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            PointerEvent *pe = static_cast<PointerEvent *>(event.get());
            const int seatButtonCount = libinput_event_pointer_get_seat_button_count(*pe);
            const int buttonState = libinput_event_pointer_get_button_state(*pe);
            if ((buttonState == LIBINPUT_BUTTON_STATE_PRESSED && seatButtonCount != 1) ||
                (buttonState == LIBINPUT_BUTTON_STATE_RELEASED && seatButtonCount != 0)) {
                break;
            }
            Q_EMIT pe->device()->pointerButtonChanged(pe->button(), pe->buttonState(), pe->time(), pe->device());
            Q_EMIT pe->device()->pointerFrame(pe->device());
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION: {
            PointerEvent *pe = static_cast<PointerEvent *>(event.get());
            auto delta = pe->delta();
            auto deltaNonAccel = pe->deltaUnaccelerated();
            auto latestTime = pe->time();
            auto it = m_eventQueue.begin();
            while (it != m_eventQueue.end()) {
                if ((*it)->type() == LIBINPUT_EVENT_POINTER_MOTION) {
                    std::unique_ptr<PointerEvent> p{static_cast<PointerEvent *>(it->release())};
                    delta += p->delta();
                    deltaNonAccel += p->deltaUnaccelerated();
                    latestTime = p->time();
                    it = m_eventQueue.erase(it);
                } else {
                    break;
                }
            }
            Q_EMIT pe->device()->pointerMotion(delta, deltaNonAccel, latestTime, pe->device());
            Q_EMIT pe->device()->pointerFrame(pe->device());
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            PointerEvent *pe = static_cast<PointerEvent *>(event.get());
            if (workspace()) {
                Q_EMIT pe->device()->pointerMotionAbsolute(pe->absolutePos(workspace()->geometry().size()), pe->time(), pe->device());
                Q_EMIT pe->device()->pointerFrame(pe->device());
            }
            break;
        }
        case LIBINPUT_EVENT_TOUCH_DOWN: {
#ifndef KWIN_BUILD_TESTING
            TouchEvent *te = static_cast<TouchEvent *>(event.get());
            const auto *output = te->device()->output();
            if (!output) {
                qCWarning(KWIN_LIBINPUT) << "Touch down received for device with no output assigned";
                break;
            }
            const QPointF globalPos = devicePointToGlobalPosition(te->absolutePos(output->modeSize()), output);
            Q_EMIT te->device()->touchDown(te->id(), globalPos, te->time(), te->device());
            break;
#endif
        }
        case LIBINPUT_EVENT_TOUCH_UP: {
            TouchEvent *te = static_cast<TouchEvent *>(event.get());
            const auto *output = te->device()->output();
            if (!output) {
                break;
            }
            Q_EMIT te->device()->touchUp(te->id(), te->time(), te->device());
            break;
        }
        case LIBINPUT_EVENT_TOUCH_MOTION: {
#ifndef KWIN_BUILD_TESTING
            TouchEvent *te = static_cast<TouchEvent *>(event.get());
            const auto *output = te->device()->output();
            if (!output) {
                break;
            }
            const QPointF globalPos = devicePointToGlobalPosition(te->absolutePos(output->modeSize()), output);
            Q_EMIT te->device()->touchMotion(te->id(), globalPos, te->time(), te->device());
            break;
#endif
        }
        case LIBINPUT_EVENT_TOUCH_CANCEL: {
            Q_EMIT event->device()->touchCanceled(event->device());
            break;
        }
        case LIBINPUT_EVENT_TOUCH_FRAME: {
            Q_EMIT event->device()->touchFrame(event->device());
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN: {
            PinchGestureEvent *pe = static_cast<PinchGestureEvent *>(event.get());
            Q_EMIT pe->device()->pinchGestureBegin(pe->fingerCount(), pe->time(), pe->device());
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE: {
            PinchGestureEvent *pe = static_cast<PinchGestureEvent *>(event.get());
            Q_EMIT pe->device()->pinchGestureUpdate(pe->scale(), pe->angleDelta(), pe->delta(), pe->time(), pe->device());
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_END: {
            PinchGestureEvent *pe = static_cast<PinchGestureEvent *>(event.get());
            if (pe->isCancelled()) {
                Q_EMIT pe->device()->pinchGestureCancelled(pe->time(), pe->device());
            } else {
                Q_EMIT pe->device()->pinchGestureEnd(pe->time(), pe->device());
            }
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN: {
            SwipeGestureEvent *se = static_cast<SwipeGestureEvent *>(event.get());
            Q_EMIT se->device()->swipeGestureBegin(se->fingerCount(), se->time(), se->device());
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE: {
            SwipeGestureEvent *se = static_cast<SwipeGestureEvent *>(event.get());
            Q_EMIT se->device()->swipeGestureUpdate(se->delta(), se->time(), se->device());
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_END: {
            SwipeGestureEvent *se = static_cast<SwipeGestureEvent *>(event.get());
            if (se->isCancelled()) {
                Q_EMIT se->device()->swipeGestureCancelled(se->time(), se->device());
            } else {
                Q_EMIT se->device()->swipeGestureEnd(se->time(), se->device());
            }
            break;
        }
        case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN: {
            HoldGestureEvent *he = static_cast<HoldGestureEvent *>(event.get());
            Q_EMIT he->device()->holdGestureBegin(he->fingerCount(), he->time(), he->device());
            break;
        }
        case LIBINPUT_EVENT_GESTURE_HOLD_END: {
            HoldGestureEvent *he = static_cast<HoldGestureEvent *>(event.get());
            if (he->isCancelled()) {
                Q_EMIT he->device()->holdGestureCancelled(he->time(), he->device());
            } else {
                Q_EMIT he->device()->holdGestureEnd(he->time(), he->device());
            }
            break;
        }
        case LIBINPUT_EVENT_SWITCH_TOGGLE: {
            SwitchEvent *se = static_cast<SwitchEvent *>(event.get());
            Q_EMIT se->device()->switchToggle(se->state(), se->time(), se->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_TOOL_AXIS: {
            auto *tte = static_cast<TabletToolEvent *>(event.get());
            if (libinput_tablet_tool_config_pressure_range_is_available(tte->tool())) {
                tte->device()->setSupportsPressureRange(true);
                libinput_tablet_tool_config_pressure_range_set(tte->tool(), tte->device()->pressureRangeMin(), tte->device()->pressureRangeMax());
            }
            Q_EMIT event->device()->tabletToolAxisEvent(tabletToolPosition(tte),
                                                        tte->device()->pressureCurve().valueForProgress(tte->pressure()),
                                                        tte->xTilt(),
                                                        tte->yTilt(),
                                                        tte->rotation(),
                                                        tte->distance(),
                                                        tte->isTipDown(),
                                                        tte->sliderPosition(),
                                                        getOrCreateTool(tte->tool()),
                                                        tte->time(),
                                                        tte->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY: {
            auto *tte = static_cast<TabletToolEvent *>(event.get());
            if (libinput_tablet_tool_config_pressure_range_is_available(tte->tool())) {
                tte->device()->setSupportsPressureRange(true);
                libinput_tablet_tool_config_pressure_range_set(tte->tool(), tte->device()->pressureRangeMin(), tte->device()->pressureRangeMax());
            }
            Q_EMIT event->device()->tabletToolProximityEvent(tabletToolPosition(tte),
                                                             tte->xTilt(),
                                                             tte->yTilt(),
                                                             tte->rotation(),
                                                             tte->distance(),
                                                             tte->isNearby(),
                                                             tte->sliderPosition(),
                                                             getOrCreateTool(tte->tool()),
                                                             tte->time(),
                                                             tte->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_TOOL_TIP: {
            auto *tte = static_cast<TabletToolEvent *>(event.get());
            if (libinput_tablet_tool_config_pressure_range_is_available(tte->tool())) {
                tte->device()->setSupportsPressureRange(true);
                libinput_tablet_tool_config_pressure_range_set(tte->tool(), tte->device()->pressureRangeMin(), tte->device()->pressureRangeMax());
            }
            Q_EMIT event->device()->tabletToolTipEvent(tabletToolPosition(tte),
                                                       tte->device()->pressureCurve().valueForProgress(tte->pressure()),
                                                       tte->xTilt(),
                                                       tte->yTilt(),
                                                       tte->rotation(),
                                                       tte->distance(),
                                                       tte->isTipDown(),
                                                       tte->sliderPosition(),
                                                       getOrCreateTool(tte->tool()),
                                                       tte->time(),
                                                       tte->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_TOOL_BUTTON: {
            auto *tabletEvent = static_cast<TabletToolButtonEvent *>(event.get());
            Q_EMIT event->device()->tabletToolButtonEvent(tabletEvent->buttonId(),
                                                          tabletEvent->isButtonPressed(),
                                                          getOrCreateTool(tabletEvent->tool()), tabletEvent->time(), tabletEvent->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_PAD_BUTTON: {
            auto *tabletEvent = static_cast<TabletPadButtonEvent *>(event.get());
            Q_EMIT event->device()->tabletPadButtonEvent(tabletEvent->buttonId(),
                                                         tabletEvent->isButtonPressed(),
                                                         tabletEvent->time(), tabletEvent->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_PAD_RING: {
            auto *tabletEvent = static_cast<TabletPadRingEvent *>(event.get());
            tabletEvent->position();
            Q_EMIT event->device()->tabletPadRingEvent(tabletEvent->number(),
                                                       tabletEvent->position(),
                                                       tabletEvent->source() == LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER,
                                                       tabletEvent->time(), tabletEvent->device());
            break;
        }
        case LIBINPUT_EVENT_TABLET_PAD_STRIP: {
            auto *tabletEvent = static_cast<TabletPadStripEvent *>(event.get());
            Q_EMIT event->device()->tabletPadStripEvent(tabletEvent->number(),
                                                        tabletEvent->position(),
                                                        tabletEvent->source() == LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER,
                                                        tabletEvent->time(), tabletEvent->device());
            break;
        }
        default:
            // nothing
            break;
        }
    }
}

void Connection::updateScreens()
{
    QMutexLocker locker(&m_mutex);
    for (auto device : std::as_const(m_devices)) {
        applyScreenToDevice(device);
    }
}

void Connection::applyScreenToDevice(Device *device)
{
#ifndef KWIN_BUILD_TESTING
    QMutexLocker locker(&m_mutex);
    if (!device->isTouch() && !device->isTabletTool()) {
        return;
    }

    Output *deviceOutput = nullptr;
    const QList<Output *> outputs = kwinApp()->outputBackend()->outputs();

    // let's try to find a screen for it
    if (!device->outputName().isEmpty()) {
        // we have an output name, try to find a screen with matching name
        for (Output *output : outputs) {
            if (!output->isEnabled()) {
                continue;
            }
            if (output->name() == device->outputName()) {
                deviceOutput = output;
                break;
            }
        }
    }
    if (!deviceOutput && device->isTouch()) {
        // do we have an internal screen?
        Output *internalOutput = nullptr;
        for (Output *output : outputs) {
            if (!output->isEnabled()) {
                continue;
            }
            if (output->isInternal()) {
                internalOutput = output;
                break;
            }
        }
        auto testScreenMatches = [device](const Output *output) {
            const auto &size = device->size();
            const auto &screenSize = output->physicalSize();
            return std::round(size.width()) == std::round(screenSize.width())
                && std::round(size.height()) == std::round(screenSize.height());
        };
        if (internalOutput && testScreenMatches(internalOutput)) {
            deviceOutput = internalOutput;
        }
        // let's compare all screens for size
        for (Output *output : outputs) {
            if (!output->isEnabled()) {
                continue;
            }
            if (testScreenMatches(output)) {
                deviceOutput = output;
                break;
            }
        }
        if (!deviceOutput) {
            // still not found
            if (internalOutput) {
                // we have an internal id, so let's use that
                deviceOutput = internalOutput;
            } else {
                for (Output *output : outputs) {
                    // just take first screen, we have no clue
                    if (output->isEnabled()) {
                        deviceOutput = output;
                        break;
                    }
                }
            }
        }
    }

    device->setOutput(deviceOutput);

    // TODO: this is currently non-functional even on DRM. Needs orientation() override there.
    device->setOrientation(Qt::PrimaryOrientation);
#endif
}

void Connection::applyDeviceConfig(Device *device)
{
    KConfigGroup defaults = m_config->group(QStringLiteral("Libinput")).group(QStringLiteral("Defaults"));
    if (defaults.isValid()) {
        if (device->isAlphaNumericKeyboard() && defaults.hasGroup(QStringLiteral("Keyboard"))) {
            defaults = defaults.group(QStringLiteral("Keyboard"));
        } else if (device->isTouchpad() && defaults.hasGroup(QStringLiteral("Touchpad"))) {
            // A Touchpad is a Pointer, so we need to check for it before Pointer.
            defaults = defaults.group(QStringLiteral("Touchpad"));
        } else if (device->isPointer() && defaults.hasGroup(QStringLiteral("Pointer"))) {
            defaults = defaults.group(QStringLiteral("Pointer"));
        }

        device->setDefaultConfig(defaults);
    }

    // pass configuration to Device
    device->setConfig(m_config->group(QStringLiteral("Libinput")).group(QString::number(device->vendor())).group(QString::number(device->product())).group(device->name()));
    device->loadConfiguration();
}

void Connection::slotKGlobalSettingsNotifyChange(int type, int arg)
{
    if (type == 3 /**SettingsChanged**/ && arg == 0 /** SETTINGS_MOUSE */) {
        m_config->reparseConfiguration();
        for (auto it = m_devices.constBegin(), end = m_devices.constEnd(); it != end; ++it) {
            if ((*it)->isPointer()) {
                applyDeviceConfig(*it);
            }
        }
    }
}

QStringList Connection::devicesSysNames() const
{
    QStringList sl;
    for (Device *d : std::as_const(m_devices)) {
        sl.append(d->sysName());
    }
    return sl;
}

}
}

#include "connection.moc"

#include "moc_connection.cpp"
