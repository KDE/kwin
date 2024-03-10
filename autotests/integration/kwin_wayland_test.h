/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_WAYLAND_TEST_H
#define KWIN_WAYLAND_TEST_H

#include "core/inputdevice.h"
#include "main.h"
#include "window.h"

// Qt
#include <QSignalSpy>
#include <QTest>

#include <KWayland/Client/surface.h>
#include <optional>

#include "qwayland-cursor-shape-v1.h"
#include "qwayland-fake-input.h"
#include "qwayland-fractional-scale-v1.h"
#include "qwayland-idle-inhibit-unstable-v1.h"
#include "qwayland-input-method-unstable-v1.h"
#include "qwayland-kde-output-device-v2.h"
#include "qwayland-kde-output-management-v2.h"
#include "qwayland-kde-screen-edge-v1.h"
#include "qwayland-security-context-v1.h"
#include "qwayland-text-input-unstable-v3.h"
#include "qwayland-wlr-layer-shell-unstable-v1.h"
#include "qwayland-xdg-decoration-unstable-v1.h"
#include "qwayland-xdg-shell.h"
#include "qwayland-zkde-screencast-unstable-v1.h"

namespace KWayland
{
namespace Client
{
class AppMenuManager;
class ConnectionThread;
class Compositor;
class Output;
class PlasmaShell;
class PlasmaWindowManagement;
class Pointer;
class PointerConstraints;
class Seat;
class ShadowManager;
class ShmPool;
class SubCompositor;
class SubSurface;
class Surface;
class TextInputManager;
}
}

namespace QtWayland
{
class zwp_input_panel_surface_v1;
class zwp_text_input_v3;
class zwp_text_input_manager_v3;
}

class ScreencastingV1;

namespace KWin
{
#if KWIN_BUILD_X11
namespace Xwl
{
class Xwayland;
}
#endif

namespace Test
{
class VirtualInputDevice;
}

class WaylandTestApplication : public Application
{
    Q_OBJECT
public:
    WaylandTestApplication(OperationMode mode, int &argc, char **argv);
    ~WaylandTestApplication() override;

    void setInputMethodServerToStart(const QString &inputMethodServer)
    {
        m_inputMethodServerToStart = inputMethodServer;
    }

    Test::VirtualInputDevice *virtualPointer() const;
    Test::VirtualInputDevice *virtualKeyboard() const;
    Test::VirtualInputDevice *virtualTouch() const;
#if KWIN_BUILD_X11
    XwaylandInterface *xwayland() const override;
#endif

protected:
    void performStartup() override;

private:
    void continueStartupWithScene();
    void finalizeStartup();

    void createVirtualInputDevices();
    void destroyVirtualInputDevices();
#if KWIN_BUILD_X11
    std::unique_ptr<Xwl::Xwayland> m_xwayland;
#endif
    QString m_inputMethodServerToStart;

    std::unique_ptr<Test::VirtualInputDevice> m_virtualPointer;
    std::unique_ptr<Test::VirtualInputDevice> m_virtualKeyboard;
    std::unique_ptr<Test::VirtualInputDevice> m_virtualTouch;
};

namespace Test
{

class ScreencastingV1;
class MockInputMethod;

class TextInputManagerV3 : public QtWayland::zwp_text_input_manager_v3
{
public:
    ~TextInputManagerV3() override
    {
        destroy();
    }
};

class TextInputV3 : public QObject, public QtWayland::zwp_text_input_v3
{
    Q_OBJECT
public:
    ~TextInputV3() override
    {
        destroy();
    }

Q_SIGNALS:
    void preeditString(const QString &text, int cursor_begin, int cursor_end);

protected:
    void zwp_text_input_v3_preedit_string(const QString &text, int32_t cursor_begin, int32_t cursor_end) override
    {
        Q_EMIT preeditString(text, cursor_begin, cursor_end);
    }
};

class LayerShellV1 : public QtWayland::zwlr_layer_shell_v1
{
public:
    ~LayerShellV1() override;
};

class LayerSurfaceV1 : public QObject, public QtWayland::zwlr_layer_surface_v1
{
    Q_OBJECT

public:
    ~LayerSurfaceV1() override;

protected:
    void zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override;
    void zwlr_layer_surface_v1_closed() override;

Q_SIGNALS:
    void closeRequested();
    void configureRequested(quint32 serial, const QSize &size);
};

/**
 * The XdgShell class represents the @c xdg_wm_base global.
 */
class XdgShell : public QtWayland::xdg_wm_base
{
public:
    ~XdgShell() override;
    void xdg_wm_base_ping(uint32_t serial) override
    {
        pong(serial);
    }
};

/**
 * The XdgSurface class represents an xdg_surface object.
 */
class XdgSurface : public QObject, public QtWayland::xdg_surface
{
    Q_OBJECT

public:
    explicit XdgSurface(XdgShell *shell, KWayland::Client::Surface *surface, QObject *parent = nullptr);
    ~XdgSurface() override;

    KWayland::Client::Surface *surface() const;

Q_SIGNALS:
    void configureRequested(quint32 serial);

protected:
    void xdg_surface_configure(uint32_t serial) override;

private:
    KWayland::Client::Surface *m_surface;
};

/**
 * The XdgToplevel class represents an xdg_toplevel surface. Note that the XdgToplevel surface
 * takes the ownership of the underlying XdgSurface object.
 */
class XdgToplevel : public QObject, public QtWayland::xdg_toplevel
{
    Q_OBJECT

public:
    enum class State {
        Maximized = 1 << 0,
        Fullscreen = 1 << 1,
        Resizing = 1 << 2,
        Activated = 1 << 3
    };
    Q_DECLARE_FLAGS(States, State)

    explicit XdgToplevel(XdgSurface *surface, QObject *parent = nullptr);
    ~XdgToplevel() override;

    XdgSurface *xdgSurface() const;

Q_SIGNALS:
    void configureRequested(const QSize &size, KWin::Test::XdgToplevel::States states);
    void closeRequested();

protected:
    void xdg_toplevel_configure(int32_t width, int32_t height, wl_array *states) override;
    void xdg_toplevel_close() override;

private:
    std::unique_ptr<XdgSurface> m_xdgSurface;
};

/**
 * The XdgPositioner class represents an xdg_positioner object.
 */
class XdgPositioner : public QtWayland::xdg_positioner
{
public:
    explicit XdgPositioner(XdgShell *shell);
    ~XdgPositioner() override;
};

/**
 * The XdgPopup class represents an xdg_popup surface. Note that the XdgPopup surface takes
 * the ownership of the underlying XdgSurface object.
 */
class XdgPopup : public QObject, public QtWayland::xdg_popup
{
    Q_OBJECT

public:
    XdgPopup(XdgSurface *surface, XdgSurface *parentSurface, XdgPositioner *positioner, QObject *parent = nullptr);
    ~XdgPopup() override;

    XdgSurface *xdgSurface() const;

Q_SIGNALS:
    void configureRequested(const QRect &rect);
    void doneReceived();

protected:
    void xdg_popup_configure(int32_t x, int32_t y, int32_t width, int32_t height) override;
    void xdg_popup_popup_done() override;

private:
    std::unique_ptr<XdgSurface> m_xdgSurface;
};

class XdgDecorationManagerV1 : public QtWayland::zxdg_decoration_manager_v1
{
public:
    ~XdgDecorationManagerV1() override;
};

class XdgToplevelDecorationV1 : public QObject, public QtWayland::zxdg_toplevel_decoration_v1
{
    Q_OBJECT

public:
    XdgToplevelDecorationV1(XdgDecorationManagerV1 *manager, XdgToplevel *toplevel, QObject *parent = nullptr);
    ~XdgToplevelDecorationV1() override;

Q_SIGNALS:
    void configureRequested(QtWayland::zxdg_toplevel_decoration_v1::mode mode);

protected:
    void zxdg_toplevel_decoration_v1_configure(uint32_t mode) override;
};

class IdleInhibitManagerV1 : public QtWayland::zwp_idle_inhibit_manager_v1
{
public:
    ~IdleInhibitManagerV1() override;
};

class IdleInhibitorV1 : public QtWayland::zwp_idle_inhibitor_v1
{
public:
    IdleInhibitorV1(IdleInhibitManagerV1 *manager, KWayland::Client::Surface *surface);
    ~IdleInhibitorV1() override;
};

class WaylandOutputConfigurationV2 : public QObject, public QtWayland::kde_output_configuration_v2
{
    Q_OBJECT
public:
    WaylandOutputConfigurationV2(struct ::kde_output_configuration_v2 *object);

Q_SIGNALS:
    void applied();
    void failed();

protected:
    void kde_output_configuration_v2_applied() override;
    void kde_output_configuration_v2_failed() override;
};

class WaylandOutputManagementV2 : public QObject, public QtWayland::kde_output_management_v2
{
    Q_OBJECT
public:
    WaylandOutputManagementV2(struct ::wl_registry *registry, int id, int version);

    WaylandOutputConfigurationV2 *createConfiguration();
};

class WaylandOutputDeviceV2Mode : public QObject, public QtWayland::kde_output_device_mode_v2
{
    Q_OBJECT

public:
    WaylandOutputDeviceV2Mode(struct ::kde_output_device_mode_v2 *object);
    ~WaylandOutputDeviceV2Mode() override;

    int refreshRate() const;
    QSize size() const;
    bool preferred() const;

    bool operator==(const WaylandOutputDeviceV2Mode &other) const;

    static WaylandOutputDeviceV2Mode *get(struct ::kde_output_device_mode_v2 *object);

Q_SIGNALS:
    void removed();

protected:
    void kde_output_device_mode_v2_size(int32_t width, int32_t height) override;
    void kde_output_device_mode_v2_refresh(int32_t refresh) override;
    void kde_output_device_mode_v2_preferred() override;
    void kde_output_device_mode_v2_removed() override;

private:
    int m_refreshRate = 60000;
    QSize m_size;
    bool m_preferred = false;
};

class WaylandOutputDeviceV2 : public QObject, public QtWayland::kde_output_device_v2
{
    Q_OBJECT

public:
    WaylandOutputDeviceV2(int id);
    ~WaylandOutputDeviceV2() override;

    QByteArray edid() const;
    bool enabled() const;
    int id() const;
    QString name() const;
    QString model() const;
    QString manufacturer() const;
    qreal scale() const;
    QPoint globalPosition() const;
    QSize pixelSize() const;
    int refreshRate() const;
    uint32_t vrrPolicy() const;
    uint32_t overscan() const;
    uint32_t capabilities() const;
    uint32_t rgbRange() const;

    QString modeId() const;

Q_SIGNALS:
    void enabledChanged();
    void done();

protected:
    void kde_output_device_v2_geometry(int32_t x,
                                       int32_t y,
                                       int32_t physical_width,
                                       int32_t physical_height,
                                       int32_t subpixel,
                                       const QString &make,
                                       const QString &model,
                                       int32_t transform) override;
    void kde_output_device_v2_current_mode(struct ::kde_output_device_mode_v2 *mode) override;
    void kde_output_device_v2_mode(struct ::kde_output_device_mode_v2 *mode) override;
    void kde_output_device_v2_done() override;
    void kde_output_device_v2_scale(wl_fixed_t factor) override;
    void kde_output_device_v2_edid(const QString &raw) override;
    void kde_output_device_v2_enabled(int32_t enabled) override;
    void kde_output_device_v2_uuid(const QString &uuid) override;
    void kde_output_device_v2_serial_number(const QString &serialNumber) override;
    void kde_output_device_v2_eisa_id(const QString &eisaId) override;
    void kde_output_device_v2_capabilities(uint32_t flags) override;
    void kde_output_device_v2_overscan(uint32_t overscan) override;
    void kde_output_device_v2_vrr_policy(uint32_t vrr_policy) override;
    void kde_output_device_v2_rgb_range(uint32_t rgb_range) override;

private:
    QString modeName(const WaylandOutputDeviceV2Mode *m) const;
    WaylandOutputDeviceV2Mode *deviceModeFromId(const int modeId) const;

    WaylandOutputDeviceV2Mode *m_mode;
    QList<WaylandOutputDeviceV2Mode *> m_modes;

    int m_id;
    QPoint m_pos;
    QSize m_physicalSize;
    int32_t m_subpixel;
    QString m_manufacturer;
    QString m_model;
    int32_t m_transform;
    qreal m_factor;
    QByteArray m_edid;
    int32_t m_enabled;
    QString m_uuid;
    QString m_serialNumber;
    QString m_eisaId;
    uint32_t m_flags;
    uint32_t m_overscan;
    uint32_t m_vrr_policy;
    uint32_t m_rgbRange;
};

class MockInputMethod : public QObject, QtWayland::zwp_input_method_v1
{
    Q_OBJECT
public:
    enum class Mode {
        TopLevel,
        Overlay,
    };

    MockInputMethod(struct wl_registry *registry, int id, int version);
    ~MockInputMethod();

    KWayland::Client::Surface *inputPanelSurface() const
    {
        return m_inputSurface.get();
    }
    auto *context() const
    {
        return m_context;
    }

    void setMode(Mode mode);

Q_SIGNALS:
    void activate();

protected:
    void zwp_input_method_v1_activate(struct ::zwp_input_method_context_v1 *context) override;
    void zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context) override;

private:
    std::unique_ptr<KWayland::Client::Surface> m_inputSurface;
    std::unique_ptr<QtWayland::zwp_input_panel_surface_v1> m_inputMethodSurface;
    struct ::zwp_input_method_context_v1 *m_context = nullptr;
    Mode m_mode = Mode::TopLevel;
};

class FractionalScaleManagerV1 : public QObject, public QtWayland::wp_fractional_scale_manager_v1
{
    Q_OBJECT
public:
    ~FractionalScaleManagerV1() override;
};

class FractionalScaleV1 : public QObject, public QtWayland::wp_fractional_scale_v1
{
    Q_OBJECT
public:
    ~FractionalScaleV1() override;
    int preferredScale();

protected:
    void wp_fractional_scale_v1_preferred_scale(uint32_t scale) override;

private:
    int m_preferredScale = 120;
};

class ScreenEdgeManagerV1 : public QObject, public QtWayland::kde_screen_edge_manager_v1
{
    Q_OBJECT
public:
    ~ScreenEdgeManagerV1() override;
};

class AutoHideScreenEdgeV1 : public QObject, public QtWayland::kde_auto_hide_screen_edge_v1
{
    Q_OBJECT
public:
    AutoHideScreenEdgeV1(ScreenEdgeManagerV1 *manager, KWayland::Client::Surface *surface, uint32_t border);
    ~AutoHideScreenEdgeV1() override;
};

class CursorShapeManagerV1 : public QObject, public QtWayland::wp_cursor_shape_manager_v1
{
    Q_OBJECT
public:
    ~CursorShapeManagerV1() override;
};

class CursorShapeDeviceV1 : public QObject, public QtWayland::wp_cursor_shape_device_v1
{
    Q_OBJECT
public:
    CursorShapeDeviceV1(CursorShapeManagerV1 *manager, KWayland::Client::Pointer *pointer);
    ~CursorShapeDeviceV1() override;
};

class FakeInput : public QtWayland::org_kde_kwin_fake_input
{
public:
    ~FakeInput() override;
};

class SecurityContextManagerV1 : public QtWayland::wp_security_context_manager_v1
{
public:
    ~SecurityContextManagerV1() override;
};

enum class AdditionalWaylandInterface {
    Seat = 1 << 0,
    PlasmaShell = 1 << 2,
    WindowManagement = 1 << 3,
    PointerConstraints = 1 << 4,
    IdleInhibitV1 = 1 << 5,
    AppMenu = 1 << 6,
    ShadowManager = 1 << 7,
    XdgDecorationV1 = 1 << 8,
    OutputManagementV2 = 1 << 9,
    TextInputManagerV2 = 1 << 10,
    InputMethodV1 = 1 << 11,
    LayerShellV1 = 1 << 12,
    TextInputManagerV3 = 1 << 13,
    OutputDeviceV2 = 1 << 14,
    FractionalScaleManagerV1 = 1 << 15,
    ScreencastingV1 = 1 << 16,
    ScreenEdgeV1 = 1 << 17,
    CursorShapeV1 = 1 << 18,
    FakeInput = 1 << 19,
    SecurityContextManagerV1 = 1 << 20,
};
Q_DECLARE_FLAGS(AdditionalWaylandInterfaces, AdditionalWaylandInterface)

class VirtualInputDevice : public InputDevice
{
    Q_OBJECT

public:
    explicit VirtualInputDevice(QObject *parent = nullptr);

    void setPointer(bool set);
    void setKeyboard(bool set);
    void setTouch(bool set);
    void setLidSwitch(bool set);
    void setName(const QString &name);

    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    LEDs leds() const override;
    void setLeds(LEDs leds) override;

    bool isKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;

private:
    QString m_name;
    bool m_pointer = false;
    bool m_keyboard = false;
    bool m_touch = false;
    bool m_lidSwitch = false;
};

void keyboardKeyPressed(quint32 key, quint32 time);
void keyboardKeyReleased(quint32 key, quint32 time);
void pointerAxisHorizontal(qreal delta,
                           quint32 time,
                           qint32 discreteDelta = 0,
                           InputRedirection::PointerAxisSource source = InputRedirection::PointerAxisSourceUnknown);
void pointerAxisVertical(qreal delta,
                         quint32 time,
                         qint32 discreteDelta = 0,
                         InputRedirection::PointerAxisSource source = InputRedirection::PointerAxisSourceUnknown);
void pointerButtonPressed(quint32 button, quint32 time);
void pointerButtonReleased(quint32 button, quint32 time);
void pointerMotion(const QPointF &position, quint32 time);
void pointerMotionRelative(const QPointF &delta, quint32 time);
void touchCancel();
void touchDown(qint32 id, const QPointF &pos, quint32 time);
void touchMotion(qint32 id, const QPointF &pos, quint32 time);
void touchUp(qint32 id, quint32 time);

/**
 * Creates a Wayland Connection in a dedicated thread and creates various
 * client side objects which can be used to create windows.
 * @returns @c true if created successfully, @c false if there was an error
 * @see destroyWaylandConnection
 */
bool setupWaylandConnection(AdditionalWaylandInterfaces flags = AdditionalWaylandInterfaces());

/**
 * Destroys the Wayland Connection created with @link{setupWaylandConnection}.
 * This can be called from cleanup in order to ensure that no Wayland Connection
 * leaks into the next test method.
 * @see setupWaylandConnection
 */
void destroyWaylandConnection();

KWayland::Client::ConnectionThread *waylandConnection();
KWayland::Client::Compositor *waylandCompositor();
KWayland::Client::SubCompositor *waylandSubCompositor();
KWayland::Client::ShadowManager *waylandShadowManager();
KWayland::Client::ShmPool *waylandShmPool();
KWayland::Client::Seat *waylandSeat();
KWayland::Client::PlasmaShell *waylandPlasmaShell();
KWayland::Client::PlasmaWindowManagement *waylandWindowManagement();
KWayland::Client::PointerConstraints *waylandPointerConstraints();
KWayland::Client::AppMenuManager *waylandAppMenuManager();
WaylandOutputManagementV2 *waylandOutputManagementV2();
KWayland::Client::TextInputManager *waylandTextInputManager();
QList<KWayland::Client::Output *> waylandOutputs();
KWayland::Client::Output *waylandOutput(const QString &name);
ScreencastingV1 *screencasting();
QList<WaylandOutputDeviceV2 *> waylandOutputDevicesV2();
FakeInput *waylandFakeInput();
SecurityContextManagerV1 *waylandSecurityContextManagerV1();

bool waitForWaylandSurface(Window *window);

bool waitForWaylandPointer();
bool waitForWaylandTouch();
bool waitForWaylandKeyboard();

void flushWaylandConnection();

/**
 * Ensures that all client requests are processed by the compositor and all events
 * sent by the compositor are seen by the client.
 */
bool waylandSync();

std::unique_ptr<KWayland::Client::Surface> createSurface();
std::unique_ptr<KWayland::Client::SubSurface> createSubSurface(KWayland::Client::Surface *surface,
                                                               KWayland::Client::Surface *parentSurface);

std::unique_ptr<LayerSurfaceV1> createLayerSurfaceV1(KWayland::Client::Surface *surface,
                                                     const QString &scope,
                                                     KWayland::Client::Output *output = nullptr,
                                                     LayerShellV1::layer layer = LayerShellV1::layer_top);

TextInputManagerV3 *waylandTextInputManagerV3();

enum class CreationSetup {
    CreateOnly,
    CreateAndConfigure, /// commit and wait for the configure event, making this surface ready to commit buffers
};

std::unique_ptr<QtWayland::zwp_input_panel_surface_v1> createInputPanelSurfaceV1(KWayland::Client::Surface *surface,
                                                                                 KWayland::Client::Output *output,
                                                                                 MockInputMethod::Mode mode);

std::unique_ptr<FractionalScaleV1> createFractionalScaleV1(KWayland::Client::Surface *surface);

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface);
std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface, CreationSetup configureMode);

std::unique_ptr<XdgPositioner> createXdgPositioner();

std::unique_ptr<XdgPopup> createXdgPopupSurface(KWayland::Client::Surface *surface, XdgSurface *parentSurface,
                                                XdgPositioner *positioner,
                                                CreationSetup configureMode = CreationSetup::CreateAndConfigure);

XdgToplevelDecorationV1 *createXdgToplevelDecorationV1(XdgToplevel *toplevel, QObject *parent = nullptr);
IdleInhibitorV1 *createIdleInhibitorV1(KWayland::Client::Surface *surface);
AutoHideScreenEdgeV1 *createAutoHideScreenEdgeV1(KWayland::Client::Surface *surface, uint32_t border);
CursorShapeDeviceV1 *createCursorShapeDeviceV1(KWayland::Client::Pointer *pointer);

/**
 * Creates a shared memory buffer of @p size in @p color and attaches it to the @p surface.
 * The @p surface gets damaged and committed, thus it's rendered.
 */
void render(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format = QImage::Format_ARGB32_Premultiplied);

/**
 * Creates a shared memory buffer using the supplied image @p img and attaches it to the @p surface
 */
void render(KWayland::Client::Surface *surface, const QImage &img);

/**
 * Waits till a new Window is shown and returns the created Window.
 * If no Window gets shown during @p timeout @c null is returned.
 */
Window *waitForWaylandWindowShown(int timeout = 5000);

/**
 * Combination of @link{render} and @link{waitForWaylandWindowShown}.
 */
Window *renderAndWaitForShown(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format = QImage::Format_ARGB32, int timeout = 5000);

Window *renderAndWaitForShown(KWayland::Client::Surface *surface, const QImage &img, int timeout = 5000);

/**
 * Waits for the @p window to be destroyed.
 */
bool waitForWindowClosed(Window *window);

/**
 * Locks the screen and waits till the screen is locked.
 * @returns @c true if the screen could be locked, @c false otherwise
 */
bool lockScreen();

/**
 * Unlocks the screen and waits till the screen is unlocked.
 * @returns @c true if the screen could be unlocked, @c false otherwise
 */
bool unlockScreen();

/**
 * Returns @c true if the system has at least one render node; otherwise returns @c false.
 *
 * This can be used to test whether the system is capable of allocating and sharing prime buffers, etc.
 */
bool renderNodeAvailable();

/**
 * Creates an X11 connection
 * Internally a nested event loop is spawned whilst we connect to avoid a deadlock
 * with X on demand
 */

#if KWIN_BUILD_X11
struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer);
};
typedef std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> XcbConnectionPtr;
XcbConnectionPtr createX11Connection();
#endif

MockInputMethod *inputMethod();
KWayland::Client::Surface *inputPanelSurface();

class ScreencastingStreamV1 : public QObject, public QtWayland::zkde_screencast_stream_unstable_v1
{
    Q_OBJECT
    friend class ScreencastingV1;

public:
    ScreencastingStreamV1(QObject *parent)
        : QObject(parent)
    {
    }

    ~ScreencastingStreamV1() override
    {
        if (isInitialized()) {
            close();
        }
    }

    quint32 nodeId() const
    {
        Q_ASSERT(m_nodeId.has_value());
        return *m_nodeId;
    }

    void zkde_screencast_stream_unstable_v1_created(uint32_t node) override
    {
        m_nodeId = node;
        Q_EMIT created(node);
    }

    void zkde_screencast_stream_unstable_v1_closed() override
    {
        Q_EMIT closed();
    }

    void zkde_screencast_stream_unstable_v1_failed(const QString &error) override
    {
        Q_EMIT failed(error);
    }

Q_SIGNALS:
    void created(quint32 nodeid);
    void failed(const QString &error);
    void closed();

private:
    std::optional<uint> m_nodeId;
};

class ScreencastingV1 : public QObject, public QtWayland::zkde_screencast_unstable_v1
{
    Q_OBJECT
public:
    explicit ScreencastingV1(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ScreencastingStreamV1 *createOutputStream(wl_output *output, pointer mode)
    {
        auto stream = new ScreencastingStreamV1(this);
        stream->init(stream_output(output, mode));
        return stream;
    }

    ScreencastingStreamV1 *createWindowStream(const QString &uuid, pointer mode)
    {
        auto stream = new ScreencastingStreamV1(this);
        stream->init(stream_window(uuid, mode));
        return stream;
    }
};

struct OutputInfo
{
    QRect geometry;
    double scale = 1;
    bool internal = false;
};
void setOutputConfig(const QList<QRect> &geometries);
void setOutputConfig(const QList<OutputInfo> &infos);
}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Test::AdditionalWaylandInterfaces)
Q_DECLARE_METATYPE(KWin::Test::XdgToplevel::States)
Q_DECLARE_METATYPE(QtWayland::zxdg_toplevel_decoration_v1::mode)

#define WAYLANDTEST_MAIN(TestObject)                                                                                                      \
    int main(int argc, char *argv[])                                                                                                      \
    {                                                                                                                                     \
        setenv("QT_QPA_PLATFORM", "wayland-org.kde.kwin.qpa", true);                                                                      \
        setenv("QT_QPA_PLATFORM_PLUGIN_PATH", QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath().toLocal8Bit().constData(), true); \
        setenv("KWIN_FORCE_OWN_QPA", "1", true);                                                                                          \
        qunsetenv("KDE_FULL_SESSION");                                                                                                    \
        qunsetenv("KDE_SESSION_VERSION");                                                                                                 \
        qunsetenv("XDG_SESSION_DESKTOP");                                                                                                 \
        qunsetenv("XDG_CURRENT_DESKTOP");                                                                                                 \
        KWin::WaylandTestApplication app(KWin::Application::OperationModeXwayland, argc, argv);                                           \
        app.setAttribute(Qt::AA_Use96Dpi, true);                                                                                          \
        TestObject tc;                                                                                                                    \
        return QTest::qExec(&tc, argc, argv);                                                                                             \
    }

#endif
