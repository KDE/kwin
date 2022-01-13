/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11windowed_backend.h"
#include "x11windowed_output.h"
#include <config-kwin.h>
#include "scene_qpainter_x11_backend.h"
#include "logging.h"
#include "wayland_server.h"
#include "utils/xcbutils.h"
#include "egl_x11_backend.h"
#include "screens.h"
#include "session.h"
#include <kwinxrenderutils.h>
#include <cursor.h>
#include <pointer_input.h>
// KDE
#include <KLocalizedString>
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QSocketNotifier>
// xcb
#include <xcb/xcb_keysyms.h>
// X11
#if HAVE_X11_XINPUT
#include "ge_event_mem_mover.h"
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>
#endif
// system
#include <linux/input.h>
#include <X11/Xlib-xcb.h>
#include <X11/keysym.h>

namespace KWin
{

X11WindowedInputDevice::X11WindowedInputDevice(QObject *parent)
    : InputDevice(parent)
{
}

void X11WindowedInputDevice::setPointer(bool set)
{
    m_pointer = set;
}

void X11WindowedInputDevice::setKeyboard(bool set)
{
    m_keyboard = set;
}

void X11WindowedInputDevice::setTouch(bool set)
{
    m_touch = set;
}

void X11WindowedInputDevice::setName(const QString &name)
{
    m_name = name;
}

QString X11WindowedInputDevice::sysName() const
{
    return QString();
}

QString X11WindowedInputDevice::name() const
{
    return m_name;
}

bool X11WindowedInputDevice::isEnabled() const
{
    return true;
}

void X11WindowedInputDevice::setEnabled(bool enabled)
{
    Q_UNUSED(enabled)
}

LEDs X11WindowedInputDevice::leds() const
{
    return LEDs();
}

void X11WindowedInputDevice::setLeds(LEDs leds)
{
    Q_UNUSED(leds)
}

bool X11WindowedInputDevice::isKeyboard() const
{
    return m_keyboard;
}

bool X11WindowedInputDevice::isAlphaNumericKeyboard() const
{
    return m_keyboard;
}

bool X11WindowedInputDevice::isPointer() const
{
    return m_pointer;
}

bool X11WindowedInputDevice::isTouchpad() const
{
    return false;
}

bool X11WindowedInputDevice::isTouch() const
{
    return m_touch;
}

bool X11WindowedInputDevice::isTabletTool() const
{
    return false;
}

bool X11WindowedInputDevice::isTabletPad() const
{
    return false;
}

bool X11WindowedInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool X11WindowedInputDevice::isLidSwitch() const
{
    return false;
}

X11WindowedInputBackend::X11WindowedInputBackend(X11WindowedBackend *backend, QObject *parent)
    : InputBackend(parent)
    , m_backend(backend)
{
}

void X11WindowedInputBackend::initialize()
{
    if (m_backend->pointerDevice()) {
        Q_EMIT deviceAdded(m_backend->pointerDevice());
    }
    if (m_backend->keyboardDevice()) {
        Q_EMIT deviceAdded(m_backend->keyboardDevice());
    }
    if (m_backend->touchDevice()) {
        Q_EMIT deviceAdded(m_backend->touchDevice());
    }
}

X11WindowedBackend::X11WindowedBackend(QObject *parent)
    : Platform(parent)
    , m_session(Session::create(Session::Type::Noop, this))
{
    setSupportsPointerWarping(true);
    setPerScreenRenderingEnabled(true);
}

X11WindowedBackend::~X11WindowedBackend()
{
    delete m_pointerDevice;
    delete m_keyboardDevice;
    delete m_touchDevice;

    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }
    if (m_connection) {
        if (m_keySymbols) {
            xcb_key_symbols_free(m_keySymbols);
        }
        if (m_cursor) {
            xcb_free_cursor(m_connection, m_cursor);
        }
        xcb_disconnect(m_connection);
    }
}

bool X11WindowedBackend::initialize()
{
    int screen = 0;
    xcb_connection_t *c = nullptr;
    Display *xDisplay = XOpenDisplay(deviceIdentifier().constData());
    if (xDisplay) {
        c = XGetXCBConnection(xDisplay);
        XSetEventQueueOwner(xDisplay, XCBOwnsEventQueue);
        screen = XDefaultScreen(xDisplay);
    }
    if (c && !xcb_connection_has_error(c)) {
        m_connection = c;
        m_screenNumber = screen;
        m_display = xDisplay;
        for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_connection));
            it.rem;
            --screen, xcb_screen_next(&it)) {
            if (screen == m_screenNumber) {
                m_screen = it.data;
            }
        }
        initXInput();
        XRenderUtils::init(m_connection, m_screen->root);
        createOutputs();
        connect(kwinApp(), &Application::workspaceCreated, this, &X11WindowedBackend::startEventReading);
        connect(Cursors::self(), &Cursors::currentCursorChanged, this,
            [this] {
                KWin::Cursor* c = KWin::Cursors::self()->currentCursor();
                createCursor(c->image(), c->hotspot());
            }
        );
        setReady(true);
        m_pointerDevice = new X11WindowedInputDevice(this);
        m_pointerDevice->setPointer(true);
        m_keyboardDevice = new X11WindowedInputDevice(this);
        m_keyboardDevice->setKeyboard(true);
        if (m_hasXInput) {
            m_touchDevice = new X11WindowedInputDevice(this);
            m_touchDevice->setTouch(true);
        }
        Q_EMIT screensQueried();
        return true;
    } else {
        return false;
    }
}

Session *X11WindowedBackend::session() const
{
    return m_session;
}

void X11WindowedBackend::initXInput()
{
#if HAVE_X11_XINPUT
    int xi_opcode, event, error;
    // init XInput extension
    if (!XQueryExtension(m_display, "XInputExtension", &xi_opcode, &event, &error)) {
        qCDebug(KWIN_X11WINDOWED) << "XInputExtension not present";
        return;
    }

    // verify that the XInput extension is at at least version 2.0
    int major = 2, minor = 2;
    int result = XIQueryVersion(m_display, &major, &minor);
    if (result != Success) {
        qCDebug(KWIN_X11WINDOWED) << "Failed to init XInput 2.2, trying 2.0";
        minor = 0;
        if (XIQueryVersion(m_display, &major, &minor) != Success) {
            qCDebug(KWIN_X11WINDOWED) << "Failed to init XInput";
            return;
        }
    }
    m_xiOpcode = xi_opcode;
    m_majorVersion = major;
    m_minorVersion = minor;
    m_hasXInput = m_majorVersion >=2 && m_minorVersion >= 2;
#endif
}

X11WindowedOutput *X11WindowedBackend::findOutput(xcb_window_t window) const
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(),
        [window] (X11WindowedOutput *output) {
            return output->window() == window;
        }
    );
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

void X11WindowedBackend::createOutputs()
{
    Xcb::Atom protocolsAtom(QByteArrayLiteral("WM_PROTOCOLS"), false, m_connection);
    Xcb::Atom deleteWindowAtom(QByteArrayLiteral("WM_DELETE_WINDOW"), false, m_connection);

    // we need to multiply the initial window size with the scale in order to
    // create an output window of this size in the end
    const int pixelWidth = initialWindowSize().width() * initialOutputScale() + 0.5;
    const int pixelHeight = initialWindowSize().height() * initialOutputScale() + 0.5;
    const int logicalWidth = initialWindowSize().width();

    int logicalWidthSum = 0;
    for (int i = 0; i < initialOutputCount(); ++i) {
        auto *output = new X11WindowedOutput(this);
        output->init(QPoint(logicalWidthSum, 0), QSize(pixelWidth, pixelHeight));

        m_protocols = protocolsAtom;
        m_deleteWindowProtocol = deleteWindowAtom;

        xcb_change_property(m_connection,
                            XCB_PROP_MODE_REPLACE,
                            output->window(),
                            m_protocols,
                            XCB_ATOM_ATOM,
                            32, 1,
                            &m_deleteWindowProtocol);

        logicalWidthSum += logicalWidth;
        m_outputs << output;
        Q_EMIT outputAdded(output);
        Q_EMIT outputEnabled(output);
    }

    updateWindowTitle();

    xcb_flush(m_connection);
}

void X11WindowedBackend::startEventReading()
{
    QSocketNotifier *notifier = new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this);
    auto processXcbEvents = [this] {
        while (auto event = xcb_poll_for_event(m_connection)) {
            handleEvent(event);
            free(event);
        }
        xcb_flush(m_connection);
    };
    connect(notifier, &QSocketNotifier::activated, this, processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, processXcbEvents);
}

#if HAVE_X11_XINPUT

static inline qreal fixed1616ToReal(FP1616 val)
{
    return (val) * 1.0 / (1 << 16);
}
#endif

void X11WindowedBackend::handleEvent(xcb_generic_event_t *e)
{
    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        handleButtonPress(reinterpret_cast<xcb_button_press_event_t*>(e));
        break;
    case XCB_MOTION_NOTIFY: {
            auto event = reinterpret_cast<xcb_motion_notify_event_t*>(e);
            const X11WindowedOutput *output = findOutput(event->event);
            if (!output) {
                break;
            }
            const QPointF position = output->mapFromGlobal(QPointF(event->root_x, event->root_y));
            Q_EMIT m_pointerDevice->pointerMotionAbsolute(position, event->time, m_pointerDevice);
        }
        break;
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
            auto event = reinterpret_cast<xcb_key_press_event_t*>(e);
            if (eventType == XCB_KEY_PRESS) {
                if (!m_keySymbols) {
                    m_keySymbols = xcb_key_symbols_alloc(m_connection);
                }
                const xcb_keysym_t kc = xcb_key_symbols_get_keysym(m_keySymbols, event->detail, 0);
                if (kc == XK_Control_R) {
                    grabKeyboard(event->time);
                }
                Q_EMIT m_keyboardDevice->keyChanged(event->detail - 8,
                                                    InputRedirection::KeyboardKeyPressed,
                                                    event->time,
                                                    m_keyboardDevice);
            } else {
                Q_EMIT m_keyboardDevice->keyChanged(event->detail - 8,
                                                    InputRedirection::KeyboardKeyReleased,
                                                    event->time,
                                                    m_keyboardDevice);
            }
        }
        break;
    case XCB_CONFIGURE_NOTIFY:
        updateSize(reinterpret_cast<xcb_configure_notify_event_t*>(e));
        break;
    case XCB_ENTER_NOTIFY: {
            auto event = reinterpret_cast<xcb_enter_notify_event_t*>(e);
            const X11WindowedOutput *output = findOutput(event->event);
            if (!output) {
                break;
            }
            const QPointF position = output->mapFromGlobal(QPointF(event->root_x, event->root_y));
            Q_EMIT m_pointerDevice->pointerMotionAbsolute(position, event->time, m_pointerDevice);
        }
        break;
    case XCB_CLIENT_MESSAGE:
        handleClientMessage(reinterpret_cast<xcb_client_message_event_t*>(e));
        break;
    case XCB_EXPOSE:
        handleExpose(reinterpret_cast<xcb_expose_event_t*>(e));
        break;
    case XCB_MAPPING_NOTIFY:
        if (m_keySymbols) {
            xcb_refresh_keyboard_mapping(m_keySymbols, reinterpret_cast<xcb_mapping_notify_event_t*>(e));
        }
        break;
#if HAVE_X11_XINPUT
    case XCB_GE_GENERIC: {
        GeEventMemMover ge(e);
        auto te = reinterpret_cast<xXIDeviceEvent*>(e);
        const X11WindowedOutput *output = findOutput(te->event);
        if (!output) {
            break;
        }

        const QPointF position = output->mapFromGlobal(QPointF(fixed1616ToReal(te->root_x), fixed1616ToReal(te->root_y)));

        switch (ge->event_type) {

        case XI_TouchBegin: {
            Q_EMIT m_touchDevice->touchDown(te->detail, position, te->time, m_touchDevice);
            Q_EMIT m_touchDevice->touchFrame(m_touchDevice);
            break;
        }
        case XI_TouchUpdate: {
            Q_EMIT m_touchDevice->touchMotion(te->detail, position, te->time, m_touchDevice);
            Q_EMIT m_touchDevice->touchFrame(m_touchDevice);
            break;
        }
        case XI_TouchEnd: {
            Q_EMIT m_touchDevice->touchUp(te->detail, te->time, m_touchDevice);
            Q_EMIT m_touchDevice->touchFrame(m_touchDevice);
            break;
        }
        case XI_TouchOwnership: {
            auto te = reinterpret_cast<xXITouchOwnershipEvent*>(e);
            XIAllowTouchEvents(m_display, te->deviceid, te->sourceid, te->touchid, XIAcceptTouch);
            break;
        }
        }
        break;
    }
#endif
    default:
        break;
    }
}

void X11WindowedBackend::grabKeyboard(xcb_timestamp_t time)
{
    const bool oldState = m_keyboardGrabbed;
    if (m_keyboardGrabbed) {
        xcb_ungrab_keyboard(m_connection, time);
        xcb_ungrab_pointer(m_connection, time);
        m_keyboardGrabbed = false;
    } else {
        const auto c = xcb_grab_keyboard_unchecked(m_connection, false, window(), time,
                                                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        ScopedCPointer<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(m_connection, c, nullptr));
        if (grab.isNull()) {
            return;
        }
        if (grab->status == XCB_GRAB_STATUS_SUCCESS) {
            const auto c = xcb_grab_pointer_unchecked(m_connection, false, window(),
                                                      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                                                      XCB_EVENT_MASK_POINTER_MOTION |
                                                      XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
                                                      XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                                                      window(), XCB_CURSOR_NONE, time);
            ScopedCPointer<xcb_grab_pointer_reply_t> grab(xcb_grab_pointer_reply(m_connection, c, nullptr));
            if (grab.isNull() || grab->status != XCB_GRAB_STATUS_SUCCESS) {
                xcb_ungrab_keyboard(m_connection, time);
                return;
            }
            m_keyboardGrabbed = true;
        }
    }
    if (oldState != m_keyboardGrabbed) {
        updateWindowTitle();
        xcb_flush(m_connection);
    }
}

void X11WindowedBackend::updateWindowTitle()
{
    const QString grab = m_keyboardGrabbed ? i18n("Press right control to ungrab input") : i18n("Press right control key to grab input");
    const QString title = QStringLiteral("%1 (%2) - %3").arg(i18n("KDE Wayland Compositor"),
                                                             waylandServer()->socketName(),
                                                             grab);
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->setWindowTitle(title);
    }
}

void X11WindowedBackend::handleClientMessage(xcb_client_message_event_t *event)
{
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                           [event] (X11WindowedOutput *output) { return output->window() == event->window; }
              );
    if (it == m_outputs.end()) {
        return;
    }
    if (event->type == m_protocols && m_protocols != XCB_ATOM_NONE) {
        if (event->data.data32[0] == m_deleteWindowProtocol && m_deleteWindowProtocol != XCB_ATOM_NONE) {
            if (m_outputs.count() == 1) {
                qCDebug(KWIN_X11WINDOWED) << "Backend window is going to be closed, shutting down.";
                QCoreApplication::quit();
            } else {
                // remove the window
                qCDebug(KWIN_X11WINDOWED) << "Removing one output window.";

                auto removedOutput = *it;
                it = m_outputs.erase(it);

                // update the sizes
                int x = removedOutput->internalPosition().x();
                for (; it != m_outputs.end(); ++it) {
                    (*it)->setGeometry(QPoint(x, 0), (*it)->pixelSize());
                    x += (*it)->geometry().width();
                }

                Q_EMIT outputDisabled(removedOutput);
                Q_EMIT outputRemoved(removedOutput);
                delete removedOutput;
                QMetaObject::invokeMethod(screens(), "updateCount");
            }
        }
    }
}

void X11WindowedBackend::handleButtonPress(xcb_button_press_event_t *event)
{
    const X11WindowedOutput *output = findOutput(event->event);
    if (!output) {
        return;
    }
    bool const pressed = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
    if (event->detail >= XCB_BUTTON_INDEX_4 && event->detail <= 7) {
        // wheel
        if (!pressed) {
            return;
        }
        const int delta = (event->detail == XCB_BUTTON_INDEX_4 || event->detail == 6) ? -1 : 1;
        static const qreal s_defaultAxisStepDistance = 10.0;
        InputRedirection::PointerAxis axis;
        if (event->detail > 5) {
            axis = InputRedirection::PointerAxisHorizontal;
        } else {
            axis = InputRedirection::PointerAxisVertical;
        }
        Q_EMIT m_pointerDevice->pointerAxisChanged(axis,
                                                   delta * s_defaultAxisStepDistance,
                                                   delta,
                                                   InputRedirection::PointerAxisSourceUnknown,
                                                   event->time,
                                                   m_pointerDevice);
        return;
    }
    uint32_t button = 0;
    switch (event->detail) {
    case XCB_BUTTON_INDEX_1:
        button = BTN_LEFT;
        break;
    case XCB_BUTTON_INDEX_2:
        button = BTN_MIDDLE;
        break;
    case XCB_BUTTON_INDEX_3:
        button = BTN_RIGHT;
        break;
    default:
        button = event->detail + BTN_LEFT - 1;
        return;
    }

    const QPointF position = output->mapFromGlobal(QPointF(event->root_x, event->root_y));
    Q_EMIT m_pointerDevice->pointerMotionAbsolute(position, event->time, m_pointerDevice);

    if (pressed) {
        Q_EMIT m_pointerDevice->pointerButtonChanged(button, InputRedirection::PointerButtonPressed, event->time, m_pointerDevice);
    } else {
        Q_EMIT m_pointerDevice->pointerButtonChanged(button, InputRedirection::PointerButtonReleased, event->time, m_pointerDevice);
    }
}

void X11WindowedBackend::handleExpose(xcb_expose_event_t *event)
{
    repaint(QRect(event->x, event->y, event->width, event->height));
}

void X11WindowedBackend::updateSize(xcb_configure_notify_event_t *event)
{
    X11WindowedOutput *output = findOutput(event->window);
    if (!output) {
        return;
    }

    output->setHostPosition(QPoint(event->x, event->y));

    const QSize s = QSize(event->width, event->height);
    if (s != output->pixelSize()) {
        output->setGeometry(output->internalPosition(), s);
    }
    Q_EMIT sizeChanged();
}

void X11WindowedBackend::createCursor(const QImage &srcImage, const QPoint &hotspot)
{
    const xcb_pixmap_t pix = xcb_generate_id(m_connection);
    const xcb_gcontext_t gc = xcb_generate_id(m_connection);
    const xcb_cursor_t cid = xcb_generate_id(m_connection);

    //right now on X we only have one scale between all screens, and we know we will have at least one screen
    const qreal outputScale = 1;
    const QSize targetSize = srcImage.size() * outputScale / srcImage.devicePixelRatio();
    const QImage img = srcImage.scaled(targetSize, Qt::KeepAspectRatio);

    xcb_create_pixmap(m_connection, 32, pix, m_screen->root, img.width(), img.height());
    xcb_create_gc(m_connection, gc, pix, 0, nullptr);

    xcb_put_image(m_connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, img.width(), img.height(), 0, 0, 0, 32, img.sizeInBytes(), img.constBits());

    XRenderPicture pic(pix, 32);
    xcb_render_create_cursor(m_connection, cid, pic, qRound(hotspot.x() * outputScale), qRound(hotspot.y() * outputScale));
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        xcb_change_window_attributes(m_connection, (*it)->window(), XCB_CW_CURSOR, &cid);
    }

    xcb_free_pixmap(m_connection, pix);
    xcb_free_gc(m_connection, gc);
    if (m_cursor) {
        xcb_free_cursor(m_connection, m_cursor);
    }
    m_cursor = cid;
    xcb_flush(m_connection);
}

xcb_window_t X11WindowedBackend::rootWindow() const
{
    if (!m_screen) {
        return XCB_WINDOW_NONE;
    }
    return m_screen->root;
}

X11WindowedInputDevice *X11WindowedBackend::pointerDevice() const
{
    return m_pointerDevice;
}

X11WindowedInputDevice *X11WindowedBackend::keyboardDevice() const
{
    return m_keyboardDevice;
}

X11WindowedInputDevice *X11WindowedBackend::touchDevice() const
{
    return m_touchDevice;
}

OpenGLBackend *X11WindowedBackend::createOpenGLBackend()
{
    return  new EglX11Backend(this);
}

QPainterBackend *X11WindowedBackend::createQPainterBackend()
{
    return new X11WindowedQPainterBackend(this);
}

InputBackend *X11WindowedBackend::createInputBackend()
{
    return new X11WindowedInputBackend(this);
}

void X11WindowedBackend::warpPointer(const QPointF &globalPos)
{
    const xcb_window_t w = m_outputs.at(0)->window();
    xcb_warp_pointer(m_connection, w, w, 0, 0, 0, 0, globalPos.x(), globalPos.y());
    xcb_flush(m_connection);
}

xcb_window_t X11WindowedBackend::windowForScreen(AbstractOutput *output) const
{
    if (!output) {
        return XCB_WINDOW_NONE;
    }
    return static_cast<X11WindowedOutput*>(output)->window();
}

xcb_window_t X11WindowedBackend::window() const
{
    return m_outputs.first()->window();
}

Outputs X11WindowedBackend::outputs() const
{
    return m_outputs;
}

Outputs X11WindowedBackend::enabledOutputs() const
{
    return m_outputs;
}

}
