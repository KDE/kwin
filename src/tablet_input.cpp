/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tablet_input.h"
#include "core/inputdevice.h"
#include "cursorsource.h"
#include "decorations/decoratedwindow.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "osd.h"
#include "pointer_input.h"
#include "wayland/tablet_v2.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration3/Decoration>
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

void TabletInputRedirection::integrateDevice(InputDevice *device)
{
    TabletSeatV2Interface *tabletSeat = findTabletSeat();
    if (!tabletSeat) {
        qCCritical(KWIN_CORE) << "Could not find tablet seat";
        return;
    }

    if (device->isTabletTool()) {
        tabletSeat->addTablet(device);
    }

    if (device->isTabletPad()) {
        tabletSeat->addPad(device);
    }
}

void TabletInputRedirection::removeDevice(InputDevice *device)
{
    TabletSeatV2Interface *tabletSeat = findTabletSeat();
    if (tabletSeat) {
        tabletSeat->remove(device);
    } else {
        qCCritical(KWIN_CORE) << "Could not find tablet to remove" << device->name();
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

void TabletInputRedirection::ensureTabletTool(InputDeviceTabletTool *device)
{
    TabletSeatV2Interface *tabletSeat = findTabletSeat();
    if (tabletSeat->tool(device)) {
        return;
    }

    TabletToolV2Interface *tool = tabletSeat->addTool(device);

    const auto cursor = new SurfaceCursor(tool);
    Cursors::self()->addCursor(cursor);
    m_cursorByTool[device] = cursor;
}

void TabletInputRedirection::tabletToolAxisEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }

    ensureTabletTool(tool);

    m_lastPosition = pos;
    m_cursorByTool[tool]->setPos(pos);

    update();
    workspace()->setActiveOutput(pos);

    TabletToolAxisEvent ev{
        .device = device,
        .rotation = rotation,
        .position = pos,
        .buttons = tipDown ? Qt::LeftButton : Qt::NoButton,
        .pressure = pressure,
        .sliderPosition = sliderPosition,
        .xTilt = xTilt,
        .yTilt = yTilt,
        .distance = distance,
        .timestamp = time,
        .tool = tool,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletToolAxisEvent, std::placeholders::_1, &ev));
    input()->processFilters(std::bind(&InputEventFilter::tabletToolAxisEvent, std::placeholders::_1, &ev));
    input()->setLastInputHandler(this);
}

void TabletInputRedirection::tabletToolProximityEvent(const QPointF &pos, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipNear, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }

    ensureTabletTool(tool);

    m_lastPosition = pos;
    if (tipNear) {
        m_cursorByTool[tool]->setPos(pos);
    }

    update();
    workspace()->setActiveOutput(pos);

    TabletToolProximityEvent ev{
        .type = tipNear ? TabletToolProximityEvent::EnterProximity : TabletToolProximityEvent::LeaveProximity,
        .device = device,
        .rotation = rotation,
        .position = pos,
        .sliderPosition = sliderPosition,
        .xTilt = xTilt,
        .yTilt = yTilt,
        .distance = distance,
        .timestamp = time,
        .tool = tool,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletToolProximityEvent, std::placeholders::_1, &ev));
    input()->processFilters(std::bind(&InputEventFilter::tabletToolProximityEvent, std::placeholders::_1, &ev));
    input()->setLastInputHandler(this);
}

void TabletInputRedirection::tabletToolTipEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }

    ensureTabletTool(tool);

    m_lastPosition = pos;
    if (tipDown) {
        m_cursorByTool[tool]->setPos(pos);
    }

    update();
    workspace()->setActiveOutput(pos);

    TabletToolTipEvent ev{
        .type = tipDown ? TabletToolTipEvent::Press : TabletToolTipEvent::Release,
        .device = device,
        .rotation = rotation,
        .position = pos,
        .buttons = tipDown ? Qt::LeftButton : Qt::NoButton,
        .pressure = pressure,
        .sliderPosition = sliderPosition,
        .xTilt = xTilt,
        .yTilt = yTilt,
        .distance = distance,
        .timestamp = time,
        .tool = tool,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletToolTipEvent, std::placeholders::_1, &ev));
    input()->processFilters(std::bind(&InputEventFilter::tabletToolTipEvent, std::placeholders::_1, &ev));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletToolButtonEvent(uint button, bool isPressed, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device)
{
    TabletToolButtonEvent event{
        .device = device,
        .button = button,
        .pressed = isPressed,
        .tool = tool,
        .time = time,
    };

    ensureTabletTool(tool);

    input()->processSpies(std::bind(&InputEventSpy::tabletToolButtonEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletToolButtonEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadButtonEvent(uint button, bool isPressed, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadButtonEvent event{
        .device = device,
        .button = button,
        .pressed = isPressed,
        .time = time,
    };
    input()->processSpies(std::bind(&InputEventSpy::tabletPadButtonEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadButtonEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadStripEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadStripEvent event{
        .device = device,
        .number = number,
        .position = position,
        .isFinger = isFinger,
        .time = time,
    };

    input()->processSpies(std::bind(&InputEventSpy::tabletPadStripEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadStripEvent, std::placeholders::_1, &event));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadRingEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadRingEvent event{
        .device = device,
        .number = number,
        .position = position,
        .isFinger = isFinger,
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

void TabletInputRedirection::cleanupDecoration(Decoration::DecoratedWindowImpl *old,
                                               Decoration::DecoratedWindowImpl *now)
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
