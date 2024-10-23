/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tablet_input.h"
#include "backends/libinput/device.h"
#include "cursorsource.h"
#include "decorations/decoratedclient.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "osd.h"
#include "pointer_input.h"
#include "wayland/tablet_v2.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration2/Decoration>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QHoverEvent>
#include <QWindow>

namespace KWin
{

class SurfaceCursor : public Cursor
{
public:
    explicit SurfaceCursor(TabletToolV2Interface *tool)
        : Cursor()
    {
        setParent(tool);
        connect(tool, &TabletToolV2Interface::cursorChanged, this, [this](const TabletCursorSourceV2 &cursor) {
            if (auto surfaceCursor = std::get_if<TabletSurfaceCursorV2 *>(&cursor)) {
                // If the cursor is unset, fallback to the cross cursor.
                if ((*surfaceCursor) && (*surfaceCursor)->enteredSerial()) {
                    if (!m_surfaceSource) {
                        m_surfaceSource = std::make_unique<SurfaceCursorSource>();
                    }
                    m_surfaceSource->update((*surfaceCursor)->surface(), (*surfaceCursor)->hotspot());
                    setSource(m_surfaceSource.get());
                    return;
                }
            }

            QByteArray shape;
            if (auto shapeCursor = std::get_if<QByteArray>(&cursor)) {
                shape = *shapeCursor;
            } else {
                shape = QByteArrayLiteral("cross");
            }

            static WaylandCursorImage defaultCursor;
            if (!m_shapeSource) {
                m_shapeSource = std::make_unique<ShapeCursorSource>();
            }
            m_shapeSource->setTheme(defaultCursor.theme());
            m_shapeSource->setShape(shape);
            setSource(m_shapeSource.get());
        });
    }

private:
    std::unique_ptr<ShapeCursorSource> m_shapeSource;
    std::unique_ptr<SurfaceCursorSource> m_surfaceSource;
};

TabletInputRedirection::TabletInputRedirection(InputRedirection *parent)
    : InputDeviceHandler(parent)
{
}

TabletInputRedirection::~TabletInputRedirection() = default;

void TabletInputRedirection::init()
{
    Q_ASSERT(!inited());
    setInited(true);
    InputDeviceHandler::init();

    connect(workspace(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
    connect(waylandServer(), &QObject::destroyed, this, [this] {
        setInited(false);
    });

    const auto devices = input()->devices();
    for (InputDevice *device : devices) {
        integrateDevice(device);
    }
    connect(input(), &InputRedirection::deviceAdded, this, &TabletInputRedirection::integrateDevice);
    connect(input(), &InputRedirection::deviceRemoved, this, &TabletInputRedirection::removeDevice);

    auto tabletNextOutput = new QAction(this);
    tabletNextOutput->setProperty("componentName", QStringLiteral("kwin"));
    tabletNextOutput->setText(i18n("Move the tablet to the next output"));
    tabletNextOutput->setObjectName(QStringLiteral("Move Tablet to Next Output"));
    KGlobalAccel::setGlobalShortcut(tabletNextOutput, QList<QKeySequence>());
    connect(tabletNextOutput, &QAction::triggered, this, &TabletInputRedirection::trackNextOutput);
}

static TabletSeatV2Interface *findTabletSeat()
{
    auto server = waylandServer();
    if (!server) {
        return nullptr;
    }
    TabletManagerV2Interface *manager = server->tabletManagerV2();
    return manager->seat(waylandServer()->seat());
}

void TabletInputRedirection::integrateDevice(InputDevice *inputDevice)
{
    auto device = qobject_cast<LibInput::Device *>(inputDevice);
    if (!device || (!device->isTabletTool() && !device->isTabletPad())) {
        return;
    }

    TabletSeatV2Interface *tabletSeat = findTabletSeat();
    if (!tabletSeat) {
        qCCritical(KWIN_CORE) << "Could not find tablet seat";
        return;
    }
    struct udev_device *const udev_device = libinput_device_get_udev_device(device->device());
    const char *devnode = udev_device_get_syspath(udev_device);

    auto deviceGroup = libinput_device_get_device_group(device->device());
    auto tablet = static_cast<TabletV2Interface *>(libinput_device_group_get_user_data(deviceGroup));
    if (!tablet) {
        tablet = tabletSeat->addTablet(device->vendor(), device->product(), device->sysName(), device->name(), {QString::fromUtf8(devnode)});
        libinput_device_group_set_user_data(deviceGroup, tablet);
    }

    if (device->isTabletPad()) {
        const int buttonsCount = libinput_device_tablet_pad_get_num_buttons(device->device());
        const int ringsCount = libinput_device_tablet_pad_get_num_rings(device->device());
        const int stripsCount = libinput_device_tablet_pad_get_num_strips(device->device());
        const int modes = libinput_device_tablet_pad_get_num_mode_groups(device->device());

        auto firstGroup = libinput_device_tablet_pad_get_mode_group(device->device(), 0);
        tabletSeat->addTabletPad(device->sysName(), device->name(), {QString::fromUtf8(devnode)}, buttonsCount, ringsCount, stripsCount, modes, libinput_tablet_pad_mode_group_get_mode(firstGroup), tablet);
    }
}

void TabletInputRedirection::removeDevice(InputDevice *inputDevice)
{
    auto device = qobject_cast<LibInput::Device *>(inputDevice);
    if (device) {
        auto deviceGroup = libinput_device_get_device_group(device->device());
        libinput_device_group_set_user_data(deviceGroup, nullptr);

        TabletSeatV2Interface *tabletSeat = findTabletSeat();
        if (tabletSeat) {
            tabletSeat->removeDevice(device->sysName());
        } else {
            qCCritical(KWIN_CORE) << "Could not find tablet to remove" << device->sysName();
        }
    }
}

void TabletInputRedirection::trackNextOutput()
{
    const auto outputs = workspace()->outputs();
    if (outputs.isEmpty()) {
        return;
    }

    int tabletToolCount = 0;
    InputDevice *changedDevice = nullptr;
    const auto devices = input()->devices();
    for (const auto device : devices) {
        if (!device->isTabletTool()) {
            continue;
        }

        tabletToolCount++;
        if (device->outputName().isEmpty()) {
            device->setOutputName(outputs.constFirst()->name());
            changedDevice = device;
            continue;
        }

        auto it = std::find_if(outputs.begin(), outputs.end(), [device](const auto &output) {
            return output->name() == device->outputName();
        });
        ++it;
        auto nextOutput = it == outputs.end() ? outputs.first() : *it;
        device->setOutputName(nextOutput->name());
        changedDevice = device;
    }
    const QString message = tabletToolCount == 1 ? i18n("Tablet moved to %1", changedDevice->outputName()) : i18n("Tablets switched outputs");
    OSD::show(message, QStringLiteral("input-tablet"), 5000);
}

static TabletToolV2Interface::Type getType(const TabletToolId &tabletToolId)
{
    using Type = TabletToolV2Interface::Type;
    switch (tabletToolId.m_toolType) {
    case InputRedirection::Pen:
        return Type::Pen;
    case InputRedirection::Eraser:
        return Type::Eraser;
    case InputRedirection::Brush:
        return Type::Brush;
    case InputRedirection::Pencil:
        return Type::Pencil;
    case InputRedirection::Airbrush:
        return Type::Airbrush;
    case InputRedirection::Finger:
        return Type::Finger;
    case InputRedirection::Mouse:
        return Type::Mouse;
    case InputRedirection::Lens:
        return Type::Lens;
    case InputRedirection::Totem:
        return Type::Totem;
    }
    return Type::Pen;
}

TabletToolV2Interface *TabletInputRedirection::ensureTabletTool(const TabletToolId &tabletToolId)
{
    TabletSeatV2Interface *tabletSeat = findTabletSeat();
    if (auto tool = tabletSeat->toolByHardwareSerial(tabletToolId.m_serialId, getType(tabletToolId))) {
        return tool;
    }

    const auto f = [](InputRedirection::Capability cap) {
        switch (cap) {
        case InputRedirection::Tilt:
            return TabletToolV2Interface::Tilt;
        case InputRedirection::Pressure:
            return TabletToolV2Interface::Pressure;
        case InputRedirection::Distance:
            return TabletToolV2Interface::Distance;
        case InputRedirection::Rotation:
            return TabletToolV2Interface::Rotation;
        case InputRedirection::Slider:
            return TabletToolV2Interface::Slider;
        case InputRedirection::Wheel:
            return TabletToolV2Interface::Wheel;
        }
        return TabletToolV2Interface::Wheel;
    };
    QList<TabletToolV2Interface::Capability> ifaceCapabilities;
    ifaceCapabilities.resize(tabletToolId.m_capabilities.size());
    std::transform(tabletToolId.m_capabilities.constBegin(), tabletToolId.m_capabilities.constEnd(), ifaceCapabilities.begin(), f);

    TabletToolV2Interface *tool = tabletSeat->addTool(getType(tabletToolId), tabletToolId.m_serialId, tabletToolId.m_uniqueId, ifaceCapabilities, tabletToolId.deviceSysName);

    const auto cursor = new SurfaceCursor(tool);
    Cursors::self()->addCursor(cursor);
    m_cursorByTool[tool] = cursor;

    return tool;
}

void TabletInputRedirection::tabletToolEvent(KWin::InputRedirection::TabletEventType type, const QPointF &pos,
                                             qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                                             bool tipNear, const TabletToolId &tabletToolId,
                                             std::chrono::microseconds time,
                                             InputDevice *device)
{
    if (!inited()) {
        return;
    }
    input()->setLastInputHandler(this);
    m_lastPosition = pos;

    QEvent::Type t;
    switch (type) {
    case InputRedirection::Axis:
        t = QEvent::TabletMove;
        break;
    case InputRedirection::Tip:
        t = tipDown ? QEvent::TabletPress : QEvent::TabletRelease;
        break;
    case InputRedirection::Proximity:
        t = tipNear ? QEvent::TabletEnterProximity : QEvent::TabletLeaveProximity;
        break;
    }

    auto tool = ensureTabletTool(tabletToolId);
    switch (t) {
    case QEvent::TabletEnterProximity:
    case QEvent::TabletPress:
    case QEvent::TabletMove:
        m_cursorByTool[tool]->setPos(pos);
        break;
    default:
        break;
    }

    update();
    workspace()->setActiveOutput(pos);

    const auto button = m_tipDown ? Qt::LeftButton : Qt::NoButton;

    // TODO: Not correct, but it should work fine. In long term, we need to stop using QTabletEvent.
    const QPointingDevice *dev = QPointingDevice::primaryPointingDevice();
    TabletEvent ev(t, dev, pos, pos, pressure,
                   xTilt, yTilt,
                   0, // tangentialPressure
                   rotation,
                   0, // z
                   Qt::NoModifier, button, button, tabletToolId, device);

    ev.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    input()->processSpies(std::bind(&InputEventSpy::tabletToolEvent, std::placeholders::_1, &ev));
    input()->processFilters(
        std::bind(&InputEventFilter::tabletToolEvent, std::placeholders::_1, &ev));

    m_tipDown = tipDown;
    m_tipNear = tipNear;
}

void KWin::TabletInputRedirection::tabletToolButtonEvent(uint button, bool isPressed, const TabletToolId &tabletToolId, std::chrono::microseconds time, InputDevice *device)
{
    TabletToolButtonEvent event{
        .device = device,
        .button = button,
        .pressed = isPressed,
        .tabletToolId = tabletToolId,
        .time = time,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletToolButtonEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletToolButtonEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadButtonEvent(uint button, bool isPressed, const TabletPadId &tabletPadId, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadButtonEvent event{
        .device = device,
        .button = button,
        .pressed = isPressed,
        .tabletPadId = tabletPadId,
        .time = time,
    };
    input()->processSpies(std::bind(&InputEventSpy::tabletPadButtonEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadButtonEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadStripEvent event{
        .device = device,
        .number = number,
        .position = position,
        .isFinger = isFinger,
        .tabletPadId = tabletPadId,
        .time = time,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletPadStripEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadStripEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadRingEvent event{
        .device = device,
        .number = number,
        .position = position,
        .isFinger = isFinger,
        .tabletPadId = tabletPadId,
        .time = time,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletPadRingEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadRingEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

bool TabletInputRedirection::focusUpdatesBlocked()
{
    return input()->isSelectingWindow();
}

void TabletInputRedirection::cleanupDecoration(Decoration::DecoratedClientImpl *old,
                                               Decoration::DecoratedClientImpl *now)
{
    disconnect(m_decorationGeometryConnection);
    m_decorationGeometryConnection = QMetaObject::Connection();

    disconnect(m_decorationDestroyedConnection);
    m_decorationDestroyedConnection = QMetaObject::Connection();

    if (old) {
        // send leave event to old decoration
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(old->decoration(), &event);
    }
    if (!now) {
        // left decoration
        return;
    }

    const auto pos = m_lastPosition - now->window()->pos();
    QHoverEvent event(QEvent::HoverEnter, pos, pos);
    QCoreApplication::instance()->sendEvent(now->decoration(), &event);
    now->window()->processDecorationMove(pos, m_lastPosition);

    m_decorationGeometryConnection = connect(
        decoration()->window(), &Window::frameGeometryChanged, this, [this]() {
            // ensure maximize button gets the leave event when maximizing/restore a window, see BUG 385140
            const auto oldDeco = decoration();
            update();
            if (oldDeco && oldDeco == decoration() && !decoration()->window()->isInteractiveMove() && !decoration()->window()->isInteractiveResize()) {
                // position of window did not change, we need to send HoverMotion manually
                const QPointF p = m_lastPosition - decoration()->window()->pos();
                QHoverEvent event(QEvent::HoverMove, p, p);
                QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
            }
        },
        Qt::QueuedConnection);

    // if our decoration gets destroyed whilst it has focus, we pass focus on to the same client
    m_decorationDestroyedConnection = connect(now, &QObject::destroyed, this, &TabletInputRedirection::update, Qt::QueuedConnection);
}

void TabletInputRedirection::focusUpdate(Window *focusOld, Window *focusNow)
{
    // This method is left blank intentionally.
}

}

#include "moc_tablet_input.cpp"
