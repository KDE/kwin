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
#include "main.h"
#include "input.h"
// Qt
#include <QDebug>
#include <QImage>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QTemporaryFile>
// xcb
#include <xcb/xtest.h>
#include <xcb/xfixes.h>
// Wayland
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
// system
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
    input()->processPointerMotion(QPoint(wl_fixed_to_double(sx), wl_fixed_to_double(sy)), time);
}

static void pointerHandleButton(void *data, wl_pointer *pointer, uint32_t serial, uint32_t time,
                                uint32_t button, uint32_t state)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    Q_UNUSED(serial)
    input()->processPointerButton(button, static_cast<InputRedirection::PointerButtonState>(state), time);
}

static void pointerHandleAxis(void *data, wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    Q_UNUSED(data)
    Q_UNUSED(pointer)
    input()->processPointerAxis(static_cast<InputRedirection::PointerAxis>(axis), wl_fixed_to_double(value), time);
}

static void keyboardHandleKeymap(void *data, wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        return;
    }
    input()->processKeymapChange(fd, size);
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
    input()->processKeyboardKey(key, static_cast<InputRedirection::KeyboardKeyState>(state), time);
}

static void keyboardHandleModifiers(void *data, wl_keyboard *keyboard, uint32_t serial, uint32_t modsDepressed,
                                    uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    Q_UNUSED(data)
    Q_UNUSED(keyboard)
    Q_UNUSED(serial)
    input()->processKeyboardModifiers(modsDepressed, modsLatched, modsLocked, group);
}

static void bufferRelease(void *data, wl_buffer *wl_buffer)
{
    Buffer *buffer = reinterpret_cast<Buffer*>(data);
    if (buffer->buffer() != wl_buffer) {
        return;
    }
    buffer->setReleased(true);
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

static const struct wl_buffer_listener s_bufferListener = {
    bufferRelease
};

CursorData::CursorData()
    : m_valid(init())
{
}

CursorData::~CursorData()
{
}

bool CursorData::init()
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
    // the backend for the cursorImage is destroyed once the xcb cursor goes out of scope
    // because of that we create a copy
    m_cursor = cursorImage.copy();

    m_hotSpot = QPoint(cursor->xhot, cursor->yhot);
    return true;
}

X11CursorTracker::X11CursorTracker(WaylandSeat *seat, WaylandBackend *backend, QObject* parent)
    : QObject(parent)
    , m_seat(seat)
    , m_backend(backend)
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
    CursorData cursor;
    if (cursor.isValid()) {
        // TODO: discard unused cursors after some time?
        m_cursors.insert(serial, cursor);
    }
    installCursor(cursor);
}

void X11CursorTracker::installCursor(const CursorData& cursor)
{
    const QImage &cursorImage = cursor.cursor();
    wl_buffer *buffer = m_backend->shmPool()->createBuffer(cursorImage);
    if (!buffer) {
        return;
    }
    m_seat->installCursorImage(buffer, cursorImage.size(), cursor.hotSpot());
}

void X11CursorTracker::resetCursor()
{
    QHash<uint32_t, CursorData>::iterator it = m_cursors.find(m_lastX11Cursor);
    if (it != m_cursors.end()) {
        installCursor(it.value());
    }
}

Buffer::Buffer(wl_buffer* buffer, const QSize& size, int32_t stride, size_t offset)
    : m_nativeBuffer(buffer)
    , m_released(false)
    , m_size(size)
    , m_stride(stride)
    , m_offset(offset)
    , m_used(false)
{
    wl_buffer_add_listener(m_nativeBuffer, &s_bufferListener, this);
}

Buffer::~Buffer()
{
    wl_buffer_destroy(m_nativeBuffer);
}

void Buffer::copy(const void* src)
{
    memcpy(address(), src, m_size.height()*m_stride);
}

uchar *Buffer::address()
{
    return (uchar*)WaylandBackend::self()->shmPool()->poolAddress() + m_offset;
}

ShmPool::ShmPool(wl_shm *shm)
    : m_shm(shm)
    , m_pool(NULL)
    , m_poolData(NULL)
    , m_size(1024)
    , m_tmpFile(new QTemporaryFile())
    , m_valid(createPool())
    , m_offset(0)
{
}

ShmPool::~ShmPool()
{
    qDeleteAll(m_buffers);
    if (m_poolData) {
        munmap(m_poolData, m_size);
    }
    if (m_pool) {
        wl_shm_pool_destroy(m_pool);
    }
    if (m_shm) {
        wl_shm_destroy(m_shm);
    }
    m_tmpFile->close();
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
    return true;
}

bool ShmPool::resizePool(int32_t newSize)
{
    if (ftruncate(m_tmpFile->handle(), newSize) < 0) {
        qDebug() << "Could not set new size for Shm pool file";
        return false;
    }
    wl_shm_pool_resize(m_pool, newSize);
    munmap(m_poolData, m_size);
    m_poolData = mmap(NULL, newSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_tmpFile->handle(), 0);
    m_size = newSize;
    if (!m_poolData) {
        qDebug() << "Resizing Shm pool failed";
        return false;
    }
    emit poolResized();
    return true;
}

wl_buffer *ShmPool::createBuffer(const QImage& image)
{
    if (image.isNull() || !m_valid) {
        return NULL;
    }
    Buffer *buffer = getBuffer(image.size(), image.bytesPerLine());
    if (!buffer) {
        return NULL;
    }
    buffer->copy(image.bits());
    return buffer->buffer();
}

wl_buffer *ShmPool::createBuffer(const QSize &size, int32_t stride, const void *src)
{
    if (size.isNull() || !m_valid) {
        return NULL;
    }
    Buffer *buffer = getBuffer(size, stride);
    if (!buffer) {
        return NULL;
    }
    buffer->copy(src);
    return buffer->buffer();
}

Buffer *ShmPool::getBuffer(const QSize &size, int32_t stride)
{
    Q_FOREACH (Buffer *buffer, m_buffers) {
        if (!buffer->isReleased() || buffer->isUsed()) {
            continue;
        }
        if (buffer->size() != size || buffer->stride() != stride) {
            continue;
        }
        buffer->setReleased(false);
        return buffer;
    }
    const int32_t byteCount = size.height() * stride;
    if (m_offset + byteCount > m_size) {
        if (!resizePool(m_size + byteCount)) {
            return NULL;
        }
    }
    // we don't have a buffer which we could reuse - need to create a new one
    wl_buffer *native = wl_shm_pool_create_buffer(m_pool, m_offset, size.width(), size.height(),
                                                  stride, WL_SHM_FORMAT_ARGB8888);
    if (!native) {
        return NULL;
    }
    Buffer *buffer = new Buffer(native, size, stride, m_offset);
    m_offset += byteCount;
    m_buffers.append(buffer);
    return buffer;
}

WaylandSeat::WaylandSeat(wl_seat *seat, WaylandBackend *backend)
    : QObject(NULL)
    , m_seat(seat)
    , m_pointer(NULL)
    , m_keyboard(NULL)
    , m_cursor(NULL)
    , m_theme(NULL)
    , m_enteredSerial(0)
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
    if (m_cursor) {
        wl_surface_destroy(m_cursor);
    }
    destroyTheme();
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
        m_cursorTracker.reset(new X11CursorTracker(this, m_backend));
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
    m_enteredSerial = serial;
}

void WaylandSeat::resetCursor()
{
    if (!m_cursorTracker.isNull()) {
        m_cursorTracker->resetCursor();
    }
}

void WaylandSeat::installCursorImage(wl_buffer *image, const QSize &size, const QPoint &hotSpot)
{
    if (!m_pointer) {
        return;
    }
    if (!m_cursor) {
        m_cursor = wl_compositor_create_surface(m_backend->compositor());
    }
    wl_pointer_set_cursor(m_pointer, m_enteredSerial, m_cursor, hotSpot.x(), hotSpot.y());
    wl_surface_attach(m_cursor, image, 0, 0);
    wl_surface_damage(m_cursor, 0, 0, size.width(), size.height());
    wl_surface_commit(m_cursor);
}

void WaylandSeat::installCursorImage(Qt::CursorShape shape)
{
    if (!m_theme) {
        loadTheme();
    }
    wl_cursor *c = wl_cursor_theme_get_cursor(m_theme, Cursor::self()->cursorName(shape).constData());
    if (c->image_count <= 0) {
        return;
    }
    wl_cursor_image *image = c->images[0];
    installCursorImage(wl_cursor_image_get_buffer(image),
                       QSize(image->width, image->height),
                       QPoint(image->hotspot_x, image->hotspot_y));
}

void WaylandSeat::loadTheme()
{
    Cursor *c = Cursor::self();
    if (!m_theme) {
        // so far the theme had not been created, this means we need to start tracking theme changes
        connect(c, SIGNAL(themeChanged()), SLOT(loadTheme()));
    } else {
        destroyTheme();
    }
    m_theme = wl_cursor_theme_load(c->themeName().toUtf8().constData(),
                                   c->themeSize(), m_backend->shmPool()->shm());
}

void WaylandSeat::destroyTheme()
{
    if (m_theme) {
        wl_cursor_theme_destroy(m_theme);
        m_theme = NULL;
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
    , m_display(nullptr)
    , m_registry(nullptr)
    , m_compositor(NULL)
    , m_shell(NULL)
    , m_surface(NULL)
    , m_shellSurface(NULL)
    , m_shellSurfaceSize(displayWidth(), displayHeight())
    , m_seat()
    , m_shm()
    , m_systemCompositorDied(false)
    , m_runtimeDir(qgetenv("XDG_RUNTIME_DIR"))
    , m_socketWatcher(nullptr)
{
    m_socketName = qgetenv("WAYLAND_DISPLAY");
    if (m_socketName.isEmpty()) {
        m_socketName = QStringLiteral("wayland-0");
    }

    initConnection();
    kwinApp()->setOperationMode(Application::OperationModeWaylandAndX11);
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

void WaylandBackend::initConnection()
{
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        // TODO: maybe we should now really tear down
        qWarning() << "Failed connecting to Wayland display";
        return;
    }
    m_registry = wl_display_get_registry(m_display);
    // setup the registry
    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_dispatch(m_display);
    int fd = wl_display_get_fd(m_display);
    QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &WaylandBackend::readEvents);

    if (m_runtimeDir.exists()) {
        m_socketWatcher = new QFileSystemWatcher(this);
        m_socketWatcher->addPath(m_runtimeDir.absoluteFilePath(m_socketName));
        connect(m_socketWatcher, &QFileSystemWatcher::fileChanged, this, &WaylandBackend::socketFileChanged);
    }
    qDebug() << "Created Wayland display";
}

void WaylandBackend::readEvents()
{
    // TODO: this still seems to block
    if (m_systemCompositorDied) {
        return;
    }
    wl_display_flush(m_display);
    wl_display_dispatch(m_display);
}

void WaylandBackend::socketFileChanged(const QString &socket)
{
    if (!QFile::exists(socket) && !m_systemCompositorDied) {
        qDebug() << "We lost the system compositor at:" << socket;
        m_systemCompositorDied = true;
        emit systemCompositorDied();
        m_seat.reset();
        m_shm.reset();
        if (m_shellSurface) {
            free(m_shellSurface);
            m_shellSurface = nullptr;
        }
        if (m_surface) {
            free(m_surface);
            m_surface = nullptr;
        }
        if (m_shell) {
            free(m_shell);
            m_shell = nullptr;
        }
        if (m_compositor) {
            free(m_compositor);
            m_compositor = nullptr;
        }
        if (m_registry) {
            free(m_registry);
            m_registry = nullptr;
        }
        if (m_display) {
            free(m_display);
            m_display = nullptr;
        }
        // need a new filesystem watcher
        delete m_socketWatcher;
        m_socketWatcher = new QFileSystemWatcher(this);
        m_socketWatcher->addPath(m_runtimeDir.absolutePath());
        connect(m_socketWatcher, &QFileSystemWatcher::directoryChanged, this, &WaylandBackend::socketDirectoryChanged);
    }
}

void WaylandBackend::socketDirectoryChanged()
{
    if (!m_systemCompositorDied) {
        return;
    }
    if (m_runtimeDir.exists(m_socketName)) {
        qDebug() << "Socket reappeared";
        delete m_socketWatcher;
        m_socketWatcher = nullptr;
        initConnection();
        m_systemCompositorDied = false;
    }
}

void WaylandBackend::createSeat(uint32_t name)
{
    wl_seat *seat = reinterpret_cast<wl_seat*>(wl_registry_bind(m_registry, name, &wl_seat_interface, 1));
    m_seat.reset(new WaylandSeat(seat, this));
}

void WaylandBackend::installCursorImage(Qt::CursorShape shape)
{
    if (m_seat.isNull()) {
        return;
    }
    m_seat->installCursorImage(shape);
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
    emit backendReady();
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
