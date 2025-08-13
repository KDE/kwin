/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_backend.h"

#include "config-kwin.h"

#include "utils/xcbutils.h"
#include "x11_windowed_egl_backend.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"
#include "x11_windowed_qpainter_backend.h"
#include <pointer_input.h>
// KDE
#include <KLocalizedString>
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QSocketNotifier>
// xcb
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/shm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xinput.h>
#include <xkbcommon/xkbcommon-keysyms.h>
// system
#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <linux/input.h>
#include <ranges>
#include <unistd.h>
#include <xf86drm.h>

namespace KWin
{

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
}

bool X11WindowedInputDevice::isKeyboard() const
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

X11WindowedInputBackend::X11WindowedInputBackend(X11WindowedBackend *backend)
    : m_backend(backend)
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

X11WindowedBackend::X11WindowedBackend(const X11WindowedBackendOptions &options)
    : m_options(options)
{
}

X11WindowedBackend::~X11WindowedBackend()
{
    destroyOutputs();
    m_pointerDevice.reset();
    m_keyboardDevice.reset();
    m_touchDevice.reset();
    m_eglDisplay.reset();

    if (m_connection) {
        if (m_keySymbols) {
            xcb_key_symbols_free(m_keySymbols);
        }
        xcb_disconnect(m_connection);
        m_connection = nullptr;
    }
}

bool X11WindowedBackend::initialize()
{
    m_connection = xcb_connect(m_options.display.toLatin1(), &m_screenNumber);
    if (xcb_connection_has_error(m_connection)) {
        xcb_disconnect(m_connection);
        return false;
    }

    int screen = m_screenNumber;
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_connection));
         it.rem;
         --screen, xcb_screen_next(&it)) {
        if (screen == m_screenNumber) {
            m_screen = it.data;
        }
    }

    const xcb_query_extension_reply_t *presentExtension = xcb_get_extension_data(m_connection, &xcb_present_id);
    if (presentExtension && presentExtension->present) {
        m_presentOpcode = presentExtension->major_opcode;
        xcb_present_query_version_cookie_t cookie = xcb_present_query_version(m_connection, 1, 2);
        UniqueCPtr<xcb_present_query_version_reply_t> reply(xcb_present_query_version_reply(m_connection, cookie, nullptr));
        if (!reply) {
            qCWarning(KWIN_X11WINDOWED) << "Requested Present extension version is unsupported";
            return false;
        }
        m_presentMajorVersion = reply->major_version;
        m_presentMinorVersion = reply->minor_version;
    } else {
        qCWarning(KWIN_X11WINDOWED) << "Present X11 extension is unavailable";
        return false;
    }

    const xcb_query_extension_reply_t *shmExtension = xcb_get_extension_data(m_connection, &xcb_shm_id);
    if (shmExtension && shmExtension->present) {
        xcb_shm_query_version_cookie_t cookie = xcb_shm_query_version(m_connection);
        UniqueCPtr<xcb_shm_query_version_reply_t> reply(xcb_shm_query_version_reply(m_connection, cookie, nullptr));
        if (!reply) {
            qCWarning(KWIN_X11WINDOWED) << "Requested SHM extension version is unsupported";
        } else {
            if (!reply->shared_pixmaps) {
                qCWarning(KWIN_X11WINDOWED) << "X server supports SHM extension but not shared pixmaps";
            } else {
                m_hasShm = true;
            }
        }
    }

    const xcb_query_extension_reply_t *driExtension = xcb_get_extension_data(m_connection, &xcb_dri3_id);
    if (driExtension && driExtension->present) {
        xcb_dri3_query_version_cookie_t cookie = xcb_dri3_query_version(m_connection, 1, 2);
        UniqueCPtr<xcb_dri3_query_version_reply_t> reply(xcb_dri3_query_version_reply(m_connection, cookie, nullptr));
        if (reply) {
            m_hasDri = true;
            m_driMajorVersion = reply->major_version;
            m_driMinorVersion = reply->minor_version;
        } else {
            qCWarning(KWIN_X11WINDOWED) << "Requested DRI3 extension version is unsupported";
        }
    }

    initXInput();
    initDri3();
    initRender();

    createOutputs();

    m_pointerDevice = std::make_unique<X11WindowedInputDevice>();
    m_pointerDevice->setPointer(true);
    m_keyboardDevice = std::make_unique<X11WindowedInputDevice>();
    m_keyboardDevice->setKeyboard(true);
    if (m_hasXInput) {
        m_touchDevice = std::make_unique<X11WindowedInputDevice>();
        m_touchDevice->setTouch(true);
    }

    m_eventNotifier = std::make_unique<QSocketNotifier>(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read);
    auto processXcbEvents = [this] {
        while (auto event = xcb_poll_for_event(m_connection)) {
            handleEvent(event);
            free(event);
        }
        xcb_flush(m_connection);
    };
    connect(m_eventNotifier.get(), &QSocketNotifier::activated, this, processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, processXcbEvents);

    Q_EMIT outputsQueried();
    return true;
}

void X11WindowedBackend::initXInput()
{
#if HAVE_X11_XINPUT
    const xcb_query_extension_reply_t *xinputExtension = xcb_get_extension_data(m_connection, &xcb_input_id);
    if (!xinputExtension || !xinputExtension->present) {
        qCDebug(KWIN_X11WINDOWED) << "XInputExtension not present";
        return;
    }
    m_xiOpcode = xinputExtension->major_opcode;

    // verify that the XInput extension is at at least version 2.0
    auto cookie = xcb_input_xi_query_version(m_connection, 2, 2);
    UniqueCPtr<xcb_input_xi_query_version_reply_t> reply(xcb_input_xi_query_version_reply(m_connection, cookie, nullptr));
    if (!reply) {
        qCDebug(KWIN_X11WINDOWED) << "Failed to init XInput 2.2, trying 2.0";
        auto cookie = xcb_input_xi_query_version(m_connection, 2, 0);
        reply.reset(xcb_input_xi_query_version_reply(m_connection, cookie, nullptr));
        if (!reply) {
            qCDebug(KWIN_X11WINDOWED) << "Failed to init XInput";
            return;
        }
    }
    m_majorVersion = reply->major_version;
    m_minorVersion = reply->minor_version;
    m_hasXInput = m_majorVersion >= 2 && m_minorVersion >= 2;
#endif
}

void X11WindowedBackend::initDri3()
{
    if (m_hasDri) {
        xcb_dri3_open_cookie_t cookie = xcb_dri3_open(m_connection, m_screen->root, 0);
        UniqueCPtr<xcb_dri3_open_reply_t> reply(xcb_dri3_open_reply(m_connection, cookie, nullptr));
        if (reply && reply->nfd == 1) {
            int fd = xcb_dri3_open_reply_fds(m_connection, reply.get())[0];
            m_drmDevice = DrmDevice::open(QByteArray(drmGetDeviceNameFromFd2(fd)));
            ::close(fd);
        }
    }

    xcb_depth_iterator_t it = xcb_screen_allowed_depths_iterator(m_screen);
    while (it.rem > 0) {
        uint32_t format = driFormatForDepth(it.data->depth);
        if (format) {
            QList<uint64_t> &mods = m_driFormats[format];

            if (m_driMajorVersion > 1 || m_driMinorVersion >= 2) {
                xcb_dri3_get_supported_modifiers_cookie_t cookie = xcb_dri3_get_supported_modifiers(m_connection, m_screen->root, it.data->depth, 32);
                UniqueCPtr<xcb_dri3_get_supported_modifiers_reply_t> reply(xcb_dri3_get_supported_modifiers_reply(m_connection, cookie, nullptr));
                if (reply) {
                    const uint64_t *modifiers = xcb_dri3_get_supported_modifiers_screen_modifiers(reply.get());
                    const int modifierCount = xcb_dri3_get_supported_modifiers_screen_modifiers_length(reply.get());
                    for (int i = 0; i < modifierCount; ++i) {
                        mods.append(modifiers[i]);
                    }
                }
            }

            if (mods.isEmpty()) {
                mods.append(DRM_FORMAT_MOD_INVALID);
            }
        }

        xcb_depth_next(&it);
    }
}

void X11WindowedBackend::initRender()
{
    xcb_render_query_pict_formats_cookie_t cookie = xcb_render_query_pict_formats(m_connection);
    xcb_render_query_pict_formats_reply_t *reply = xcb_render_query_pict_formats_reply(m_connection, cookie, nullptr);
    if (reply) {
        for (xcb_render_pictforminfo_iterator_t it = xcb_render_query_pict_formats_formats_iterator(reply); it.rem; xcb_render_pictforminfo_next(&it)) {
            xcb_render_pictforminfo_t *info = it.data;
            if (info->depth == 32) {
                m_pictureFormats.insert(info->depth, info->id);
                break;
            }
        }
        free(reply);
    }
}

X11WindowedOutput *X11WindowedBackend::findOutput(xcb_window_t window) const
{
    const auto it = std::ranges::find_if(m_outputs, [window](X11WindowedOutput *output) {
        return output->window() == window;
    });
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
    const QSize pixelSize = m_options.outputSize * m_options.outputScale;
    for (int i = 0; i < m_options.outputCount; ++i) {
        auto *output = new X11WindowedOutput(this);
        output->init(pixelSize, m_options.outputScale, m_options.fullscreen);

        m_protocols = protocolsAtom;
        m_deleteWindowProtocol = deleteWindowAtom;

        xcb_change_property(m_connection,
                            XCB_PROP_MODE_REPLACE,
                            output->window(),
                            m_protocols,
                            XCB_ATOM_ATOM,
                            32, 1,
                            &m_deleteWindowProtocol);

        m_outputs << output;
        Q_EMIT outputAdded(output);
    }

    updateWindowTitle();

    xcb_flush(m_connection);
}

#if HAVE_X11_XINPUT

static inline qreal fixed1616ToReal(xcb_input_fp1616_t val)
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
        handleButtonPress(reinterpret_cast<xcb_button_press_event_t *>(e));
        break;
    case XCB_MOTION_NOTIFY: {
        auto event = reinterpret_cast<xcb_motion_notify_event_t *>(e);
        const X11WindowedOutput *output = findOutput(event->event);
        if (!output) {
            break;
        }
        const QPointF position = output->mapFromGlobal(QPointF(event->root_x, event->root_y));
        Q_EMIT m_pointerDevice->pointerMotionAbsolute(position, std::chrono::milliseconds(event->time), m_pointerDevice.get());
        Q_EMIT m_pointerDevice->pointerFrame(m_pointerDevice.get());
    } break;
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
        auto event = reinterpret_cast<xcb_key_press_event_t *>(e);
        if (eventType == XCB_KEY_PRESS) {
            if (!m_keySymbols) {
                m_keySymbols = xcb_key_symbols_alloc(m_connection);
            }
            const xcb_keysym_t kc = xcb_key_symbols_get_keysym(m_keySymbols, event->detail, 0);
            if (kc == XKB_KEY_Control_R) {
                grabKeyboard(event->time);
            }
            Q_EMIT m_keyboardDevice->keyChanged(event->detail - 8,
                                                KeyboardKeyState::Pressed,
                                                std::chrono::milliseconds(event->time),
                                                m_keyboardDevice.get());
        } else {
            Q_EMIT m_keyboardDevice->keyChanged(event->detail - 8,
                                                KeyboardKeyState::Released,
                                                std::chrono::milliseconds(event->time),
                                                m_keyboardDevice.get());
        }
    } break;
    case XCB_CONFIGURE_NOTIFY:
        updateSize(reinterpret_cast<xcb_configure_notify_event_t *>(e));
        break;
    case XCB_ENTER_NOTIFY: {
        auto event = reinterpret_cast<xcb_enter_notify_event_t *>(e);
        const X11WindowedOutput *output = findOutput(event->event);
        if (!output) {
            break;
        }
        const QPointF position = output->mapFromGlobal(QPointF(event->root_x, event->root_y));
        Q_EMIT m_pointerDevice->pointerMotionAbsolute(position, std::chrono::milliseconds(event->time), m_pointerDevice.get());
    } break;
    case XCB_CLIENT_MESSAGE:
        handleClientMessage(reinterpret_cast<xcb_client_message_event_t *>(e));
        break;
    case XCB_EXPOSE:
        handleExpose(reinterpret_cast<xcb_expose_event_t *>(e));
        break;
    case XCB_MAPPING_NOTIFY:
        if (m_keySymbols) {
            xcb_refresh_keyboard_mapping(m_keySymbols, reinterpret_cast<xcb_mapping_notify_event_t *>(e));
        }
        break;
    case XCB_GE_GENERIC: {
        xcb_ge_generic_event_t *ev = reinterpret_cast<xcb_ge_generic_event_t *>(e);
        if (ev->extension == m_presentOpcode) {
            handlePresentEvent(ev);
        } else if (ev->extension == m_xiOpcode) {
            handleXinputEvent(ev);
        }
        break;
    }
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
        const X11WindowedOutput *output = static_cast<X11WindowedOutput *>(m_outputs[0]);
        const auto c = xcb_grab_keyboard_unchecked(m_connection, false, output->window(), time,
                                                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        UniqueCPtr<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(m_connection, c, nullptr));
        if (!grab) {
            return;
        }
        if (grab->status == XCB_GRAB_STATUS_SUCCESS) {
            const auto c = xcb_grab_pointer_unchecked(m_connection, false, output->window(),
                                                      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
                                                      XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                                                      output->window(), XCB_CURSOR_NONE, time);
            UniqueCPtr<xcb_grab_pointer_reply_t> grab(xcb_grab_pointer_reply(m_connection, c, nullptr));
            if (!grab || grab->status != XCB_GRAB_STATUS_SUCCESS) {
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
    const QString title = QStringLiteral("%1 - %2").arg(i18n("KDE Wayland Compositor"), grab);
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->setWindowTitle(title);
    }
}

void X11WindowedBackend::handleClientMessage(xcb_client_message_event_t *event)
{
    auto it = std::ranges::find_if(std::as_const(m_outputs), [event](X11WindowedOutput *output) {
        return output->window() == event->window;
    });
    if (it == m_outputs.cend()) {
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

                Q_EMIT outputRemoved(removedOutput);
                removedOutput->unref();
                Q_EMIT outputsQueried();
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
        const int delta = (event->detail == XCB_BUTTON_INDEX_4 || event->detail == 6) ? -120 : 120;
        static const qreal s_defaultAxisStepDistance = 10.0;
        PointerAxis axis;
        if (event->detail > 5) {
            axis = PointerAxis::Horizontal;
        } else {
            axis = PointerAxis::Vertical;
        }
        Q_EMIT m_pointerDevice->pointerAxisChanged(axis,
                                                   delta * s_defaultAxisStepDistance,
                                                   delta,
                                                   PointerAxisSource::Unknown,
                                                   false,
                                                   std::chrono::milliseconds(event->time),
                                                   m_pointerDevice.get());
        Q_EMIT m_pointerDevice->pointerFrame(m_pointerDevice.get());
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
    Q_EMIT m_pointerDevice->pointerMotionAbsolute(position, std::chrono::milliseconds(event->time), m_pointerDevice.get());

    if (pressed) {
        Q_EMIT m_pointerDevice->pointerButtonChanged(button, PointerButtonState::Pressed, std::chrono::milliseconds(event->time), m_pointerDevice.get());
    } else {
        Q_EMIT m_pointerDevice->pointerButtonChanged(button, PointerButtonState::Released, std::chrono::milliseconds(event->time), m_pointerDevice.get());
    }
    Q_EMIT m_pointerDevice->pointerFrame(m_pointerDevice.get());
}

void X11WindowedBackend::handleExpose(xcb_expose_event_t *event)
{
    X11WindowedOutput *output = findOutput(event->window);
    if (output) {
        output->addExposedArea(QRect(event->x, event->y, event->width, event->height));
        output->renderLoop()->scheduleRepaint();
    }
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
        output->resize(s);
    }
}

void X11WindowedBackend::handleXinputEvent(xcb_ge_generic_event_t *ge)
{
#if HAVE_X11_XINPUT
    auto te = reinterpret_cast<xcb_input_motion_event_t *>(ge);
    const X11WindowedOutput *output = findOutput(te->event);
    if (!output) {
        return;
    }

    const QPointF position = output->mapFromGlobal(QPointF(fixed1616ToReal(te->root_x), fixed1616ToReal(te->root_y)));

    switch (ge->event_type) {
    case XCB_INPUT_TOUCH_BEGIN: {
        Q_EMIT m_touchDevice->touchDown(te->detail, position, std::chrono::milliseconds(te->time), m_touchDevice.get());
        Q_EMIT m_touchDevice->touchFrame(m_touchDevice.get());
        break;
    }
    case XCB_INPUT_TOUCH_UPDATE: {
        Q_EMIT m_touchDevice->touchMotion(te->detail, position, std::chrono::milliseconds(te->time), m_touchDevice.get());
        Q_EMIT m_touchDevice->touchFrame(m_touchDevice.get());
        break;
    }
    case XCB_INPUT_TOUCH_END: {
        Q_EMIT m_touchDevice->touchUp(te->detail, std::chrono::milliseconds(te->time), m_touchDevice.get());
        Q_EMIT m_touchDevice->touchFrame(m_touchDevice.get());
        break;
    }
    case XCB_INPUT_TOUCH_OWNERSHIP: {
        auto *te = reinterpret_cast<xcb_input_touch_ownership_event_t *>(ge);
        xcb_input_xi_allow_events(m_connection, te->time, te->deviceid, XCB_INPUT_EVENT_MODE_ACCEPT_TOUCH, te->touchid, te->event);
        break;
    }
    }
#endif
}

void X11WindowedBackend::handlePresentEvent(xcb_ge_generic_event_t *ge)
{
    switch (ge->event_type) {
    case XCB_PRESENT_EVENT_IDLE_NOTIFY: {
        xcb_present_idle_notify_event_t *idleNotify = reinterpret_cast<xcb_present_idle_notify_event_t *>(ge);
        if (X11WindowedOutput *output = findOutput(idleNotify->window)) {
            output->handlePresentIdleNotify(idleNotify);
        }
        break;
    }
    case XCB_PRESENT_EVENT_COMPLETE_NOTIFY: {
        xcb_present_complete_notify_event_t *completeNotify = reinterpret_cast<xcb_present_complete_notify_event_t *>(ge);
        if (X11WindowedOutput *output = findOutput(completeNotify->window)) {
            output->handlePresentCompleteNotify(completeNotify);
        }
        break;
    }
    }
}

xcb_window_t X11WindowedBackend::rootWindow() const
{
    if (!m_screen) {
        return XCB_WINDOW_NONE;
    }
    return m_screen->root;
}

DrmDevice *X11WindowedBackend::drmDevice() const
{
    return m_drmDevice.get();
}

X11WindowedInputDevice *X11WindowedBackend::pointerDevice() const
{
    return m_pointerDevice.get();
}

X11WindowedInputDevice *X11WindowedBackend::keyboardDevice() const
{
    return m_keyboardDevice.get();
}

X11WindowedInputDevice *X11WindowedBackend::touchDevice() const
{
    return m_touchDevice.get();
}

std::unique_ptr<EglBackend> X11WindowedBackend::createOpenGLBackend()
{
    return std::make_unique<X11WindowedEglBackend>(this);
}

std::unique_ptr<QPainterBackend> X11WindowedBackend::createQPainterBackend()
{
    return std::make_unique<X11WindowedQPainterBackend>(this);
}

std::unique_ptr<InputBackend> X11WindowedBackend::createInputBackend()
{
    return std::make_unique<X11WindowedInputBackend>(this);
}

xcb_connection_t *X11WindowedBackend::connection() const
{
    return m_connection;
}

xcb_screen_t *X11WindowedBackend::screen() const
{
    return m_screen;
}

int X11WindowedBackend::screenNumer() const
{
    return m_screenNumber;
}

bool X11WindowedBackend::hasXInput() const
{
    return m_hasXInput;
}

xcb_render_pictformat_t X11WindowedBackend::pictureFormatForDepth(int depth) const
{
    return m_pictureFormats.value(depth);
}

QHash<uint32_t, QList<uint64_t>> X11WindowedBackend::driFormats() const
{
    return m_driFormats;
}

uint32_t X11WindowedBackend::driFormatForDepth(int depth) const
{
    switch (depth) {
    case 24:
        return DRM_FORMAT_XRGB8888;
    case 32:
        return DRM_FORMAT_ARGB8888;
    default:
        return 0;
    }
}

int X11WindowedBackend::driMajorVersion() const
{
    return m_driMajorVersion;
}

int X11WindowedBackend::driMinorVersion() const
{
    return m_driMinorVersion;
}

QList<CompositingType> X11WindowedBackend::supportedCompositors() const
{
    QList<CompositingType> ret;
    if (m_drmDevice) {
        ret.append(OpenGLCompositing);
    }
    if (m_hasShm) {
        ret.append(QPainterCompositing);
    }
    return ret;
}

QList<BackendOutput *> X11WindowedBackend::outputs() const
{
    return m_outputs | std::ranges::to<QList<BackendOutput *>>();
}

void X11WindowedBackend::destroyOutputs()
{
    while (!m_outputs.isEmpty()) {
        auto output = m_outputs.takeLast();
        Q_EMIT outputRemoved(output);
        delete output;
    }
}

void X11WindowedBackend::setEglDisplay(std::unique_ptr<EglDisplay> &&display)
{
    m_eglDisplay = std::move(display);
}

EglDisplay *X11WindowedBackend::sceneEglDisplayObject() const
{
    return m_eglDisplay.get();
}

} // namespace KWin

#include "moc_x11_windowed_backend.cpp"
