/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "debug_console.h"
#include "composite.h"
#include "client.h"
#include "input_event.h"
#include "main.h"
#include "scene.h"
#include "shell_client.h"
#include "unmanaged.h"
#include "wayland_server.h"
#include "workspace.h"
#include "keyboard_input.h"
#if HAVE_INPUT
#include "libinput/connection.h"
#include "libinput/device.h"
#endif
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include "ui_debug_console.h"

// KWayland
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/subcompositor_interface.h>
#include <KWayland/Server/surface_interface.h>
// frameworks
#include <KLocalizedString>
#include <NETWM>
// Qt
#include <QMouseEvent>
#include <QMetaProperty>
#include <QMetaType>

// xkb
#include <xkbcommon/xkbcommon.h>

#include <functional>

namespace KWin
{


static QString tableHeaderRow(const QString &title)
{
    return QStringLiteral("<tr><th colspan=\"2\">%1</th></tr>").arg(title);
}

template<typename T>
static
QString tableRow(const QString &title, const T &argument)
{
    return QStringLiteral("<tr><td>%1</td><td>%2</td></tr>").arg(title).arg(argument);
}

static QString timestampRow(quint32 timestamp)
{
    return tableRow(i18n("Timestamp"), timestamp);
}

static QString timestampRowUsec(quint64 timestamp)
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

#if HAVE_INPUT
static QString deviceRow(LibInput::Device *device)
{
    if (!device) {
        return tableRow(i18n("Input Device"), i18nc("The input device of the event is not known", "Unknown"));
    }
    return tableRow(i18n("Input Device"), QStringLiteral("%1 (%2)").arg(device->name()).arg(device->sysName()));
}
#endif

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
#if HAVE_INPUT
        text.append(deviceRow(event->device()));
#endif
        text.append(timestamp);
        if (event->timestampMicroseconds() != 0) {
            text.append(timestampRowUsec(event->timestampMicroseconds()));
        }
        if (event->delta() != QSizeF()) {
            text.append(tableRow(i18nc("The relative mouse movement", "Delta"),
                                 QStringLiteral("%1/%2").arg(event->delta().width()).arg(event->delta().height())));
        }
        if (event->deltaUnaccelerated() != QSizeF()) {
            text.append(tableRow(i18nc("The relative mouse movement", "Delta (not accelerated)"),
                                 QStringLiteral("%1/%2").arg(event->deltaUnaccelerated().width()).arg(event->deltaUnaccelerated().height())));
        }
        text.append(tableRow(i18nc("The global mouse pointer position", "Global Position"), QStringLiteral("%1/%2").arg(event->pos().x()).arg(event->pos().y())));
        break;
    }
    case QEvent::MouseButtonPress:
        text.append(tableHeaderRow(i18nc("A mouse pointer button press event", "Pointer Button Press")));
#if HAVE_INPUT
        text.append(deviceRow(event->device()));
#endif
        text.append(timestamp);
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Button"), buttonToString(event->button())));
        text.append(tableRow(i18nc("A button in a mouse press/release event",  "Native Button code"), event->nativeButton()));
        text.append(tableRow(i18nc("All currently pressed buttons in a mouse press/release event", "Pressed Buttons"), buttonsToString(event->buttons())));
        break;
    case QEvent::MouseButtonRelease:
        text.append(tableHeaderRow(i18nc("A mouse pointer button release event", "Pointer Button Release")));
#if HAVE_INPUT
        text.append(deviceRow(event->device()));
#endif
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
#if HAVE_INPUT
        text.append(deviceRow(event->device()));
#endif
    text.append(timestampRow(event->timestamp()));
    const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
    text.append(tableRow(i18nc("The orientation of a pointer axis event", "Orientation"),
                         orientation == Qt::Horizontal ? i18nc("An orientation of a pointer axis event", "Horizontal")
                                                       : i18nc("An orientation of a pointer axis event", "Vertical")));
    text.append(tableRow(QStringLiteral("Delta"), orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y()));
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
#if HAVE_INPUT
        text.append(deviceRow(event->device()));
#endif
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
    text.append(tableRow(i18nc("The code as read from the input device", "Scan code"), event->nativeScanCode()));
    text.append(tableRow(i18nc("The translated code to an Xkb symbol", "Xkb symbol"), event->nativeVirtualKey()));
    text.append(tableRow(i18nc("The translated code interpreted as text", "Utf8"), event->text()));
    text.append(tableRow(i18nc("The currently active modifiers", "Modifiers"), modifiersToString()));

    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchDown(quint32 id, const QPointF &pos, quint32 time)
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

void DebugConsoleFilter::touchMotion(quint32 id, const QPointF &pos, quint32 time)
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

void DebugConsoleFilter::touchUp(quint32 id, quint32 time)
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

void DebugConsoleFilter::pinchGestureBegin(int fingerCount, quint32 time)
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

void DebugConsoleFilter::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture is updated", "Pinch update")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("Current scale in pinch gesture", "Scale"), scale));
    text.append(tableRow(i18nc("Current angle in pinch gesture", "Angle delta"), angleDelta));
    text.append(tableRow(i18nc("Current delta in pinch gesture", "Delta x"), delta.width()));
    text.append(tableRow(i18nc("Current delta in pinch gesture", "Delta y"), delta.height()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureEnd(quint32 time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture ended", "Pinch end")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureCancelled(quint32 time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture got cancelled", "Pinch cancelled")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureBegin(int fingerCount, quint32 time)
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

void DebugConsoleFilter::swipeGestureUpdate(const QSizeF &delta, quint32 time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture is updated", "Swipe update")));
    text.append(timestampRow(time));
    text.append(tableRow(i18nc("Current delta in swipe gesture", "Delta x"), delta.width()));
    text.append(tableRow(i18nc("Current delta in swipe gesture", "Delta y"), delta.height()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureEnd(quint32 time)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture ended", "Swipe end")));
    text.append(timestampRow(time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureCancelled(quint32 time)
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
    if (event->timestampMicroseconds() != 0) {
        text.append(timestampRowUsec(event->timestampMicroseconds()));
    }
#if HAVE_INPUT
    text.append(deviceRow(event->device()));
    QString switchName;
    if (event->device()->isLidSwitch()) {
        switchName = i18nc("Name of a hardware switch", "Notebook lid");
    } else if (event->device()->isTabletModeSwitch()) {
        switchName = i18nc("Name of a hardware switch", "Tablet mode");
    }
    text.append(tableRow(i18nc("A hardware switch", "Switch"), switchName));
#endif
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

DebugConsole::DebugConsole()
    : QWidget()
    , m_ui(new Ui::DebugConsole)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    m_ui->setupUi(this);
    m_ui->windowsView->setItemDelegate(new DebugConsoleDelegate(this));
    m_ui->windowsView->setModel(new DebugConsoleModel(this));
    m_ui->surfacesView->setModel(new SurfaceTreeModel(this));
#if HAVE_INPUT
    if (kwinApp()->usesLibinput()) {
        m_ui->inputDevicesView->setModel(new InputDeviceModel(this));
        m_ui->inputDevicesView->setItemDelegate(new DebugConsoleDelegate(this));
    }
#endif
    m_ui->quitButton->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
    m_ui->tabWidget->setTabIcon(0, QIcon::fromTheme(QStringLiteral("view-list-tree")));
    m_ui->tabWidget->setTabIcon(1, QIcon::fromTheme(QStringLiteral("view-list-tree")));

    if (kwinApp()->operationMode() == Application::OperationMode::OperationModeX11) {
        m_ui->tabWidget->setTabEnabled(1, false);
        m_ui->tabWidget->setTabEnabled(2, false);
    }
    if (!kwinApp()->usesLibinput()) {
        m_ui->tabWidget->setTabEnabled(3, false);
    }

    connect(m_ui->quitButton, &QAbstractButton::clicked, this, &DebugConsole::deleteLater);
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this,
        [this] (int index) {
            // delay creation of input event filter until the tab is selected
            if (index == 2 && m_inputFilter.isNull()) {
                m_inputFilter.reset(new DebugConsoleFilter(m_ui->inputTextEdit));
                input()->installInputEventSpy(m_inputFilter.data());
            }
            if (index == 5) {
                updateKeyboardTab();
                connect(input(), &InputRedirection::keyStateChanged, this, &DebugConsole::updateKeyboardTab);
            }
        }
    );

    // for X11
    setWindowFlags(Qt::X11BypassWindowManagerHint);

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

    auto extensionsString = [] (const auto &extensions) {
        QString text = QStringLiteral("<ul>");
        for (auto extension : extensions) {
            text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(extension)));
        }
        text.append(QStringLiteral("</ul>"));
        return text;
    };

    m_ui->platformExtensionsLabel->setText(extensionsString(Compositor::self()->scene()->openGLPlatformInterfaceExtensions()));
    m_ui->openGLExtensionsLabel->setText(extensionsString(openGLExtensions()));
}

template <typename T>
QString keymapComponentToString(xkb_keymap *map, const T &count, std::function<const char*(xkb_keymap*,T)> f)
{
    QString text = QStringLiteral("<ul>");
    for (T i = 0; i < count; i++) {
        text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(f(map, i))));
    }
    text.append(QStringLiteral("</ul>"));
    return text;
}

template <typename T>
QString stateActiveComponents(xkb_state *state, const T &count, std::function<int(xkb_state*,T)> f, std::function<const char*(xkb_keymap*,T)> name)
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
    connect(windowHandle(), &QWindow::visibleChanged, this,
        [this] (bool visible) {
            if (visible) {
                // ignore
                return;
            }
            deleteLater();
        }
    );
}

DebugConsoleDelegate::DebugConsoleDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

DebugConsoleDelegate::~DebugConsoleDelegate() = default;

QString DebugConsoleDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    switch (value.type()) {
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
    default:
        if (value.userType() == qMetaTypeId<KWayland::Server::SurfaceInterface*>()) {
            if (auto s = value.value<KWayland::Server::SurfaceInterface*>()) {
                return QStringLiteral("KWayland::Server::SurfaceInterface(0x%1)").arg(qulonglong(s), 0, 16);
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

static const int s_x11ClientId = 1;
static const int s_x11UnmanagedId = 2;
static const int s_waylandClientId = 3;
static const int s_waylandInternalId = 4;
static const quint32 s_propertyBitMask = 0xFFFF0000;
static const quint32 s_clientBitMask   = 0x0000FFFF;
static const quint32 s_idDistance = 10000;

template <class T>
void DebugConsoleModel::add(int parentRow, QVector<T*> &clients, T *client)
{
    beginInsertRows(index(parentRow, 0, QModelIndex()), clients.count(), clients.count());
    clients.append(client);
    endInsertRows();
}

template <class T>
void DebugConsoleModel::remove(int parentRow, QVector<T*> &clients, T *client)
{
    const int remove = clients.indexOf(client);
    if (remove == -1) {
        return;
    }
    beginRemoveRows(index(parentRow, 0, QModelIndex()), remove, remove);
    clients.removeAt(remove);
    endRemoveRows();
}

DebugConsoleModel::DebugConsoleModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    if (waylandServer()) {
        const auto clients = waylandServer()->clients();
        for (auto c : clients) {
            m_shellClients.append(c);
        }
        const auto internals = waylandServer()->internalClients();
        for (auto c : internals) {
            m_internalClients.append(c);
        }
        // TODO: that only includes windows getting shown, not those which are only created
        connect(waylandServer(), &WaylandServer::shellClientAdded, this,
            [this] (ShellClient *c) {
                if (c->isInternal()) {
                    add(s_waylandInternalId -1, m_internalClients, c);
                } else {
                    add(s_waylandClientId -1, m_shellClients, c);
                }
            }
        );
        connect(waylandServer(), &WaylandServer::shellClientRemoved, this,
            [this] (ShellClient *c) {
                remove(s_waylandInternalId -1, m_internalClients, c);
                remove(s_waylandClientId -1, m_shellClients, c);
            }
        );
    }
    const auto x11Clients = workspace()->clientList();
    for (auto c : x11Clients) {
        m_x11Clients.append(c);
    }
    const auto x11DesktopClients = workspace()->desktopList();
    for (auto c : x11DesktopClients) {
        m_x11Clients.append(c);
    }
    connect(workspace(), &Workspace::clientAdded, this,
        [this] (Client *c) {
            add(s_x11ClientId -1, m_x11Clients, c);
        }
    );
    connect(workspace(), &Workspace::clientRemoved, this,
        [this] (AbstractClient *ac) {
            Client *c = qobject_cast<Client*>(ac);
            if (!c) {
                return;
            }
            remove(s_x11ClientId -1, m_x11Clients, c);
        }
    );

    const auto unmangeds = workspace()->unmanagedList();
    for (auto u : unmangeds) {
        m_unmanageds.append(u);
    }
    connect(workspace(), &Workspace::unmanagedAdded, this,
        [this] (Unmanaged *u) {
            add(s_x11UnmanagedId -1, m_unmanageds, u);
        }
    );
    connect(workspace(), &Workspace::unmanagedRemoved, this,
        [this] (Unmanaged *u) {
            remove(s_x11UnmanagedId -1, m_unmanageds, u);
        }
    );
}

DebugConsoleModel::~DebugConsoleModel() = default;

int DebugConsoleModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2;
}

int DebugConsoleModel::topLevelRowCount() const
{
    return kwinApp()->shouldUseWaylandForCompositing() ? 4 : 2;
}

template <class T>
int DebugConsoleModel::propertyCount(const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex&) const) const
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
    case s_x11ClientId:
        return m_x11Clients.count();
    case s_x11UnmanagedId:
        return m_unmanageds.count();
    case s_waylandClientId:
        return m_shellClients.count();
    case s_waylandInternalId:
        return m_internalClients.count();
    default:
        break;
    }

    if (parent.internalId() & s_propertyBitMask) {
        // properties do not have children
        return 0;
    }

    if (parent.internalId() < s_idDistance * (s_x11ClientId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::x11Client);
    } else if (parent.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::unmanaged);
    } else if (parent.internalId() < s_idDistance * (s_waylandClientId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::shellClient);
    } else if (parent.internalId() < s_idDistance * (s_waylandInternalId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::internalClient);
    }

    return 0;
}

template <class T>
QModelIndex DebugConsoleModel::indexForClient(int row, int column, const QVector<T*> &clients, int id) const
{
    if (column != 0) {
        return QModelIndex();
    }
    if (row >= clients.count()) {
        return QModelIndex();
    }
    return createIndex(row, column, s_idDistance * id + row);
}

template <class T>
QModelIndex DebugConsoleModel::indexForProperty(int row, int column, const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex&) const) const
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
    // index for a client (second level)
    switch (parent.internalId()) {
    case s_x11ClientId:
        return indexForClient(row, column, m_x11Clients, s_x11ClientId);
    case s_x11UnmanagedId:
        return indexForClient(row, column, m_unmanageds, s_x11UnmanagedId);
    case s_waylandClientId:
        return indexForClient(row, column, m_shellClients, s_waylandClientId);
    case s_waylandInternalId:
        return indexForClient(row, column, m_internalClients, s_waylandInternalId);
    default:
        break;
    }

    // index for a property (third level)
    if (parent.internalId() < s_idDistance * (s_x11ClientId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::x11Client);
    } else if (parent.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::unmanaged);
    } else if (parent.internalId() < s_idDistance * (s_waylandClientId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::shellClient);
    } else if (parent.internalId() < s_idDistance * (s_waylandInternalId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::internalClient);
    }

    return QModelIndex();
}

QModelIndex DebugConsoleModel::parent(const QModelIndex &child) const
{
    if (child.internalId() <= s_waylandInternalId) {
        return QModelIndex();
    }
    if (child.internalId() & s_propertyBitMask) {
        // a property
        const quint32 parentId = child.internalId() & s_clientBitMask;
        if (parentId < s_idDistance * (s_x11ClientId + 1)) {
            return createIndex(parentId - (s_idDistance * s_x11ClientId), 0, parentId);
        } else if (parentId < s_idDistance * (s_x11UnmanagedId + 1)) {
            return createIndex(parentId - (s_idDistance * s_x11UnmanagedId), 0, parentId);
        } else if (parentId < s_idDistance * (s_waylandClientId + 1)) {
            return createIndex(parentId - (s_idDistance * s_waylandClientId), 0, parentId);
        } else if (parentId < s_idDistance * (s_waylandInternalId + 1)) {
            return createIndex(parentId - (s_idDistance * s_waylandInternalId), 0, parentId);
        }
        return QModelIndex();
    }
    if (child.internalId() < s_idDistance * (s_x11ClientId + 1)) {
        return createIndex(s_x11ClientId -1, 0, s_x11ClientId);
    } else if (child.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return createIndex(s_x11UnmanagedId -1, 0, s_x11UnmanagedId);
    } else if (child.internalId() < s_idDistance * (s_waylandClientId + 1)) {
        return createIndex(s_waylandClientId -1, 0, s_waylandClientId);
    } else if (child.internalId() < s_idDistance * (s_waylandInternalId + 1)) {
        return createIndex(s_waylandInternalId -1, 0, s_waylandInternalId);
    }
    return QModelIndex();
}

QVariant DebugConsoleModel::propertyData(QObject *object, const QModelIndex &index, int role) const
{
    Q_UNUSED(role)
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
            case NET::Unknown:
            default:
                return QStringLiteral("NET::Unknown");
            }
        }
        return value;
    }
    return QVariant();
}

template <class T>
QVariant DebugConsoleModel::clientData(const QModelIndex &index, int role, const QVector<T*> clients) const
{
    if (index.row() >= clients.count()) {
        return QVariant();
    }
    auto c = clients.at(index.row());
    if (role == Qt::DisplayRole) {
        return QStringLiteral("%1: %2").arg(c->window()).arg(c->caption());
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
        case s_x11ClientId:
            return i18n("X11 Client Windows");
        case s_x11UnmanagedId:
            return i18n("X11 Unmanaged Windows");
        case s_waylandClientId:
            return i18n("Wayland Windows");
        case s_waylandInternalId:
            return i18n("Internal Windows");
        default:
            return QVariant();
        }
    }
    if (index.internalId() & s_propertyBitMask) {
        if (index.column() >= 2 || role != Qt::DisplayRole) {
            return QVariant();
        }
        if (ShellClient *c = shellClient(index)) {
            return propertyData(c, index, role);
        } else if (ShellClient *c = internalClient(index)) {
            return propertyData(c, index, role);
        } else if (Client *c = x11Client(index)) {
            return propertyData(c, index, role);
        } else if (Unmanaged *u = unmanaged(index)) {
            return propertyData(u, index, role);
        }
    } else {
        if (index.column() != 0) {
            return QVariant();
        }
        switch (index.parent().internalId()) {
        case s_x11ClientId:
            return clientData(index, role, m_x11Clients);
        case s_x11UnmanagedId: {
            if (index.row() >= m_unmanageds.count()) {
                return QVariant();
            }
            auto u = m_unmanageds.at(index.row());
            if (role == Qt::DisplayRole) {
                return u->window();
            }
            break;
        }
        case s_waylandClientId:
            return clientData(index, role, m_shellClients);
        case s_waylandInternalId:
            return clientData(index, role, m_internalClients);
        default:
            break;
        }
    }

    return QVariant();
}

template<class T>
static T *clientForIndex(const QModelIndex &index, const QVector<T*> &clients, int id)
{
    const qint32 row = (index.internalId() & s_clientBitMask) - (s_idDistance * id);
    if (row < 0 || row >= clients.count()) {
        return nullptr;
    }
    return clients.at(row);
}

ShellClient *DebugConsoleModel::shellClient(const QModelIndex &index) const
{
    return clientForIndex(index, m_shellClients, s_waylandClientId);
}

ShellClient *DebugConsoleModel::internalClient(const QModelIndex &index) const
{
    return clientForIndex(index, m_internalClients, s_waylandInternalId);
}

Client *DebugConsoleModel::x11Client(const QModelIndex &index) const
{
    return clientForIndex(index, m_x11Clients, s_x11ClientId);
}

Unmanaged *DebugConsoleModel::unmanaged(const QModelIndex &index) const
{
    return clientForIndex(index, m_unmanageds, s_x11UnmanagedId);
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
    using namespace KWayland::Server;

    const auto unmangeds = workspace()->unmanagedList();
    for (auto u : unmangeds) {
        if (!u->surface()) {
            continue;
        }
        connect(u->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
    }
    for (auto c : workspace()->allClientList()) {
        if (!c->surface()) {
            continue;
        }
        connect(c->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
    }
    for (auto c : workspace()->desktopList()) {
        if (!c->surface()) {
            continue;
        }
        connect(c->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
    }
    if (waylandServer()) {
        for (auto c : waylandServer()->internalClients()) {
            connect(c->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
        }
        connect(waylandServer(), &WaylandServer::shellClientAdded, this,
            [this, reset] (ShellClient *c) {
                connect(c->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
                reset();
            }
        );
    }
    connect(workspace(), &Workspace::clientAdded, this,
        [this, reset] (AbstractClient *c) {
            if (c->surface()) {
                connect(c->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
            }
            reset();
        }
    );
    connect(workspace(), &Workspace::clientRemoved, this, reset);
    connect(workspace(), &Workspace::unmanagedAdded, this,
        [this, reset] (Unmanaged *u) {
            if (u->surface()) {
                connect(u->surface(), &SurfaceInterface::subSurfaceTreeChanged, this, reset);
            }
            reset();
        }
    );
    connect(workspace(), &Workspace::unmanagedRemoved, this, reset);
}

SurfaceTreeModel::~SurfaceTreeModel() = default;

int SurfaceTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int SurfaceTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        using namespace KWayland::Server;
        if (SurfaceInterface *surface = static_cast<SurfaceInterface*>(parent.internalPointer())) {
            const auto &children = surface->childSubSurfaces();
            return children.count();
        }
        return 0;
    }
    const int internalClientsCount = waylandServer() ? waylandServer()->internalClients().count() : 0;
    // toplevel are all windows
    return workspace()->allClientList().count() +
           workspace()->desktopList().count() +
           workspace()->unmanagedList().count() +
           internalClientsCount;
}

QModelIndex SurfaceTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0) {
        // invalid column
        return QModelIndex();
    }

    if (parent.isValid()) {
        using namespace KWayland::Server;
        if (SurfaceInterface *surface = static_cast<SurfaceInterface*>(parent.internalPointer())) {
            const auto &children = surface->childSubSurfaces();
            if (row < children.count()) {
                return createIndex(row, column, children.at(row)->surface().data());
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
    const auto &desktopClients = workspace()->desktopList();
    if (row < reference + desktopClients.count()) {
        return createIndex(row, column, desktopClients.at(row-reference)->surface());
    }
    reference += desktopClients.count();
    const auto &unmanaged = workspace()->unmanagedList();
    if (row < reference + unmanaged.count()) {
        return createIndex(row, column, unmanaged.at(row-reference)->surface());
    }
    reference += unmanaged.count();
    if (waylandServer()) {
        const auto &internal = waylandServer()->internalClients();
        if (row < reference + internal.count()) {
            return createIndex(row, column, internal.at(row-reference)->surface());
        }
    }
    // not found
    return QModelIndex();
}

QModelIndex SurfaceTreeModel::parent(const QModelIndex &child) const
{
    using namespace KWayland::Server;
    if (SurfaceInterface *surface = static_cast<SurfaceInterface*>(child.internalPointer())) {
        const auto &subsurface = surface->subSurface();
        if (subsurface.isNull()) {
            // doesn't reference a subsurface, this is a top-level window
            return QModelIndex();
        }
        SurfaceInterface *parent = subsurface->parentSurface().data();
        if (!parent) {
            // something is wrong
            return QModelIndex();
        }
        // is the parent a subsurface itself?
        if (parent->subSurface()) {
            auto grandParent = parent->subSurface()->parentSurface();
            if (grandParent.isNull()) {
                // something is wrong
                return QModelIndex();
            }
            const auto &children = grandParent->childSubSurfaces();
            for (int row = 0; row < children.count(); row++) {
                if (children.at(row).data() == parent->subSurface().data()) {
                    return createIndex(row, 0, parent);
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
        const auto &desktopClients = workspace()->desktopList();
        for (int i = 0; i < desktopClients.count(); i++) {
            if (desktopClients.at(i)->surface() == parent) {
                return createIndex(row + i, 0, parent);
            }
        }
        row += desktopClients.count();
        const auto &unmanaged = workspace()->unmanagedList();
        for (int i = 0; i < unmanaged.count(); i++) {
            if (unmanaged.at(i)->surface() == parent) {
                return createIndex(row + i, 0, parent);
            }
        }
        row += unmanaged.count();
        if (waylandServer()) {
            const auto &internal = waylandServer()->internalClients();
            for (int i = 0; i < internal.count(); i++) {
                if (internal.at(i)->surface() == parent) {
                    return createIndex(row + i, 0, parent);
                }
            }
        }
    }
    return QModelIndex();
}

QVariant SurfaceTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    using namespace KWayland::Server;
    if (SurfaceInterface *surface = static_cast<SurfaceInterface*>(index.internalPointer())) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            return QStringLiteral("%1 (%2) - %3").arg(surface->client()->executablePath())
                                                .arg(surface->client()->processId())
                                                .arg(surface->id());
        } else if (role == Qt::DecorationRole) {
            if (auto buffer = surface->buffer()) {
                if (buffer->shmBuffer()) {
                    return buffer->data().scaled(QSize(64, 64), Qt::KeepAspectRatio);
                }
            }
        }
    }
    return QVariant();
}

#if HAVE_INPUT
InputDeviceModel::InputDeviceModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_devices(LibInput::Connection::self()->devices())
{
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        setupDeviceConnections(*it);
    }
    auto c = LibInput::Connection::self();
    connect(c, &LibInput::Connection::deviceAdded, this,
        [this] (LibInput::Device *d) {
            beginInsertRows(QModelIndex(), m_devices.count(), m_devices.count());
            m_devices << d;
            setupDeviceConnections(d);
            endInsertRows();
        }
    );
    connect(c, &LibInput::Connection::deviceRemoved, this,
        [this] (LibInput::Device *d) {
            const int index = m_devices.indexOf(d);
            if (index == -1) {
                return;
            }
            beginRemoveRows(QModelIndex(), index, index);
            m_devices.removeAt(index);
            endRemoveRows();
        }
    );
}

InputDeviceModel::~InputDeviceModel() = default;


int InputDeviceModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2;
}

QVariant InputDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (!index.parent().isValid() && index.column() == 0) {
        const auto devices = LibInput::Connection::self()->devices();
        if (index.row() >= devices.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return devices.at(index.row())->name();
        }
    }
    if (index.parent().isValid()) {
        if (role == Qt::DisplayRole) {
            const auto device = LibInput::Connection::self()->devices().at(index.parent().row());
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
        if (row >= LibInput::Connection::self()->devices().at(parent.row())->metaObject()->propertyCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, quint32(row + 1) << 16 | parent.internalId());
    }
    if (row >= LibInput::Connection::self()->devices().count()) {
        return QModelIndex();
    }
    return createIndex(row, column, row + 1);
}

int InputDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return LibInput::Connection::self()->devices().count();
    }
    if (parent.internalId() & s_propertyBitMask) {
        return 0;
    }

    return LibInput::Connection::self()->devices().at(parent.row())->metaObject()->propertyCount();
}

QModelIndex InputDeviceModel::parent(const QModelIndex &child) const
{
    if (child.internalId() & s_propertyBitMask) {
        const quintptr parentId = child.internalId() & s_clientBitMask;
        return createIndex(parentId - 1, 0, parentId);
    }
    return QModelIndex();
}

void InputDeviceModel::setupDeviceConnections(LibInput::Device *device)
{
    connect(device, &LibInput::Device::enabledChanged, this,
        [this, device] {
            const QModelIndex parent = index(m_devices.indexOf(device), 0, QModelIndex());
            const QModelIndex child = index(device->metaObject()->indexOfProperty("enabled"), 1, parent);
            emit dataChanged(child, child, QVector<int>{Qt::DisplayRole});
        }
    );
    connect(device, &LibInput::Device::leftHandedChanged, this,
        [this, device] {
            const QModelIndex parent = index(m_devices.indexOf(device), 0, QModelIndex());
            const QModelIndex child = index(device->metaObject()->indexOfProperty("leftHanded"), 1, parent);
            emit dataChanged(child, child, QVector<int>{Qt::DisplayRole});
        }
    );
    connect(device, &LibInput::Device::pointerAccelerationChanged, this,
        [this, device] {
            const QModelIndex parent = index(m_devices.indexOf(device), 0, QModelIndex());
            const QModelIndex child = index(device->metaObject()->indexOfProperty("pointerAcceleration"), 1, parent);
            emit dataChanged(child, child, QVector<int>{Qt::DisplayRole});
        }
    );
}

#endif

}
