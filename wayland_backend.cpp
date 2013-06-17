/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
// own
#include "wayland_backend.h"
// KWin
#include "cursor.h"
// Qt
#include <QDebug>
#include <QImage>
#include <QSocketNotifier>
#include <QTemporaryFile>
// xcb
#include <xcb/xtest.h>
#include <xcb/xfixes.h>
// Wayland
#include <wayland-client-protocol.h>
// system
#include <linux/input.h>
#include <unistd.h>
#include <sys/mman.h>

namespace KWin
{
namespace Wayland
{

/**
 * Callback for announcing global objects in the registry
 **/
static void registryHandleGlobal(void *data, struct wl_registry *registry,
                                 uint32_t name, const char *interface, uint32_t version)
{
    Q_UNUSED(version)
    WaylandBackend *d = reinterpret_cast<WaylandBackend*>(data);

    if (strcmp(interface, "wl_compositor") == 0) {
        d->setCompositor(reinterpret_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1)));
    } else if (strcmp(interface, "wl_shell") == 0) {
        d->setShell(reinterpret_cast<wl_shell *>(wl_registry_bind(registry, name, &wl_shell_interface, 1)));
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->createSeat(name);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->createShm(name);
    }
    qDebug() << "Wayland Interface: " << interface;
}

/**
 * Callback for removal of global objects in the registry
 **/
static void registryHandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
    // TODO: implement me
}

/**
 * Call back for ping from Wayland Shell.
 **/
static void handlePing(void *data, struct wl_shell_surface *shellSurface, uint32_t serial)
{
    Q_UNUSED(shellSurface);
    reinterpret_cast<WaylandBackend*>(data)->ping(serial);
}

/**
 * Callback for a configure request for a shell surface
 **/
static void handleConfigure(void *data, struct wl_shell_surface *shellSurface, uint32_t edges, int32_t width, int32_t height)
{
    Q_UNUSED(shellSurface)
    Q_UNUSED(edges)
    WaylandBackend *display = reinterpret_cast<WaylandBackend*>(data);
    display->setShellSurfaceSize(QSize(width, height));
    // TODO: this information should probably go into Screens
}

/**
 * Callback for popups - not needed, we don't have popups
 **/
static void handlePopupDone(void *data, struct wl_shell_surface *shellSurface)
{
    Q_UNUSED(data)
    Q_UNUSED(shellSurface)
}

static void seatHandleCapabilities(void *data, wl_seat *seat, uint32_t capabilities)
{
    WaylandSeat *s = reinterpret_cast<WaylandSeat*>(data);
    if (seat != s->seat()) {
        return;
    }
    s->changed(capabilities);
}

static void pointerHandleEnter(void *data, wl_pointer *pointer, uint32_t serial, wl_surface *surface,
                               wl_fixed_t sx, wl_fixed_t sy)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    Q_UNUSED(surface)
    Q_UNUSED(sx)
    Q_UNUSED(sy)
    WaylandSeat *seat = reinterpret_cast<WaylandSeat*>(data);
    seat->pointerEntered(serial);
}

static void pointerHandleLeave(void *data, wl_pointer *pointer, uint32_t serial, wl_surface *surface)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    Q_UNUSED(serial)
    Q_UNUSED(surface)
}

static void pointerHandleMotion(void *data, wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    Q_UNUSED(time)
    xcb_test_fake_input(connection(), XCB_MOTION_NOTIFY, 0, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE,
                        wl_fixed_to_int(sx), wl_fixed_to_int(sy), 0);
}

static void pointerHandleButton(void *data, wl_pointer *pointer, uint32_t serial, uint32_t time,
                                uint32_t button, uint32_t state)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    Q_UNUSED(serial)
    Q_UNUSED(time)
    uint8_t type = XCB_BUTTON_PRESS;
    if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
        type = XCB_BUTTON_RELEASE;
    }
    // TODO: there must be a better way for mapping
    uint8_t xButton = 0;
    switch (button) {
    case BTN_LEFT:
        xButton = XCB_BUTTON_INDEX_1;
        break;
    case BTN_RIGHT:
        xButton = XCB_BUTTON_INDEX_3;
        break;
    case BTN_MIDDLE:
        xButton = XCB_BUTTON_INDEX_2;
        break;
    default:
        // TODO: add more buttons
        return;
    }
    xcb_test_fake_input(connection(), type, xButton, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
}

static void pointerHandleAxis(void *data, wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    Q_UNUSED(time)
    uint8_t xButton = 0;
    const int delta = wl_fixed_to_int(value);
    if (delta == 0) {
        return;
    }
    switch (axis) {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        xButton = delta > 0 ? XCB_BUTTON_INDEX_5 : XCB_BUTTON_INDEX_4;
        break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        // no enum values defined for buttons larger than 5
        xButton = delta > 0 ? 7 : 6;
        break;
    default:
        // doesn't exist
        return;
    }
    for (int i = 0; i < qAbs(delta); ++i) {
        xcb_test_fake_input(connection(), XCB_BUTTON_PRESS, xButton, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        xcb_test_fake_input(connection(), XCB_BUTTON_RELEASE, xButton, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
    }
}

static void keyboardHandleKeymap(void *data, wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(format)
    Q_UNUSED(fd)
    Q_UNUSED(size)
}

static void keyboardHandleEnter(void *data, wl_keyboard *keyboard,
                      uint32_t serial, wl_surface *surface,
                      wl_array *keys)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    Q_UNUSED(surface)
    Q_UNUSED(keys)
}

static void keyboardHandleLeave(void *data, wl_keyboard *keyboard, uint32_t serial, wl_surface *surface)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    Q_UNUSED(surface)
}

static void keyboardHandleKey(void *data, wl_keyboard *keyboard, uint32_t serial, uint32_t time,
                              uint32_t key, uint32_t state)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    Q_UNUSED(time)
    uint8_t type = XCB_KEY_PRESS;
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        type = XCB_KEY_RELEASE;
    }
    xcb_test_fake_input(connection(), type, key + 8 /*magic*/, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
}

static void keyboardHandleModifiers(void *data, wl_keyboard *keyboard, uint32_t serial, uint32_t modsDepressed,
                                    uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    Q_UNUSED(modsDepressed)
    Q_UNUSED(modsLatched)
    Q_UNUSED(modsLocked)
    Q_UNUSED(group)
}

// handlers
static const struct wl_registry_listener s_registryListener = {
    registryHandleGlobal,
    registryHandleGlobalRemove
};

static const struct wl_shell_surface_listener s_shellSurfaceListener = {
    handlePing,
    handleConfigure,
    handlePopupDone
};

static const struct wl_pointer_listener s_pointerListener = {
    pointerHandleEnter,
    pointerHandleLeave,
    pointerHandleMotion,
    pointerHandleButton,
    pointerHandleAxis
};

static const struct wl_keyboard_listener s_keyboardListener = {
    keyboardHandleKeymap,
    keyboardHandleEnter,
    keyboardHandleLeave,
    keyboardHandleKey,
    keyboardHandleModifiers,
};

static const struct wl_seat_listener s_seatListener = {
    seatHandleCapabilities
};

CursorData::CursorData(ShmPool *pool)
    : m_cursor(NULL)
    , m_valid(init(pool))
{
}

CursorData::~CursorData()
{
}

bool CursorData::init(ShmPool *pool)
{
    QScopedPointer<xcb_xfixes_get_cursor_image_reply_t, QScopedPointerPodDeleter> cursor(
        xcb_xfixes_get_cursor_image_reply(connection(),
                                          xcb_xfixes_get_cursor_image_unchecked(connection()),
                                          NULL));
    if (cursor.isNull()) {
        return false;
    }

    QImage cursorImage((uchar *) xcb_xfixes_get_cursor_image_cursor_image(cursor.data()), cursor->width, cursor->height,
                       QImage::Format_ARGB32_Premultiplied);
    if (cursorImage.isNull()) {
        return false;
    }
    m_size = QSize(cursor->width, cursor->height);

    m_cursor = pool->createBuffer(cursorImage);
    if (!m_cursor) {
        qDebug() << "Creating cursor buffer failed";
        return false;
    }

    m_hotSpot = QPoint(cursor->xhot, cursor->yhot);
    return true;
}

X11CursorTracker::X11CursorTracker(wl_pointer *pointer, WaylandBackend *backend, QObject* parent)
    : QObject(parent)
    , m_pointer(pointer)
    , m_backend(backend)
    , m_cursor(wl_compositor_create_surface(backend->compositor()))
    , m_enteredSerial(0)
    , m_lastX11Cursor(0)
{
    Cursor::self()->startCursorTracking();
    connect(Cursor::self(), SIGNAL(cursorChanged(uint32_t)), SLOT(cursorChanged(uint32_t)));
}

X11CursorTracker::~X11CursorTracker()
{
    if (Cursor::self()) {
        // Cursor might have been destroyed before Wayland backend gets destroyed
        Cursor::self()->stopCursorTracking();
    }
    if (m_cursor) {
        wl_surface_destroy(m_cursor);
    }
}

void X11CursorTracker::cursorChanged(uint32_t serial)
{
    if (m_lastX11Cursor == serial) {
        // not changed;
        return;
    }
    m_lastX11Cursor = serial;
    QHash<uint32_t, CursorData>::iterator it = m_cursors.find(serial);
    if (it != m_cursors.end()) {
        installCursor(it.value());
        return;
    }
    ShmPool *pool = m_backend->shmPool();
    if (!pool) {
        return;
    }
    CursorData cursor(pool);
    if (cursor.isValid()) {
        // TODO: discard unused cursors after some time?
        m_cursors.insert(serial, cursor);
    }
    installCursor(cursor);
}

void X11CursorTracker::installCursor(const CursorData& cursor)
{
    wl_pointer_set_cursor(m_pointer, m_enteredSerial, m_cursor, cursor.hotSpot().x(), cursor.hotSpot().y());
    wl_surface_attach(m_cursor, cursor.cursor(), 0, 0);
    wl_surface_damage(m_cursor, 0, 0, cursor.size().width(), cursor.size().height());
    wl_surface_commit(m_cursor);
}

void X11CursorTracker::setEnteredSerial(uint32_t serial)
{
    m_enteredSerial = serial;
}

void X11CursorTracker::resetCursor()
{
    QHash<uint32_t, CursorData>::iterator it = m_cursors.find(m_lastX11Cursor);
    if (it != m_cursors.end()) {
        installCursor(it.value());
    }
}

ShmPool::ShmPool(wl_shm *shm)
    : m_shm(shm)
    , m_pool(NULL)
    , m_poolData(NULL)
    , m_size(1024 * 1024) // TODO: useful size?
    , m_tmpFile(new QTemporaryFile())
    , m_valid(createPool())
    , m_offset(0)
{
}

ShmPool::~ShmPool()
{
    if (m_poolData) {
        munmap(m_poolData, m_size);
    }
    if (m_pool) {
        wl_shm_pool_destroy(m_pool);
    }
    if (m_shm) {
        wl_shm_destroy(m_shm);
    }
}

bool ShmPool::createPool()
{
    if (!m_tmpFile->open()) {
        qDebug() << "Could not open temporary file for Shm pool";
        return false;
    }
    if (ftruncate(m_tmpFile->handle(), m_size) < 0) {
        qDebug() << "Could not set size for Shm pool file";
        return false;
    }
    m_poolData = mmap(NULL, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_tmpFile->handle(), 0);
    m_pool = wl_shm_create_pool(m_shm, m_tmpFile->handle(), m_size);

    if (!m_poolData || !m_pool) {
        qDebug() << "Creating Shm pool failed";
        return false;
    }
    m_tmpFile->close();
    return true;
}

wl_buffer *ShmPool::createBuffer(const QImage& image)
{
    if (image.isNull() || !m_valid) {
        return NULL;
    }
    // TODO: test whether buffer needs resizing
    wl_buffer *buffer = wl_shm_pool_create_buffer(m_pool, m_offset, image.width(), image.height(),
                                                  image.bytesPerLine(), WL_SHM_FORMAT_ARGB8888);
    if (buffer) {
        memcpy((char *)m_poolData + m_offset, image.bits(), image.byteCount());
        m_offset += image.byteCount();
    }
    return buffer;
}

WaylandSeat::WaylandSeat(wl_seat *seat, WaylandBackend *backend)
    : m_seat(seat)
    , m_pointer(NULL)
    , m_keyboard(NULL)
    , m_cursorTracker()
    , m_backend(backend)
{
    if (m_seat) {
        wl_seat_add_listener(m_seat, &s_seatListener, this);
    }
}

WaylandSeat::~WaylandSeat()
{
    destroyPointer();
    destroyKeyboard();
    if (m_seat) {
        wl_seat_destroy(m_seat);
    }
}

void WaylandSeat::destroyPointer()
{
    if (m_pointer) {
        wl_pointer_destroy(m_pointer);
        m_pointer = NULL;
        m_cursorTracker.reset();
    }
}

void WaylandSeat::destroyKeyboard()
{
    if (m_keyboard) {
        wl_keyboard_destroy(m_keyboard);
        m_keyboard = NULL;
    }
}

void WaylandSeat::changed(uint32_t capabilities)
{
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !m_pointer) {
        m_pointer = wl_seat_get_pointer(m_seat);
        wl_pointer_add_listener(m_pointer, &s_pointerListener, this);
        m_cursorTracker.reset(new X11CursorTracker(m_pointer, m_backend));
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        destroyPointer();
    }
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
        m_keyboard = wl_seat_get_keyboard(m_seat);
        wl_keyboard_add_listener(m_keyboard, &s_keyboardListener, this);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
        destroyKeyboard();
    }
}

void WaylandSeat::pointerEntered(uint32_t serial)
{
    if (m_cursorTracker.isNull()) {
        return;
    }
    m_cursorTracker->setEnteredSerial(serial);
}

void WaylandSeat::resetCursor()
{
    if (!m_cursorTracker.isNull()) {
        m_cursorTracker->resetCursor();
    }
}

WaylandBackend *WaylandBackend::s_self = 0;
WaylandBackend *WaylandBackend::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    const QByteArray display = qgetenv("WAYLAND_DISPLAY");
    if (display.isEmpty()) {
        return NULL;
    }
    s_self = new WaylandBackend(parent);
    return s_self;
}

WaylandBackend::WaylandBackend(QObject *parent)
    : QObject(parent)
    , m_display(wl_display_connect(NULL))
    , m_registry(wl_display_get_registry(m_display))
    , m_compositor(NULL)
    , m_shell(NULL)
    , m_surface(NULL)
    , m_shellSurface(NULL)
    , m_shellSurfaceSize(displayWidth(), displayHeight())
    , m_seat()
    , m_shm()
{
    qDebug() << "Created Wayland display";
    // setup the registry
    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_dispatch(m_display);
    int fd = wl_display_get_fd(m_display);
    QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, SIGNAL(activated(int)), SLOT(readEvents()));
}

WaylandBackend::~WaylandBackend()
{
    if (m_shellSurface) {
        wl_shell_surface_destroy(m_shellSurface);
    }
    if (m_surface) {
        wl_surface_destroy(m_surface);
    }
    if (m_shell) {
        wl_shell_destroy(m_shell);
    }
    if (m_compositor) {
        wl_compositor_destroy(m_compositor);
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
    if (m_display) {
        wl_display_flush(m_display);
        wl_display_disconnect(m_display);
    }
    qDebug() << "Destroyed Wayland display";
    s_self = NULL;
}

void WaylandBackend::readEvents()
{
    // TODO: this still seems to block
    wl_display_flush(m_display);
    wl_display_dispatch(m_display);
}

void WaylandBackend::createSeat(uint32_t name)
{
    wl_seat *seat = reinterpret_cast<wl_seat*>(wl_registry_bind(m_registry, name, &wl_seat_interface, 1));
    m_seat.reset(new WaylandSeat(seat, this));
}

void WaylandBackend::createSurface()
{
    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        qCritical() << "Creating Wayland Surface failed";
        return;
    }
    // map the surface as fullscreen
    m_shellSurface = wl_shell_get_shell_surface(m_shell, m_surface);
    wl_shell_surface_add_listener(m_shellSurface, &s_shellSurfaceListener, this);
    wl_shell_surface_set_fullscreen(m_shellSurface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);
}

void WaylandBackend::createShm(uint32_t name)
{
    m_shm.reset(new ShmPool(reinterpret_cast<wl_shm *>(wl_registry_bind(m_registry, name, &wl_shm_interface, 1))));
    if (!m_shm->isValid()) {
        m_shm.reset();
    }
}

void WaylandBackend::ping(uint32_t serial)
{
    wl_shell_surface_pong(m_shellSurface, serial);
    if (!m_seat.isNull()) {
        m_seat->resetCursor();
    }
}

void WaylandBackend::setShell(wl_shell *s)
{
    m_shell = s;
    createSurface();
}

void WaylandBackend::setShellSurfaceSize(const QSize &size)
{
    if (m_shellSurfaceSize == size) {
        return;
    }
    m_shellSurfaceSize = size;
    emit shellSurfaceSizeChanged(m_shellSurfaceSize);
}

void WaylandBackend::dispatchEvents()
{
    wl_display_dispatch_pending(m_display);
    wl_display_flush(m_display);
}

}

} // KWin
