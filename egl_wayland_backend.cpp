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
#define WL_EGL_PLATFORM 1
#include "egl_wayland_backend.h"
// kwin
#include "cursor.h"
#include "options.h"
// kwin libs
#include <kwinglplatform.h>
// KDE
#include <KDE/KDebug>
#include <KDE/KTemporaryFile>
// Qt
#include <QSocketNotifier>
// xcb
#include <xcb/xtest.h>
// Wayland
#include <wayland-client-protocol.h>
// system
#include <linux/input.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>

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
    kDebug(1212) << "Wayland Interface: " << interface;
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
    wl_egl_window_resize(display->overlay(), width, height, 0, 0);
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
        kDebug(1212) << "Creating cursor buffer failed";
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
    Cursor::self()->stopCursorTracking();
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
    , m_tmpFile(new KTemporaryFile())
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
        kDebug(1212) << "Could not open temporary file for Shm pool";
        return false;
    }
    if (ftruncate(m_tmpFile->handle(), m_size) < 0) {
        kDebug(1212) << "Could not set size for Shm pool file";
        return false;
    }
    m_poolData = mmap(NULL, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_tmpFile->handle(), 0);
    m_pool = wl_shm_create_pool(m_shm, m_tmpFile->handle(), m_size);

    if (!m_poolData || !m_pool) {
        kDebug(1212) << "Creating Shm pool failed";
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

WaylandBackend::WaylandBackend()
    : QObject(NULL)
    , m_display(wl_display_connect(NULL))
    , m_registry(wl_display_get_registry(m_display))
    , m_compositor(NULL)
    , m_shell(NULL)
    , m_surface(NULL)
    , m_overlay(NULL)
    , m_shellSurface(NULL)
    , m_seat()
    , m_shm()
{
    kDebug(1212) << "Created Wayland display";
    // setup the registry
    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_dispatch(m_display);
    int fd = wl_display_get_fd(m_display);
    QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, SIGNAL(activated(int)), SLOT(readEvents()));
}

WaylandBackend::~WaylandBackend()
{
    if (m_overlay) {
        wl_egl_window_destroy(m_overlay);
    }
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
    kDebug(1212) << "Destroyed Wayland display";
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

bool WaylandBackend::createSurface()
{
    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        kError(1212) << "Creating Wayland Surface failed";
        return false;
    }
    // map the surface as fullscreen
    m_shellSurface = wl_shell_get_shell_surface(m_shell, m_surface);
    wl_shell_surface_add_listener(m_shellSurface, &s_shellSurfaceListener, this);

    // TODO: do something better than displayWidth/displayHeight
    m_overlay = wl_egl_window_create(m_surface, displayWidth(), displayHeight());
    if (!m_overlay) {
        kError(1212) << "Creating Wayland Egl window failed";
        return false;
    }
    wl_shell_surface_set_fullscreen(m_shellSurface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);

    return true;
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

}

EglWaylandBackend::EglWaylandBackend()
    : OpenGLBackend()
    , m_context(EGL_NO_CONTEXT)
    , m_wayland(new Wayland::WaylandBackend)
{
    kDebug(1212) << "Connected to Wayland display?" << (m_wayland->display() ? "yes" : "no" );
    if (!m_wayland->display()) {
        setFailed("Could not connect to Wayland compositor");
        return;
    }
    initializeEgl();
    init();
    // Egl is always direct rendering
    setIsDirectRendering(true);

    kWarning(1212) << "Using Wayland rendering backend";
    kWarning(1212) << "This is a highly experimental backend, do not use for productive usage!";
    kWarning(1212) << "Please do not report any issues you might encounter when using this backend!";
}

EglWaylandBackend::~EglWaylandBackend()
{
    cleanupGL();
    checkGLError("Cleanup");
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(m_display, m_context);
    eglDestroySurface(m_display, m_surface);
    eglTerminate(m_display);
    eglReleaseThread();
}

bool EglWaylandBackend::initializeEgl()
{
    m_display = eglGetDisplay(m_wayland->display());
    if (m_display == EGL_NO_DISPLAY)
        return false;

    EGLint major, minor;
    if (eglInitialize(m_display, &major, &minor) == EGL_FALSE)
        return false;
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        kWarning(1212) << "Error during eglInitialize " << error;
        return false;
    }
    kDebug(1212) << "Egl Initialize succeeded";

#ifdef KWIN_HAVE_OPENGLES
    eglBindAPI(EGL_OPENGL_ES_API);
#else
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
        kError(1212) << "bind OpenGL API failed";
        return false;
    }
#endif
    kDebug(1212) << "EGL version: " << major << "." << minor;
    return true;
}

void EglWaylandBackend::init()
{
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initEGL();
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    glPlatform->printResults();
    initGL(EglPlatformInterface);
}

bool EglWaylandBackend::initRenderingContext()
{
    initBufferConfigs();

#ifdef KWIN_HAVE_OPENGLES
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs);
#else
    const EGLint context_attribs_31_core[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, 1,
        EGL_CONTEXT_FLAGS_KHR,         EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
        EGL_NONE
    };

    const EGLint context_attribs_legacy[] = {
        EGL_NONE
    };

    const QByteArray eglExtensions = eglQueryString(m_display, EGL_EXTENSIONS);
    const QList<QByteArray> extensions = eglExtensions.split(' ');

    // Try to create a 3.1 core context
    if (options->glCoreProfile() && extensions.contains("EGL_KHR_create_context"))
        m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs_31_core);

    if (m_context == EGL_NO_CONTEXT)
        m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs_legacy);
#endif

    if (m_context == EGL_NO_CONTEXT) {
        kError(1212) << "Create Context failed";
        return false;
    }

    if (!m_wayland->createSurface()) {
        return false;
    }

    m_surface = eglCreateWindowSurface(m_display, m_config, m_wayland->overlay(), NULL);
    if (m_surface == EGL_NO_SURFACE) {
        kError(1212) << "Create Window Surface failed";
        return false;
    }

    return makeContextCurrent();
}

bool EglWaylandBackend::makeContextCurrent()
{
    if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE) {
        kError(1212) << "Make Context Current failed";
        return false;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        kWarning(1212) << "Error occurred while creating context " << error;
        return false;
    }
    return true;
}

bool EglWaylandBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
#ifdef KWIN_HAVE_OPENGLES
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_ES2_BIT,
#else
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_BIT,
#endif
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(m_display, config_attribs, configs, 1, &count) == EGL_FALSE) {
        kError(1212) << "choose config failed";
        return false;
    }
    if (count != 1) {
        kError(1212) << "choose config did not return a config" << count;
        return false;
    }
    m_config = configs[0];

    return true;
}

void EglWaylandBackend::present()
{
    setLastDamage(QRegion());
    // need to dispatch pending events as eglSwapBuffers can block
    wl_display_dispatch_pending(m_wayland->display());
    wl_display_flush(m_wayland->display());
    eglSwapBuffers(m_display, m_surface);
}

void EglWaylandBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // no backend specific code needed
    // TODO: base implementation in OpenGLBackend
}

SceneOpenGL::TexturePrivate *EglWaylandBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglWaylandTexture(texture, this);
}

QRegion EglWaylandBackend::prepareRenderingFrame()
{
    if (!lastDamage().isEmpty())
        present();

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    startRenderTimer();

    return QRegion();
}

void EglWaylandBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    setLastDamage(renderedRegion);
    glFlush();
}

Shm *EglWaylandBackend::shm()
{
    if (m_shm.isNull()) {
        m_shm.reset(new Shm);
    }
    return m_shm.data();
}

/************************************************
 * EglTexture
 ************************************************/

EglWaylandTexture::EglWaylandTexture(KWin::SceneOpenGL::Texture *texture, KWin::EglWaylandBackend *backend)
    : SceneOpenGL::TexturePrivate()
    , q(texture)
    , m_backend(backend)
    , m_referencedPixmap(XCB_PIXMAP_NONE)
{
    m_target = GL_TEXTURE_2D;
}

EglWaylandTexture::~EglWaylandTexture()
{
}

OpenGLBackend *EglWaylandTexture::backend()
{
    return m_backend;
}

void EglWaylandTexture::findTarget()
{
    if (m_target != GL_TEXTURE_2D) {
        m_target = GL_TEXTURE_2D;
    }
}

bool EglWaylandTexture::loadTexture(const Pixmap &pix, const QSize &size, int depth)
{
    // HACK: egl wayland platform doesn't support texture from X11 pixmap through the KHR_image_pixmap
    // extension. To circumvent this problem we copy the pixmap content into a SHM image and from there
    // to the OpenGL texture. This is a temporary solution. In future we won't need to get the content
    // from X11 pixmaps. That's what we have XWayland for to get the content into a nice Wayland buffer.
    Q_UNUSED(depth)
    if (pix == XCB_PIXMAP_NONE)
        return false;
    m_referencedPixmap = pix;

    Shm *shm = m_backend->shm();
    if (!shm->isValid()) {
        return false;
    }

    xcb_shm_get_image_cookie_t cookie = xcb_shm_get_image_unchecked(connection(), pix, 0, 0, size.width(),
        size.height(), ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, shm->segment(), 0);

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();

    ScopedCPointer<xcb_shm_get_image_reply_t> image(xcb_shm_get_image_reply(connection(), cookie, NULL));
    if (image.isNull()) {
        return false;
    }

    // TODO: other formats
#ifndef KWIN_HAVE_OPENGLES
    glTexImage2D(m_target, 0, GL_RGBA8, size.width(), size.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, shm->buffer());
#endif

    q->unbind();
    checkGLError("load texture");
    q->setYInverted(true);
    m_size = size;
    updateMatrix();
    return true;
}

bool EglWaylandTexture::update(const QRegion &damage)
{
    if (m_referencedPixmap == XCB_PIXMAP_NONE) {
        return false;
    }

    Shm *shm = m_backend->shm();
    if (!shm->isValid()) {
        return false;
    }

    // TODO: optimize by only updating the damaged areas
    const QRect &damagedRect = damage.boundingRect();
    xcb_shm_get_image_cookie_t cookie = xcb_shm_get_image_unchecked(connection(), m_referencedPixmap,
        damagedRect.x(), damagedRect.y(), damagedRect.width(), damagedRect.height(),
        ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, shm->segment(), 0);

    q->bind();

    ScopedCPointer<xcb_shm_get_image_reply_t> image(xcb_shm_get_image_reply(connection(), cookie, NULL));
    if (image.isNull()) {
        return false;
    }

    // TODO: other formats
#ifndef KWIN_HAVE_OPENGLES
    glTexSubImage2D(m_target, 0, damagedRect.x(), damagedRect.y(), damagedRect.width(), damagedRect.height(), GL_BGRA, GL_UNSIGNED_BYTE, shm->buffer());
#endif

    q->unbind();
    checkGLError("update texture");
    return true;
}

Shm::Shm()
    : m_shmId(-1)
    , m_buffer(NULL)
    , m_segment(XCB_NONE)
    , m_valid(false)
{
    m_valid = init();
}

Shm::~Shm()
{
    if (m_valid) {
        xcb_shm_detach(connection(), m_segment);
        shmdt(m_buffer);
    }
}

bool Shm::init()
{
    const xcb_query_extension_reply_t *ext = xcb_get_extension_data(connection(), &xcb_shm_id);
    if (!ext || !ext->present) {
        kDebug(1212) << "SHM extension not available";
        return false;
    }
    ScopedCPointer<xcb_shm_query_version_reply_t> version(xcb_shm_query_version_reply(connection(),
        xcb_shm_query_version_unchecked(connection()), NULL));
    if (version.isNull()) {
        kDebug(1212) << "Failed to get SHM extension version information";
        return false;
    }
    const int MAXSIZE = 4096 * 2048 * 4; // TODO check there are not larger windows
    m_shmId = shmget(IPC_PRIVATE, MAXSIZE, IPC_CREAT | 0600);
    if (m_shmId < 0) {
        kDebug(1212) << "Failed to allocate SHM segment";
        return false;
    }
    m_buffer = shmat(m_shmId, NULL, 0 /*read/write*/);
    if (-1 == reinterpret_cast<long>(m_buffer)) {
        kDebug(1212) << "Failed to attach SHM segment";
        shmctl(m_shmId, IPC_RMID, NULL);
        return false;
    }
    shmctl(m_shmId, IPC_RMID, NULL);

    m_segment = xcb_generate_id(connection());
    const xcb_void_cookie_t cookie = xcb_shm_attach_checked(connection(), m_segment, m_shmId, false);
    ScopedCPointer<xcb_generic_error_t> error(xcb_request_check(connection(), cookie));
    if (!error.isNull()) {
        kDebug(1212) << "xcb_shm_attach error: " << error->error_code;
        shmdt(m_buffer);
        return false;
    }

    return true;
}

} // namespace
