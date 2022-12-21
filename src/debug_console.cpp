/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "debug_console.h"
#include "composite.h"
#include "core/inputdevice.h"
#include "input_event.h"
#include "internalwindow.h"
#include "keyboard_input.h"
#include "main.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "unmanaged.h"
#include "utils/filedescriptor.h"
#include "utils/subsurfacemonitor.h"
#include "wayland/abstract_data_source.h"
#include "wayland/clientconnection.h"
#include "wayland/datacontrolsource_v1_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/display.h"
#include "wayland/primaryselectionsource_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/subcompositor_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "workspace.h"
#include "x11window.h"
#include "xkb.h"
#include <cerrno>
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include "ui_debug_console.h"

// frameworks
#include <KLocalizedString>
#include <NETWM>
// Qt
#include <QFutureWatcher>
#include <QMetaProperty>
#include <QMetaType>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSortFilterProxyModel>
#include <QtConcurrentRun>

#include <wayland-server-core.h>

// xkb
#include <xkbcommon/xkbcommon.h>

#include <fcntl.h>
#include <functional>
#include <sys/poll.h>
#include <unistd.h>

namespace KWin
{

static QString tableHeaderRow(const QString &title)
{
    return QStringLiteral("<tr><th colspan=\"2\">%1</th></tr>").arg(title);
}

template<typename T>
static QString tableRow(const QString &title, const T &argument)
{
    return QStringLiteral("<tr><td>%1</td><td>%2</td></tr>").arg(title).arg(argument);
}

static QString timestampRow(uint32_t timestamp)
{
    return tableRow(i18n("Timestamp"), timestamp);
}

static QString timestampRow(std::chrono::microseconds timestamp)
{
    return tableRow(i18n("Timestamp"), std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count());
}

static QString timestampRowUsec(std::chrono::microseconds timestamp)
{
    return tableRow(i18n("Timestamp (µsec)"), timestamp.count());
}

static QString timestampRowUsec(uint64_t timestamp)
{
    return tableRow(i18n("Timestamp (µsec)"), timestamp);
}

static QString buttonToString(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:
        return i18nc("A mouse button", "Left");
    case Qt::RightButton:
        return i18nc("A mouse button", "Right");
    case Qt::MiddleButton:
        return i18nc("A mouse button", "Middle");
    case Qt::BackButton:
        return i18nc("A mouse button", "Back");
    case Qt::ForwardButton:
        return i18nc("A mouse button", "Forward");
    case Qt::TaskButton:
        return i18nc("A mouse button", "Task");
    case Qt::ExtraButton4:
        return i18nc("A mouse button", "Extra Button 4");
    case Qt::ExtraButton5:
        return i18nc("A mouse button", "Extra Button 5");
    case Qt::ExtraButton6:
        return i18nc("A mouse button", "Extra Button 6");
    case Qt::ExtraButton7:
        return i18nc("A mouse button", "Extra Button 7");
    case Qt::ExtraButton8:
        return i18nc("A mouse button", "Extra Button 8");
    case Qt::ExtraButton9:
        return i18nc("A mouse button", "Extra Button 9");
    case Qt::ExtraButton10:
        return i18nc("A mouse button", "Extra Button 10");
    case Qt::ExtraButton11:
        return i18nc("A mouse button", "Extra Button 11");
    case Qt::ExtraButton12:
        return i18nc("A mouse button", "Extra Button 12");
    case Qt::ExtraButton13:
        return i18nc("A mouse button", "Extra Button 13");
    case Qt::ExtraButton14:
        return i18nc("A mouse button", "Extra Button 14");
    case Qt::ExtraButton15:
        return i18nc("A mouse button", "Extra Button 15");
    case Qt::ExtraButton16:
        return i18nc("A mouse button", "Extra Button 16");
    case Qt::ExtraButton17:
        return i18nc("A mouse button", "Extra Button 17");
    case Qt::ExtraButton18:
        return i18nc("A mouse button", "Extra Button 18");
    case Qt::ExtraButton19:
        return i18nc("A mouse button", "Extra Button 19");
    case Qt::ExtraButton20:
        return i18nc("A mouse button", "Extra Button 20");
    case Qt::ExtraButton21:
        return i18nc("A mouse button", "Extra Button 21");
    case Qt::ExtraButton22:
        return i18nc("A mouse button", "Extra Button 22");
    case Qt::ExtraButton23:
        return i18nc("A mouse button", "Extra Button 23");
    case Qt::ExtraButton24:
        return i18nc("A mouse button", "Extra Button 24");
    default:
        return QString();
    }
}

static QString deviceRow(InputDevice *device)
{
    if (!device) {
        return tableRow(i18n("Input Device"), i18nc("The input device of the event is not known", "Unknown"));
    }
    return tableRow(i18n("Input Device"), QStringLiteral("%1 (%2)").arg(device->name(), device->sysName()));
}

static QString buttonsToString(Qt::MouseButtons buttons)
{
    QString ret;
    for (uint i = 1; i < Qt::ExtraButton24; i = i << 1) {
        if (buttons & i) {
            ret.append(buttonToString(Qt::MouseButton(uint(buttons) & i)));
            ret.append(QStringLiteral(" "));
        }
    };
    return ret.trimmed();
}

static const QString s_hr = QStringLiteral("<hr/>");
static const QString s_tableStart = QStringLiteral("<table>");
static const QString s_tableEnd = QStringLiteral("</table>");

DebugConsoleFilter::DebugConsoleFilter(QTextEdit *textEdit)
    : InputEventSpy()
    , m_textEdit(textEdit)
{
}

DebugConsoleFilter::~DebugConsoleFilter() = default;

void DebugConsoleFilter::pointerEvent(MouseEvent *event)
{
    QString text = s_hr;
    const QString timestamp = timestampRow(event->timestamp());

    text.append(s_tableStart);
    switch (event->type()) {
    case QEvent::MouseMove: {
        text.append(tableHeaderRow(i18nc("A mouse pointer motion event", "Pointer Motion")));
        text.append(deviceRow(event->device()));
        text.append(timestamp);
        text.append(timestampRowUsec(event->timestamp()));
        if (!event->delta().isNull()) {
            text.append(tableRow(i18nc("The relative mouse movement", "Delta"),
                                 QStringLiteral("%1/%2").arg(event->delta().x()).arg(event->delta().y())));
        }
        if (!event->deltaUnaccelerated().isNull()) {
            text.append(tableRow(i18nc("The relative mouse movement", "Delta (not accelerated)"),
                                 QStringLiteral("%1/%2").arg(event->deltaUnaccelerated().x()).arg(event->deltaUnaccelerated().y())));
        }
        text.append(tableRow(i18nc("The global mouse pointer position", "Global Position"), QStringLiteral("%1/%2").arg(event->pos().x()).arg(event->pos().y())));
        break;
    }
    case QEvent::MouseButtonPress:
        text.append(tableHeaderRow(i18nc("A mouse pointer button press event", "Pointer Button Press")));
        text.append(deviceRow(event->device()));
        text.append(timestamp);
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Button"), buttonToString(event->button())));
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Native Button code"), event->nativeButton()));
        text.append(tableRow(i18nc("All currently pressed buttons in a mouse press/release event", "Pressed Buttons"), buttonsToString(event->buttons())));
        break;
    case QEvent::MouseButtonRelease:
        text.append(tableHeaderRow(i18nc("A mouse pointer button release event", "Pointer Button Release")));
        text.append(deviceRow(event->device()));
        text.append(timestamp);
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Button"), buttonToString(event->button())));
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Native Button code"), event->nativeButton()));
        text.append(tableRow(i18nc("All currently pressed buttons in a mouse press/release event", "Pressed Buttons"), buttonsToString(event->buttons())));
        break;
    default:
        break;
    }
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::wheelEvent(WheelEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A mouse pointer axis (wheel) event", "Pointer Axis")));
    text.append(deviceRow(event->device()));
    text.append(timestampRow(event->timestamp()));
    const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
    text.append(tableRow(i18nc("The orientation of a pointer axis event", "Orientation"),
                         orientation == Qt::Horizontal ? i18nc("An orientation of a pointer axis event", "Horizontal")
                                                       : i18nc("An orientation of a pointer axis event", "Vertical")));
    text.append(tableRow(i18nc("The angle delta of a pointer axis event", "Delta"),
                         orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::keyEvent(KeyEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);

    switch (event->type()) {
    case QEvent::KeyPress:
        text.append(tableHeaderRow(i18nc("A key press event", "Key Press")));
        break;
    case QEvent::KeyRelease:
        text.append(tableHeaderRow(i18nc("A key release event", "Key Release")));
        break;
    default:
        break;
    }
    text.append(deviceRow(event->device()));
    auto modifiersToString = [event] {
        QString ret;
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            ret.append(i18nc("A keyboard modifier", "Shift"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            ret.append(i18nc("A keyboard modifier", "Control"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers().testFlag(Qt::AltModifier)) {
            ret.append(i18nc("A keyboard modifier", "Alt"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers().testFlag(Qt::MetaModifier)) {
            ret.append(i18nc("A keyboard modifier", "Meta"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers().testFlag(Qt::KeypadModifier)) {
            ret.append(i18nc("A keyboard modifier", "Keypad"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers().testFlag(Qt::GroupSwitchModifier)) {
            ret.append(i18nc("A keyboard modifier", "Group-switch"));
            ret.append(QStringLiteral(" "));
        }
        return ret;
    };
    text.append(timestampRow(event->timestamp()));
    text.append(tableRow(i18nc("Whether the event is an automatic key repeat", "Repeat"), event->isAutoRepeat()));

    const auto keyMetaObject = Qt::qt_getEnumMetaObject(Qt::Key());
    const auto enumerator = keyMetaObject->enumerator(keyMetaObject->indexOfEnumerator("Key"));
    text.append(tableRow(i18nc("The code as read from the input device", "Scan code"), event->nativeScanCode()));
    text.append(tableRow(i18nc("Key according to Qt", "Qt::Key code"),
                         enumerator.valueToKey(event->key())));
    text.append(tableRow(i18nc("The translated code to an Xkb symbol", "Xkb symbol"), event->nativeVirtualKey()));
    text.append(tableRow(i18nc("The translated code interpreted as text", "Utf8"), event->text()));
    text.append(tableRow(i18nc("The currently active modifiers", "Modifiers"), modifiersToString()));

    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A touch down event", "Touch down")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("The id of the touch point in the touch event", "Point identifier"), id));
    text.append(tableRow(i18nc("The global position of the touch point", "Global position"),
                         QStringLiteral("%1/%2").arg(pos.x()).arg(pos.y())));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A touch motion event", "Touch Motion")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("The id of the touch point in the touch event", "Point identifier"), id));
    text.append(tableRow(i18nc("The global position of the touch point", "Global position"),
                         QStringLiteral("%1/%2").arg(pos.x()).arg(pos.y())));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchUp(qint32 id, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A touch up event", "Touch Up")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("The id of the touch point in the touch event", "Point identifier"), id));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture is started", "Pinch start")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("Number of fingers in this pinch gesture", "Finger count"), fingerCount));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture is updated", "Pinch update")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("Current scale in pinch gesture", "Scale"), scale));
    text.append(tableRow(i18nc("Current angle in pinch gesture", "Angle delta"), angleDelta));
    text.append(tableRow(i18nc("Current delta in pinch gesture", "Delta x"), delta.x()));
    text.append(tableRow(i18nc("Current delta in pinch gesture", "Delta y"), delta.y()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureEnd(std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture ended", "Pinch end")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureCancelled(std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture got cancelled", "Pinch cancelled")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture is started", "Swipe start")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("Number of fingers in this swipe gesture", "Finger count"), fingerCount));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture is updated", "Swipe update")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("Current delta in swipe gesture", "Delta x"), delta.x()));
    text.append(tableRow(i18nc("Current delta in swipe gesture", "Delta y"), delta.y()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureEnd(std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture ended", "Swipe end")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureCancelled(std::chrono::microseconds time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture got cancelled", "Swipe cancelled")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::switchEvent(SwitchEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A hardware switch (e.g. notebook lid) got toggled", "Switch toggled")));
    text.append(timestampRow(event->timestamp()));
    text.append(timestampRowUsec(event->timestamp()));
    text.append(deviceRow(event->device()));
    QString switchName;
    if (event->device()->isLidSwitch()) {
        switchName = i18nc("Name of a hardware switch", "Notebook lid");
    } else if (event->device()->isTabletModeSwitch()) {
        switchName = i18nc("Name of a hardware switch", "Tablet mode");
    }
    text.append(tableRow(i18nc("A hardware switch", "Switch"), switchName));
    QString switchState;
    switch (event->state()) {
    case SwitchEvent::State::Off:
        switchState = i18nc("The hardware switch got turned off", "Off");
        break;
    case SwitchEvent::State::On:
        switchState = i18nc("The hardware switch got turned on", "On");
        break;
    default:
        Q_UNREACHABLE();
    }
    text.append(tableRow(i18nc("State of a hardware switch (on/off)", "State"), switchState));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletToolEvent(TabletEvent *event)
{
    QString typeString;
    {
        QDebug d(&typeString);
        d << event->type();
    }

    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Tool"))
        + tableRow(i18n("EventType"), typeString)
        + tableRow(i18n("Position"),
                   QStringLiteral("%1,%2").arg(event->pos().x()).arg(event->pos().y()))
        + tableRow(i18n("Tilt"),
                   QStringLiteral("%1,%2").arg(event->xTilt()).arg(event->yTilt()))
        + tableRow(i18n("Rotation"), QString::number(event->rotation()))
        + tableRow(i18n("Pressure"), QString::number(event->pressure()))
        + tableRow(i18n("Buttons"), QString::number(event->buttons()))
        + tableRow(i18n("Modifiers"), QString::number(event->modifiers()))
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Tool Button"))
        + tableRow(i18n("Button"), button)
        + tableRow(i18n("Pressed"), pressed)
        + tableRow(i18n("Tablet"), qHash(tabletToolId.m_deviceGroupData))
        + timestampRow(time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    QString text = s_hr + s_tableStart
        + tableHeaderRow(i18n("Tablet Pad Button"))
        + tableRow(i18n("Button"), button)
        + tableRow(i18n("Pressed"), pressed)
        + tableRow(i18n("Tablet"), qHash(tabletPadId.data))
        + timestampRow(time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Pad Strip"))
        + tableRow(i18n("Number"), number)
        + tableRow(i18n("Position"), position)
        + tableRow(i18n("isFinger"), isFinger)
        + tableRow(i18n("Tablet"), qHash(tabletPadId.data))
        + timestampRow(time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Pad Ring"))
        + tableRow(i18n("Number"), number)
        + tableRow(i18n("Position"), position)
        + tableRow(i18n("isFinger"), isFinger)
        + tableRow(i18n("Tablet"), qHash(tabletPadId.data))
        + timestampRow(time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

static QString sourceString(const KWaylandServer::AbstractDataSource *const source)
{
    if (!source) {
        return QString();
    }

    if (!source->client()) {
        return QStringLiteral("XWayland source");
    }

    const QString executable = waylandServer()->display()->getConnection(source->client())->executablePath();

    if (auto dataSource = qobject_cast<const KWaylandServer::DataSourceInterface *const>(source)) {
        return QStringLiteral("wl_data_source@%1 of %2").arg(wl_resource_get_id(dataSource->resource())).arg(executable);
    } else if (qobject_cast<const KWaylandServer::PrimarySelectionSourceV1Interface *const>(source)) {
        return QStringLiteral("zwp_primary_selection_source_v1 of %2").arg(executable);
    } else if (qobject_cast<const KWaylandServer::DataControlSourceV1Interface *const>(source)) {
        return QStringLiteral("data control by %1").arg(executable);
    }
    return QStringLiteral("unknown source of").arg(executable);
}

DebugConsole::DebugConsole()
    : QWidget()
    , m_ui(new Ui::DebugConsole)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    m_ui->setupUi(this);

    auto windowsModel = new DebugConsoleModel(this);
    QSortFilterProxyModel *proxyWindowsModel = new QSortFilterProxyModel(this);
    proxyWindowsModel->setSourceModel(windowsModel);
    m_ui->windowsView->setModel(proxyWindowsModel);
    m_ui->windowsView->sortByColumn(0, Qt::AscendingOrder);
    m_ui->windowsView->header()->setSortIndicatorShown(true);
    m_ui->windowsView->setItemDelegate(new DebugConsoleDelegate(this));

    m_ui->surfacesView->setModel(new SurfaceTreeModel(this));
    m_ui->clipboardContent->setModel(new DataSourceModel(this));
    m_ui->primaryContent->setModel(new DataSourceModel(this));
    m_ui->inputDevicesView->setModel(new InputDeviceModel(this));
    m_ui->inputDevicesView->setItemDelegate(new DebugConsoleDelegate(this));
    m_ui->quitButton->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
    m_ui->tabWidget->setTabIcon(0, QIcon::fromTheme(QStringLiteral("view-list-tree")));
    m_ui->tabWidget->setTabIcon(1, QIcon::fromTheme(QStringLiteral("view-list-tree")));

    if (kwinApp()->operationMode() == Application::OperationMode::OperationModeX11) {
        m_ui->tabWidget->setTabEnabled(1, false);
        m_ui->tabWidget->setTabEnabled(2, false);
        m_ui->tabWidget->setTabEnabled(6, false);
        setWindowFlags(Qt::X11BypassWindowManagerHint);
    }

    connect(m_ui->quitButton, &QAbstractButton::clicked, this, &DebugConsole::deleteLater);
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        // delay creation of input event filter until the tab is selected
        if (index == 2 && !m_inputFilter) {
            m_inputFilter.reset(new DebugConsoleFilter(m_ui->inputTextEdit));
            input()->installInputEventSpy(m_inputFilter.get());
        }
        if (index == 5) {
            updateKeyboardTab();
            connect(input(), &InputRedirection::keyStateChanged, this, &DebugConsole::updateKeyboardTab);
        }
        if (index == 6) {
            static_cast<DataSourceModel *>(m_ui->clipboardContent->model())->setSource(waylandServer()->seat()->selection());
            m_ui->clipboardSource->setText(sourceString(waylandServer()->seat()->selection()));
            connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::selectionChanged, this, [this](KWaylandServer::AbstractDataSource *source) {
                static_cast<DataSourceModel *>(m_ui->clipboardContent->model())->setSource(source);
                m_ui->clipboardSource->setText(sourceString(source));
            });
            static_cast<DataSourceModel *>(m_ui->primaryContent->model())->setSource(waylandServer()->seat()->primarySelection());
            m_ui->primarySource->setText(sourceString(waylandServer()->seat()->primarySelection()));
            connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::primarySelectionChanged, this, [this](KWaylandServer::AbstractDataSource *source) {
                static_cast<DataSourceModel *>(m_ui->primaryContent->model())->setSource(source);
                m_ui->primarySource->setText(sourceString(source));
            });
        }
    });

    initGLTab();
}

DebugConsole::~DebugConsole() = default;

void DebugConsole::initGLTab()
{
    if (!effects || !effects->isOpenGLCompositing()) {
        m_ui->noOpenGLLabel->setVisible(true);
        m_ui->glInfoScrollArea->setVisible(false);
        return;
    }
    GLPlatform *gl = GLPlatform::instance();
    m_ui->noOpenGLLabel->setVisible(false);
    m_ui->glInfoScrollArea->setVisible(true);
    m_ui->glVendorStringLabel->setText(QString::fromLocal8Bit(gl->glVendorString()));
    m_ui->glRendererStringLabel->setText(QString::fromLocal8Bit(gl->glRendererString()));
    m_ui->glVersionStringLabel->setText(QString::fromLocal8Bit(gl->glVersionString()));
    m_ui->glslVersionStringLabel->setText(QString::fromLocal8Bit(gl->glShadingLanguageVersionString()));
    m_ui->glDriverLabel->setText(GLPlatform::driverToString(gl->driver()));
    m_ui->glGPULabel->setText(GLPlatform::chipClassToString(gl->chipClass()));
    m_ui->glVersionLabel->setText(GLPlatform::versionToString(gl->glVersion()));
    m_ui->glslLabel->setText(GLPlatform::versionToString(gl->glslVersion()));

    auto extensionsString = [](const auto &extensions) {
        QString text = QStringLiteral("<ul>");
        for (auto extension : extensions) {
            text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(extension)));
        }
        text.append(QStringLiteral("</ul>"));
        return text;
    };

    const OpenGLBackend *backend = static_cast<OpenGLBackend *>(Compositor::self()->backend());
    m_ui->platformExtensionsLabel->setText(extensionsString(backend->extensions()));
    m_ui->openGLExtensionsLabel->setText(extensionsString(openGLExtensions()));
}

template<typename T>
QString keymapComponentToString(xkb_keymap *map, const T &count, std::function<const char *(xkb_keymap *, T)> f)
{
    QString text = QStringLiteral("<ul>");
    for (T i = 0; i < count; i++) {
        text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(f(map, i))));
    }
    text.append(QStringLiteral("</ul>"));
    return text;
}

template<typename T>
QString stateActiveComponents(xkb_state *state, const T &count, std::function<int(xkb_state *, T)> f, std::function<const char *(xkb_keymap *, T)> name)
{
    QString text = QStringLiteral("<ul>");
    xkb_keymap *map = xkb_state_get_keymap(state);
    for (T i = 0; i < count; i++) {
        if (f(state, i) == 1) {
            text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(name(map, i))));
        }
    }
    text.append(QStringLiteral("</ul>"));
    return text;
}

void DebugConsole::updateKeyboardTab()
{
    auto xkb = input()->keyboard()->xkb();
    xkb_keymap *map = xkb->keymap();
    xkb_state *state = xkb->state();
    m_ui->layoutsLabel->setText(keymapComponentToString<xkb_layout_index_t>(map, xkb_keymap_num_layouts(map), &xkb_keymap_layout_get_name));
    m_ui->currentLayoutLabel->setText(xkb_keymap_layout_get_name(map, xkb->currentLayout()));
    m_ui->modifiersLabel->setText(keymapComponentToString<xkb_mod_index_t>(map, xkb_keymap_num_mods(map), &xkb_keymap_mod_get_name));
    m_ui->ledsLabel->setText(keymapComponentToString<xkb_led_index_t>(map, xkb_keymap_num_leds(map), &xkb_keymap_led_get_name));
    m_ui->activeLedsLabel->setText(stateActiveComponents<xkb_led_index_t>(state, xkb_keymap_num_leds(map), &xkb_state_led_index_is_active, &xkb_keymap_led_get_name));

    using namespace std::placeholders;
    auto modActive = std::bind(xkb_state_mod_index_is_active, _1, _2, XKB_STATE_MODS_EFFECTIVE);
    m_ui->activeModifiersLabel->setText(stateActiveComponents<xkb_mod_index_t>(state, xkb_keymap_num_mods(map), modActive, &xkb_keymap_mod_get_name));
}

void DebugConsole::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // delay the connection to the show event as in ctor the windowHandle returns null
    connect(windowHandle(), &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible) {
            // ignore
            return;
        }
        deleteLater();
    });
}

DebugConsoleDelegate::DebugConsoleDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

DebugConsoleDelegate::~DebugConsoleDelegate() = default;

QString DebugConsoleDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    switch (value.userType()) {
    case QMetaType::QPoint: {
        const QPoint p = value.toPoint();
        return QStringLiteral("%1,%2").arg(p.x()).arg(p.y());
    }
    case QMetaType::QPointF: {
        const QPointF p = value.toPointF();
        return QStringLiteral("%1,%2").arg(p.x()).arg(p.y());
    }
    case QMetaType::QSize: {
        const QSize s = value.toSize();
        return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
    }
    case QMetaType::QSizeF: {
        const QSizeF s = value.toSizeF();
        return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
    }
    case QMetaType::QRect: {
        const QRect r = value.toRect();
        return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    case QMetaType::QRectF: {
        const QRectF r = value.toRectF();
        return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    default:
        if (value.userType() == qMetaTypeId<KWaylandServer::SurfaceInterface *>()) {
            if (auto s = value.value<KWaylandServer::SurfaceInterface *>()) {
                return QStringLiteral("KWaylandServer::SurfaceInterface(0x%1)").arg(qulonglong(s), 0, 16);
            } else {
                return QStringLiteral("nullptr");
            }
        }
        if (value.userType() == qMetaTypeId<Qt::MouseButtons>()) {
            const auto buttons = value.value<Qt::MouseButtons>();
            if (buttons == Qt::NoButton) {
                return i18n("No Mouse Buttons");
            }
            QStringList list;
            if (buttons.testFlag(Qt::LeftButton)) {
                list << i18nc("Mouse Button", "left");
            }
            if (buttons.testFlag(Qt::RightButton)) {
                list << i18nc("Mouse Button", "right");
            }
            if (buttons.testFlag(Qt::MiddleButton)) {
                list << i18nc("Mouse Button", "middle");
            }
            if (buttons.testFlag(Qt::BackButton)) {
                list << i18nc("Mouse Button", "back");
            }
            if (buttons.testFlag(Qt::ForwardButton)) {
                list << i18nc("Mouse Button", "forward");
            }
            if (buttons.testFlag(Qt::ExtraButton1)) {
                list << i18nc("Mouse Button", "extra 1");
            }
            if (buttons.testFlag(Qt::ExtraButton2)) {
                list << i18nc("Mouse Button", "extra 2");
            }
            if (buttons.testFlag(Qt::ExtraButton3)) {
                list << i18nc("Mouse Button", "extra 3");
            }
            if (buttons.testFlag(Qt::ExtraButton4)) {
                list << i18nc("Mouse Button", "extra 4");
            }
            if (buttons.testFlag(Qt::ExtraButton5)) {
                list << i18nc("Mouse Button", "extra 5");
            }
            if (buttons.testFlag(Qt::ExtraButton6)) {
                list << i18nc("Mouse Button", "extra 6");
            }
            if (buttons.testFlag(Qt::ExtraButton7)) {
                list << i18nc("Mouse Button", "extra 7");
            }
            if (buttons.testFlag(Qt::ExtraButton8)) {
                list << i18nc("Mouse Button", "extra 8");
            }
            if (buttons.testFlag(Qt::ExtraButton9)) {
                list << i18nc("Mouse Button", "extra 9");
            }
            if (buttons.testFlag(Qt::ExtraButton10)) {
                list << i18nc("Mouse Button", "extra 10");
            }
            if (buttons.testFlag(Qt::ExtraButton11)) {
                list << i18nc("Mouse Button", "extra 11");
            }
            if (buttons.testFlag(Qt::ExtraButton12)) {
                list << i18nc("Mouse Button", "extra 12");
            }
            if (buttons.testFlag(Qt::ExtraButton13)) {
                list << i18nc("Mouse Button", "extra 13");
            }
            if (buttons.testFlag(Qt::ExtraButton14)) {
                list << i18nc("Mouse Button", "extra 14");
            }
            if (buttons.testFlag(Qt::ExtraButton15)) {
                list << i18nc("Mouse Button", "extra 15");
            }
            if (buttons.testFlag(Qt::ExtraButton16)) {
                list << i18nc("Mouse Button", "extra 16");
            }
            if (buttons.testFlag(Qt::ExtraButton17)) {
                list << i18nc("Mouse Button", "extra 17");
            }
            if (buttons.testFlag(Qt::ExtraButton18)) {
                list << i18nc("Mouse Button", "extra 18");
            }
            if (buttons.testFlag(Qt::ExtraButton19)) {
                list << i18nc("Mouse Button", "extra 19");
            }
            if (buttons.testFlag(Qt::ExtraButton20)) {
                list << i18nc("Mouse Button", "extra 20");
            }
            if (buttons.testFlag(Qt::ExtraButton21)) {
                list << i18nc("Mouse Button", "extra 21");
            }
            if (buttons.testFlag(Qt::ExtraButton22)) {
                list << i18nc("Mouse Button", "extra 22");
            }
            if (buttons.testFlag(Qt::ExtraButton23)) {
                list << i18nc("Mouse Button", "extra 23");
            }
            if (buttons.testFlag(Qt::ExtraButton24)) {
                list << i18nc("Mouse Button", "extra 24");
            }
            if (buttons.testFlag(Qt::TaskButton)) {
                list << i18nc("Mouse Button", "task");
            }
            return list.join(QStringLiteral(", "));
        }
        break;
    }
    return QStyledItemDelegate::displayText(value, locale);
}

static const int s_x11WindowId = 1;
static const int s_x11UnmanagedId = 2;
static const int s_waylandWindowId = 3;
static const int s_workspaceInternalId = 4;
static const quint32 s_propertyBitMask = 0xFFFF0000;
static const quint32 s_windowBitMask = 0x0000FFFF;
static const quint32 s_idDistance = 10000;

template<class T>
void DebugConsoleModel::add(int parentRow, QVector<T *> &windows, T *window)
{
    beginInsertRows(index(parentRow, 0, QModelIndex()), windows.count(), windows.count());
    windows.append(window);
    endInsertRows();
}

template<class T>
void DebugConsoleModel::remove(int parentRow, QVector<T *> &windows, T *window)
{
    const int remove = windows.indexOf(window);
    if (remove == -1) {
        return;
    }
    beginRemoveRows(index(parentRow, 0, QModelIndex()), remove, remove);
    windows.removeAt(remove);
    endRemoveRows();
}

DebugConsoleModel::DebugConsoleModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    const auto windows = workspace()->allClientList();
    for (auto window : windows) {
        handleWindowAdded(window);
    }
    connect(workspace(), &Workspace::windowAdded, this, &DebugConsoleModel::handleWindowAdded);
    connect(workspace(), &Workspace::windowRemoved, this, &DebugConsoleModel::handleWindowRemoved);

    const auto unmangeds = workspace()->unmanagedList();
    for (auto u : unmangeds) {
        m_unmanageds.append(u);
    }
    connect(workspace(), &Workspace::unmanagedAdded, this, [this](Unmanaged *u) {
        add(s_x11UnmanagedId - 1, m_unmanageds, u);
    });
    connect(workspace(), &Workspace::unmanagedRemoved, this, [this](Unmanaged *u) {
        remove(s_x11UnmanagedId - 1, m_unmanageds, u);
    });
    for (InternalWindow *window : workspace()->internalWindows()) {
        m_internalWindows.append(window);
    }
    connect(workspace(), &Workspace::internalWindowAdded, this, [this](InternalWindow *window) {
        add(s_workspaceInternalId - 1, m_internalWindows, window);
    });
    connect(workspace(), &Workspace::internalWindowRemoved, this, [this](InternalWindow *window) {
        remove(s_workspaceInternalId - 1, m_internalWindows, window);
    });
}

void DebugConsoleModel::handleWindowAdded(Window *window)
{
    if (auto x11 = qobject_cast<X11Window *>(window)) {
        add(s_x11WindowId - 1, m_x11Windows, x11);
        return;
    }

    if (auto wayland = qobject_cast<WaylandWindow *>(window)) {
        add(s_waylandWindowId - 1, m_waylandWindows, wayland);
        return;
    }
}

void DebugConsoleModel::handleWindowRemoved(Window *window)
{
    if (auto x11 = qobject_cast<X11Window *>(window)) {
        remove(s_x11WindowId - 1, m_x11Windows, x11);
        return;
    }

    if (auto wayland = qobject_cast<WaylandWindow *>(window)) {
        remove(s_waylandWindowId - 1, m_waylandWindows, wayland);
        return;
    }
}

DebugConsoleModel::~DebugConsoleModel() = default;

int DebugConsoleModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

int DebugConsoleModel::topLevelRowCount() const
{
    return kwinApp()->shouldUseWaylandForCompositing() ? 4 : 2;
}

template<class T>
int DebugConsoleModel::propertyCount(const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const
{
    if (T *t = (this->*filter)(parent)) {
        return t->metaObject()->propertyCount();
    }
    return 0;
}

int DebugConsoleModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return topLevelRowCount();
    }

    switch (parent.internalId()) {
    case s_x11WindowId:
        return m_x11Windows.count();
    case s_x11UnmanagedId:
        return m_unmanageds.count();
    case s_waylandWindowId:
        return m_waylandWindows.count();
    case s_workspaceInternalId:
        return m_internalWindows.count();
    default:
        break;
    }

    if (parent.internalId() & s_propertyBitMask) {
        // properties do not have children
        return 0;
    }

    if (parent.internalId() < s_idDistance * (s_x11WindowId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::x11Window);
    } else if (parent.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::unmanaged);
    } else if (parent.internalId() < s_idDistance * (s_waylandWindowId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::waylandWindow);
    } else if (parent.internalId() < s_idDistance * (s_workspaceInternalId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::internalWindow);
    }

    return 0;
}

template<class T>
QModelIndex DebugConsoleModel::indexForWindow(int row, int column, const QVector<T *> &windows, int id) const
{
    if (column != 0) {
        return QModelIndex();
    }
    if (row >= windows.count()) {
        return QModelIndex();
    }
    return createIndex(row, column, s_idDistance * id + row);
}

template<class T>
QModelIndex DebugConsoleModel::indexForProperty(int row, int column, const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const
{
    if (T *t = (this->*filter)(parent)) {
        if (row >= t->metaObject()->propertyCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, quint32(row + 1) << 16 | parent.internalId());
    }
    return QModelIndex();
}

QModelIndex DebugConsoleModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        // index for a top level item
        if (column != 0 || row >= topLevelRowCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, row + 1);
    }
    if (column >= 2) {
        // max of 2 columns
        return QModelIndex();
    }
    // index for a window (second level)
    switch (parent.internalId()) {
    case s_x11WindowId:
        return indexForWindow(row, column, m_x11Windows, s_x11WindowId);
    case s_x11UnmanagedId:
        return indexForWindow(row, column, m_unmanageds, s_x11UnmanagedId);
    case s_waylandWindowId:
        return indexForWindow(row, column, m_waylandWindows, s_waylandWindowId);
    case s_workspaceInternalId:
        return indexForWindow(row, column, m_internalWindows, s_workspaceInternalId);
    default:
        break;
    }

    // index for a property (third level)
    if (parent.internalId() < s_idDistance * (s_x11WindowId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::x11Window);
    } else if (parent.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::unmanaged);
    } else if (parent.internalId() < s_idDistance * (s_waylandWindowId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::waylandWindow);
    } else if (parent.internalId() < s_idDistance * (s_workspaceInternalId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::internalWindow);
    }

    return QModelIndex();
}

QModelIndex DebugConsoleModel::parent(const QModelIndex &child) const
{
    if (child.internalId() <= s_workspaceInternalId) {
        return QModelIndex();
    }
    if (child.internalId() & s_propertyBitMask) {
        // a property
        const quint32 parentId = child.internalId() & s_windowBitMask;
        if (parentId < s_idDistance * (s_x11WindowId + 1)) {
            return createIndex(parentId - (s_idDistance * s_x11WindowId), 0, parentId);
        } else if (parentId < s_idDistance * (s_x11UnmanagedId + 1)) {
            return createIndex(parentId - (s_idDistance * s_x11UnmanagedId), 0, parentId);
        } else if (parentId < s_idDistance * (s_waylandWindowId + 1)) {
            return createIndex(parentId - (s_idDistance * s_waylandWindowId), 0, parentId);
        } else if (parentId < s_idDistance * (s_workspaceInternalId + 1)) {
            return createIndex(parentId - (s_idDistance * s_workspaceInternalId), 0, parentId);
        }
        return QModelIndex();
    }
    if (child.internalId() < s_idDistance * (s_x11WindowId + 1)) {
        return createIndex(s_x11WindowId - 1, 0, s_x11WindowId);
    } else if (child.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return createIndex(s_x11UnmanagedId - 1, 0, s_x11UnmanagedId);
    } else if (child.internalId() < s_idDistance * (s_waylandWindowId + 1)) {
        return createIndex(s_waylandWindowId - 1, 0, s_waylandWindowId);
    } else if (child.internalId() < s_idDistance * (s_workspaceInternalId + 1)) {
        return createIndex(s_workspaceInternalId - 1, 0, s_workspaceInternalId);
    }
    return QModelIndex();
}

QVariant DebugConsoleModel::propertyData(QObject *object, const QModelIndex &index, int role) const
{
    const auto property = object->metaObject()->property(index.row());
    if (index.column() == 0) {
        return property.name();
    } else {
        const QVariant value = property.read(object);
        if (qstrcmp(property.name(), "windowType") == 0) {
            switch (value.toInt()) {
            case NET::Normal:
                return QStringLiteral("NET::Normal");
            case NET::Desktop:
                return QStringLiteral("NET::Desktop");
            case NET::Dock:
                return QStringLiteral("NET::Dock");
            case NET::Toolbar:
                return QStringLiteral("NET::Toolbar");
            case NET::Menu:
                return QStringLiteral("NET::Menu");
            case NET::Dialog:
                return QStringLiteral("NET::Dialog");
            case NET::Override:
                return QStringLiteral("NET::Override");
            case NET::TopMenu:
                return QStringLiteral("NET::TopMenu");
            case NET::Utility:
                return QStringLiteral("NET::Utility");
            case NET::Splash:
                return QStringLiteral("NET::Splash");
            case NET::DropdownMenu:
                return QStringLiteral("NET::DropdownMenu");
            case NET::PopupMenu:
                return QStringLiteral("NET::PopupMenu");
            case NET::Tooltip:
                return QStringLiteral("NET::Tooltip");
            case NET::Notification:
                return QStringLiteral("NET::Notification");
            case NET::ComboBox:
                return QStringLiteral("NET::ComboBox");
            case NET::DNDIcon:
                return QStringLiteral("NET::DNDIcon");
            case NET::OnScreenDisplay:
                return QStringLiteral("NET::OnScreenDisplay");
            case NET::CriticalNotification:
                return QStringLiteral("NET::CriticalNotification");
            case NET::AppletPopup:
                return QStringLiteral("NET::AppletPopup");
            case NET::Unknown:
            default:
                return QStringLiteral("NET::Unknown");
            }
        } else if (qstrcmp(property.name(), "layer") == 0) {
            return QMetaEnum::fromType<Layer>().valueToKey(value.value<Layer>());
        }
        return value;
    }
    return QVariant();
}

template<class T>
QVariant DebugConsoleModel::windowData(const QModelIndex &index, int role, const QVector<T *> windows, const std::function<QString(T *)> &toString) const
{
    if (index.row() >= windows.count()) {
        return QVariant();
    }
    auto c = windows.at(index.row());
    if (role == Qt::DisplayRole) {
        return toString(c);
    } else if (role == Qt::DecorationRole) {
        return c->icon();
    }
    return QVariant();
}

QVariant DebugConsoleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (!index.parent().isValid()) {
        // one of the top levels
        if (index.column() != 0 || role != Qt::DisplayRole) {
            return QVariant();
        }
        switch (index.internalId()) {
        case s_x11WindowId:
            return i18n("X11 Windows");
        case s_x11UnmanagedId:
            return i18n("X11 Unmanaged Windows");
        case s_waylandWindowId:
            return i18n("Wayland Windows");
        case s_workspaceInternalId:
            return i18n("Internal Windows");
        default:
            return QVariant();
        }
    }
    if (index.internalId() & s_propertyBitMask) {
        if (index.column() >= 2 || role != Qt::DisplayRole) {
            return QVariant();
        }
        if (Window *w = waylandWindow(index)) {
            return propertyData(w, index, role);
        } else if (InternalWindow *w = internalWindow(index)) {
            return propertyData(w, index, role);
        } else if (X11Window *w = x11Window(index)) {
            return propertyData(w, index, role);
        } else if (Unmanaged *u = unmanaged(index)) {
            return propertyData(u, index, role);
        }
    } else {
        if (index.column() != 0) {
            return QVariant();
        }

        auto generic = [](Window *c) -> QString {
            return c->caption() + QLatin1Char(' ') + QString::fromUtf8(c->metaObject()->className());
        };
        switch (index.parent().internalId()) {
        case s_x11WindowId:
            return windowData<X11Window>(index, role, m_x11Windows, [](X11Window *c) -> QString {
                return QStringLiteral("0x%1: %2").arg(c->window(), 0, 16).arg(c->caption());
            });
        case s_x11UnmanagedId: {
            if (index.row() >= m_unmanageds.count()) {
                return QVariant();
            }
            auto u = m_unmanageds.at(index.row());
            if (role == Qt::DisplayRole) {
                return QStringLiteral("0x%1").arg(u->window(), 0, 16);
            }
            break;
        }
        case s_waylandWindowId:
            return windowData<WaylandWindow>(index, role, m_waylandWindows, generic);
        case s_workspaceInternalId:
            return windowData<InternalWindow>(index, role, m_internalWindows, generic);
        default:
            break;
        }
    }

    return QVariant();
}

template<class T>
static T *windowForIndex(const QModelIndex &index, const QVector<T *> &windows, int id)
{
    const qint32 row = (index.internalId() & s_windowBitMask) - (s_idDistance * id);
    if (row < 0 || row >= windows.count()) {
        return nullptr;
    }
    return windows.at(row);
}

WaylandWindow *DebugConsoleModel::waylandWindow(const QModelIndex &index) const
{
    return windowForIndex(index, m_waylandWindows, s_waylandWindowId);
}

InternalWindow *DebugConsoleModel::internalWindow(const QModelIndex &index) const
{
    return windowForIndex(index, m_internalWindows, s_workspaceInternalId);
}

X11Window *DebugConsoleModel::x11Window(const QModelIndex &index) const
{
    return windowForIndex(index, m_x11Windows, s_x11WindowId);
}

Unmanaged *DebugConsoleModel::unmanaged(const QModelIndex &index) const
{
    return windowForIndex(index, m_unmanageds, s_x11UnmanagedId);
}

/////////////////////////////////////// SurfaceTreeModel
SurfaceTreeModel::SurfaceTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    // TODO: it would be nice to not have to reset the model on each change
    auto reset = [this] {
        beginResetModel();
        endResetModel();
    };
    using namespace KWaylandServer;

    auto watchSubsurfaces = [this, reset](Window *c) {
        if (!c->surface()) {
            return;
        }
        auto monitor = new SubSurfaceMonitor(c->surface(), this);
        connect(monitor, &SubSurfaceMonitor::subSurfaceAdded, this, reset);
        connect(monitor, &SubSurfaceMonitor::subSurfaceRemoved, this, reset);
        connect(c, &QObject::destroyed, monitor, &QObject::deleteLater);
    };

    for (auto c : workspace()->allClientList()) {
        watchSubsurfaces(c);
    }
    connect(workspace(), &Workspace::windowAdded, this, [reset, watchSubsurfaces](Window *c) {
        watchSubsurfaces(c);
        reset();
    });
    connect(workspace(), &Workspace::windowRemoved, this, reset);
    connect(workspace(), &Workspace::unmanagedAdded, this, reset);
    connect(workspace(), &Workspace::unmanagedRemoved, this, reset);
}

SurfaceTreeModel::~SurfaceTreeModel() = default;

int SurfaceTreeModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

int SurfaceTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        using namespace KWaylandServer;
        if (SurfaceInterface *surface = static_cast<SurfaceInterface *>(parent.internalPointer())) {
            return surface->below().count() + surface->above().count();
        }
        return 0;
    }
    // toplevel are all windows
    return workspace()->allClientList().count() + workspace()->unmanagedList().count();
}

QModelIndex SurfaceTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0) {
        // invalid column
        return QModelIndex();
    }

    if (parent.isValid()) {
        using namespace KWaylandServer;
        if (SurfaceInterface *surface = static_cast<SurfaceInterface *>(parent.internalPointer())) {
            int reference = 0;
            const auto &below = surface->below();
            if (row < reference + below.count()) {
                return createIndex(row, column, below.at(row - reference)->surface());
            }
            reference += below.count();

            const auto &above = surface->above();
            if (row < reference + above.count()) {
                return createIndex(row, column, above.at(row - reference)->surface());
            }
        }
        return QModelIndex();
    }
    // a window
    const auto &allClients = workspace()->allClientList();
    if (row < allClients.count()) {
        // references a client
        return createIndex(row, column, allClients.at(row)->surface());
    }
    int reference = allClients.count();
    const auto &unmanaged = workspace()->unmanagedList();
    if (row < reference + unmanaged.count()) {
        return createIndex(row, column, unmanaged.at(row - reference)->surface());
    }
    reference += unmanaged.count();
    // not found
    return QModelIndex();
}

QModelIndex SurfaceTreeModel::parent(const QModelIndex &child) const
{
    using namespace KWaylandServer;
    if (SurfaceInterface *surface = static_cast<SurfaceInterface *>(child.internalPointer())) {
        const auto &subsurface = surface->subSurface();
        if (!subsurface) {
            // doesn't reference a subsurface, this is a top-level window
            return QModelIndex();
        }
        SurfaceInterface *parent = subsurface->parentSurface();
        if (!parent) {
            // something is wrong
            return QModelIndex();
        }
        // is the parent a subsurface itself?
        if (parent->subSurface()) {
            auto grandParent = parent->subSurface()->parentSurface();
            if (!grandParent) {
                // something is wrong
                return QModelIndex();
            }
            int row = 0;
            const auto &below = grandParent->below();
            for (int i = 0; i < below.count(); i++) {
                if (below.at(i) == parent->subSurface()) {
                    return createIndex(row + i, 0, parent);
                }
            }
            row += below.count();
            const auto &above = grandParent->above();
            for (int i = 0; i < above.count(); i++) {
                if (above.at(i) == parent->subSurface()) {
                    return createIndex(row + i, 0, parent);
                }
            }
            return QModelIndex();
        }
        // not a subsurface, thus it's a true window
        int row = 0;
        const auto &allClients = workspace()->allClientList();
        for (; row < allClients.count(); row++) {
            if (allClients.at(row)->surface() == parent) {
                return createIndex(row, 0, parent);
            }
        }
        row = allClients.count();
        const auto &unmanaged = workspace()->unmanagedList();
        for (int i = 0; i < unmanaged.count(); i++) {
            if (unmanaged.at(i)->surface() == parent) {
                return createIndex(row + i, 0, parent);
            }
        }
        row += unmanaged.count();
    }
    return QModelIndex();
}

QVariant SurfaceTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    using namespace KWaylandServer;
    if (SurfaceInterface *surface = static_cast<SurfaceInterface *>(index.internalPointer())) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            return QStringLiteral("%1 (%2) - %3").arg(surface->client()->executablePath()).arg(surface->client()->processId()).arg(surface->id());
        } else if (role == Qt::DecorationRole) {
            if (auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(surface->buffer())) {
                return buffer->data().scaled(QSize(64, 64), Qt::KeepAspectRatio);
            }
        }
    }
    return QVariant();
}

InputDeviceModel::InputDeviceModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_devices(input()->devices())
{
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        setupDeviceConnections(*it);
    }

    connect(input(), &InputRedirection::deviceAdded, this, [this](InputDevice *d) {
        beginInsertRows(QModelIndex(), m_devices.count(), m_devices.count());
        m_devices << d;
        setupDeviceConnections(d);
        endInsertRows();
    });
    connect(input(), &InputRedirection::deviceRemoved, this, [this](InputDevice *d) {
        const int index = m_devices.indexOf(d);
        if (index == -1) {
            return;
        }
        beginRemoveRows(QModelIndex(), index, index);
        m_devices.removeAt(index);
        endRemoveRows();
    });
}

InputDeviceModel::~InputDeviceModel() = default;

int InputDeviceModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant InputDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (!index.parent().isValid() && index.column() == 0) {
        if (index.row() >= m_devices.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return m_devices.at(index.row())->name();
        }
    }
    if (index.parent().isValid()) {
        if (role == Qt::DisplayRole) {
            const auto device = m_devices.at(index.parent().row());
            const auto property = device->metaObject()->property(index.row());
            if (index.column() == 0) {
                return property.name();
            } else if (index.column() == 1) {
                return device->property(property.name());
            }
        }
    }
    return QVariant();
}

QModelIndex InputDeviceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column >= 2) {
        return QModelIndex();
    }
    if (parent.isValid()) {
        if (parent.internalId() & s_propertyBitMask) {
            return QModelIndex();
        }
        if (row >= m_devices.at(parent.row())->metaObject()->propertyCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, quint32(row + 1) << 16 | parent.internalId());
    }
    if (row >= m_devices.count()) {
        return QModelIndex();
    }
    return createIndex(row, column, row + 1);
}

int InputDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_devices.count();
    }
    if (parent.internalId() & s_propertyBitMask) {
        return 0;
    }

    return m_devices.at(parent.row())->metaObject()->propertyCount();
}

QModelIndex InputDeviceModel::parent(const QModelIndex &child) const
{
    if (child.internalId() & s_propertyBitMask) {
        const quintptr parentId = child.internalId() & s_windowBitMask;
        return createIndex(parentId - 1, 0, parentId);
    }
    return QModelIndex();
}

void InputDeviceModel::slotPropertyChanged()
{
    const auto device = static_cast<InputDevice *>(sender());

    for (int i = 0; i < device->metaObject()->propertyCount(); ++i) {
        const QMetaProperty metaProperty = device->metaObject()->property(i);
        if (metaProperty.notifySignalIndex() == senderSignalIndex()) {
            const QModelIndex parent = index(m_devices.indexOf(device), 0, QModelIndex());
            const QModelIndex child = index(i, 1, parent);
            Q_EMIT dataChanged(child, child, QVector<int>{Qt::DisplayRole});
        }
    }
}

void InputDeviceModel::setupDeviceConnections(InputDevice *device)
{
    QMetaMethod handler = metaObject()->method(metaObject()->indexOfMethod("slotPropertyChanged()"));
    for (int i = 0; i < device->metaObject()->propertyCount(); ++i) {
        const QMetaProperty metaProperty = device->metaObject()->property(i);
        if (metaProperty.hasNotifySignal()) {
            connect(device, metaProperty.notifySignal(), this, handler);
        }
    }
}

QModelIndex DataSourceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_source || parent.isValid() || column >= 2 || row >= m_source->mimeTypes().size()) {
        return QModelIndex();
    }
    return createIndex(row, column, nullptr);
}

QModelIndex DataSourceModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

int DataSourceModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_source ? m_source->mimeTypes().count() : 0;
    }
    return 0;
}

QVariant DataSourceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section >= 2) {
        return QVariant();
    }
    return section == 0 ? QStringLiteral("Mime type") : QStringLiteral("Content");
}

QVariant DataSourceModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::ParentIsInvalid | CheckIndexOption::IndexIsValid)) {
        return QVariant();
    }
    const QString mimeType = m_source->mimeTypes().at(index.row());
    ;
    if (index.column() == 0 && role == Qt::DisplayRole) {
        return mimeType;
    } else if (index.column() == 1 && index.row() < m_data.count()) {
        const QByteArray &data = m_data.at(index.row());
        if (mimeType.contains(QLatin1String("image"))) {
            if (role == Qt::DecorationRole) {
                return QImage::fromData(data);
            }
        } else if (role == Qt::DisplayRole) {
            return data;
        }
    }
    return QVariant();
}

static QByteArray readData(int fd)
{
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    FileDescriptor closeFd{fd};
    QByteArray data;
    while (true) {
        const int ready = poll(&pfd, 1, 1000);
        if (ready < 0) {
            if (errno != EINTR) {
                return QByteArrayLiteral("poll() failed: ") + strerror(errno);
            }
        } else if (ready == 0) {
            return QByteArrayLiteral("timeout reading from pipe");
        } else {
            char buf[4096];
            int n = read(fd, buf, sizeof buf);

            if (n < 0) {
                return QByteArrayLiteral("read failed: ") + strerror(errno);
            } else if (n == 0) {
                return data;
            } else if (n > 0) {
                data.append(buf, n);
            }
        }
    }
}

void DataSourceModel::setSource(KWaylandServer::AbstractDataSource *source)
{
    beginResetModel();
    m_source = source;
    m_data.clear();
    if (source) {
        m_data.resize(m_source->mimeTypes().size());
        for (auto type = m_source->mimeTypes().cbegin(); type != m_source->mimeTypes().cend(); ++type) {
            int pipeFds[2];
            if (pipe2(pipeFds, O_CLOEXEC) != 0) {
                continue;
            }
            source->requestData(*type, pipeFds[1]);
            QFuture<QByteArray> data = QtConcurrent::run(readData, pipeFds[0]);
            auto watcher = new QFutureWatcher<QByteArray>(this);
            watcher->setFuture(data);
            const int index = type - m_source->mimeTypes().cbegin();
            connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, index, source = QPointer(source)] {
                watcher->deleteLater();
                if (source && source == m_source) {
                    m_data[index] = watcher->result();
                    Q_EMIT dataChanged(this->index(index, 1), this->index(index, 1), {Qt::DecorationRole | Qt::DisplayRole});
                }
            });
        }
    }
    endResetModel();
}
}
