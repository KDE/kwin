/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tablet_input.h"
#include "core/inputdevice.h"
#include "core/outputbackend.h"
#include "cursorsource.h"
#include "decorations/decoratedwindow.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "osd.h"
#include "pointer_input.h"
#include "wayland/display.h"
#include "wayland/seat.h"
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

static QPointF confineToBoundingBox(const QPointF &pos, const QRectF &boundingBox)
{
    return QPointF(
        std::clamp(pos.x(), boundingBox.left(), boundingBox.right() - 1.0),
        std::clamp(pos.y(), boundingBox.top(), boundingBox.bottom() - 1.0));
}

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
    tabletNextOutput->setObjectName(QStringLiteral("Move Tablet to Next LogicalOutput"));
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

    // Ensure that the cursor position to set to the center of the screen in relative (mouse) mode
    if (device->tabletToolIsRelative()) {
        if (auto output = workspace()->activeOutput(); output != nullptr) {
            m_lastPosition = output->geometry().center();
        }
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
    m_cursorByTool[tool]->setPos(m_lastPosition);

    update();
    workspace()->setActiveOutput(m_lastPosition);

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

    input()->processSpies(&InputEventSpy::tabletToolAxisEvent, &ev);
    input()->processFilters(&InputEventFilter::tabletToolAxisEvent, &ev);
    input()->setLastInputHandler(this);
}

void TabletInputRedirection::tabletToolAxisEventRelative(const QPointF &delta,
                                                         qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, InputDeviceTabletTool *tool,
                                                         std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }

    ensureTabletTool(tool);

    m_lastPosition += delta;

    // Make sure pointer doesn't go outside of the screens range
    LogicalOutput *output = Workspace::self()->outputAt(m_lastPosition);
    m_lastPosition = confineToBoundingBox(m_lastPosition, output->geometryF());

    m_cursorByTool[tool]->setPos(m_lastPosition);

    update();
    workspace()->setActiveOutput(output);

    TabletToolAxisEvent ev{
        .device = device,
        .rotation = rotation,
        .position = m_lastPosition,
        .buttons = tipDown ? Qt::LeftButton : Qt::NoButton,
        .pressure = pressure,
        .sliderPosition = sliderPosition,
        .xTilt = xTilt,
        .yTilt = yTilt,
        .distance = distance,
        .timestamp = time,
        .tool = tool,
    };

    input()->processSpies(&InputEventSpy::tabletToolAxisEvent, &ev);
    input()->processFilters(&InputEventFilter::tabletToolAxisEvent, &ev);
    input()->setLastInputHandler(this);
}

void TabletInputRedirection::tabletToolProximityEvent(const QPointF &pos, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipNear, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }

    ensureTabletTool(tool);

    if (!device->tabletToolIsRelative()) {
        m_lastPosition = pos;
    }

    if (tipNear) {
        m_cursorByTool[tool]->setPos(m_lastPosition);
    }

    update();
    workspace()->setActiveOutput(m_lastPosition);

    TabletToolProximityEvent ev{
        .type = tipNear ? TabletToolProximityEvent::EnterProximity : TabletToolProximityEvent::LeaveProximity,
        .device = device,
        .rotation = rotation,
        .position = m_lastPosition,
        .sliderPosition = sliderPosition,
        .xTilt = xTilt,
        .yTilt = yTilt,
        .distance = distance,
        .timestamp = time,
        .tool = tool,
    };

    input()->processSpies(&InputEventSpy::tabletToolProximityEvent, &ev);
    input()->processFilters(&InputEventFilter::tabletToolProximityEvent, &ev);
    input()->setLastInputHandler(this);
}

void TabletInputRedirection::tabletToolTipEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }

    ensureTabletTool(tool);

    if (!device->tabletToolIsRelative()) {
        m_lastPosition = pos;
    }

    if (tipDown) {
        m_cursorByTool[tool]->setPos(m_lastPosition);
    }

    if (!tipDown) {
        m_tipDown = false;
    }

    update();

    if (tipDown) {
        m_tipDown = true;
    }

    workspace()->setActiveOutput(m_lastPosition);

    TabletToolTipEvent ev{
        .type = tipDown ? TabletToolTipEvent::Press : TabletToolTipEvent::Release,
        .device = device,
        .rotation = rotation,
        .position = m_lastPosition,
        .pressure = pressure,
        .sliderPosition = sliderPosition,
        .xTilt = xTilt,
        .yTilt = yTilt,
        .distance = distance,
        .timestamp = time,
        .tool = tool,
    };

    input()->processSpies(&InputEventSpy::tabletToolTipEvent, &ev);
    input()->processFilters(&InputEventFilter::tabletToolTipEvent, &ev);
    input()->setLastInputHandler(this);
    if (tipDown) {
        input()->setLastInteractionSerial(waylandServer()->seat()->display()->serial());
        if (auto f = focus()) {
            f->setLastUsageSerial(waylandServer()->seat()->display()->serial());
        }
    }
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

    m_buttonDown = isPressed;

    input()->processSpies(&InputEventSpy::tabletToolButtonEvent, &event);
    input()->processFilters(&InputEventFilter::tabletToolButtonEvent, &event);
    input()->setLastInputHandler(this);
    if (isPressed) {
        input()->setLastInteractionSerial(waylandServer()->seat()->display()->serial());
        if (auto f = focus()) {
            f->setLastUsageSerial(waylandServer()->seat()->display()->serial());
        }
    }
}

void KWin::TabletInputRedirection::tabletPadButtonEvent(uint button, bool isPressed, quint32 group, quint32 mode, bool isModeSwitch, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadButtonEvent event{
        .device = device,
        .button = button,
        .pressed = isPressed,
        .group = group,
        .mode = mode,
        .isModeSwitch = isModeSwitch,
        .time = time,
    };
    input()->processSpies(&InputEventSpy::tabletPadButtonEvent, &event);
    input()->processFilters(&InputEventFilter::tabletPadButtonEvent, &event);
    input()->setLastInputHandler(this);
    if (isPressed) {
        input()->setLastInteractionSerial(waylandServer()->seat()->display()->serial());
        if (auto f = focus()) {
            f->setLastUsageSerial(waylandServer()->seat()->display()->serial());
        }
    }
}

void KWin::TabletInputRedirection::tabletPadStripEvent(int number, qreal position, bool isFinger, quint32 group, quint32 mode, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadStripEvent event{
        .device = device,
        .number = number,
        .position = position,
        .isFinger = isFinger,
        .group = group,
        .mode = mode,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::tabletPadStripEvent, &event);
    input()->processFilters(&InputEventFilter::tabletPadStripEvent, &event);
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadRingEvent(int number, qreal position, bool isFinger, quint32 group, quint32 mode, std::chrono::microseconds time, InputDevice *device)
{
    TabletPadRingEvent event{
        .device = device,
        .number = number,
        .position = position,
        .isFinger = isFinger,
        .group = group,
        .mode = mode,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::tabletPadRingEvent, &event);
    input()->processFilters(&InputEventFilter::tabletPadRingEvent, &event);
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadDialEvent(int number, double delta, quint32 group, std::chrono::microseconds time, InputDevice *device)
{

    TabletPadDialEvent event{
        .device = device,
        .number = number,
        .delta = delta,
        .group = group,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::tabletPadDialEvent, &event);
    input()->processFilters(&InputEventFilter::tabletPadDialEvent, &event);
    input()->setLastInputHandler(this);
}

bool TabletInputRedirection::focusUpdatesBlocked()
{
    return input()->isSelectingWindow() || m_tipDown || m_buttonDown;
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
