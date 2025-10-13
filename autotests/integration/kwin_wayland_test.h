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

// KWayland
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/surface.h>

// Qt
#include <QMimeType>
#include <QSignalSpy>
#include <QTest>

#include <optional>

#include <xkbcommon/xkbcommon.h>

#include "qwayland-color-management-v1.h"
#include "qwayland-cursor-shape-v1.h"
#include "qwayland-fake-input.h"
#include "qwayland-fifo-v1.h"
#include "qwayland-fractional-scale-v1.h"
#include "qwayland-idle-inhibit-unstable-v1.h"
#include "qwayland-input-method-unstable-v1.h"
#include "qwayland-kde-output-device-v2.h"
#include "qwayland-kde-output-management-v2.h"
#include "qwayland-kde-screen-edge-v1.h"
#include "qwayland-keystate.h"
#include "qwayland-presentation-time.h"
#include "qwayland-primary-selection-unstable-v1.h"
#include "qwayland-security-context-v1.h"
#include "qwayland-tablet-v2.h"
#include "qwayland-text-input-unstable-v3.h"
#include "qwayland-wlr-layer-shell-unstable-v1.h"
#include "qwayland-xdg-activation-v1.h"
#include "qwayland-xdg-decoration-unstable-v1.h"
#include "qwayland-xdg-dialog-v1.h"
#include "qwayland-xdg-shell.h"
#include "qwayland-xx-session-management-v1.h"
#include "qwayland-zkde-screencast-unstable-v1.h"

namespace KWayland
{
namespace Client
{
class AppMenuManager;
class ConnectionThread;
class Compositor;
class EventQueue;
class Output;
class PlasmaShell;
class PlasmaWindowManagement;
class Pointer;
class PointerConstraints;
class Registry;
class Seat;
class ShadowManager;
class ShmPool;
class SubCompositor;
class SubSurface;
class Surface;
class TextInputManager;
class DataDeviceManager;
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

class WaylandServer;

#if KWIN_BUILD_X11
namespace Xwl
{
class Xwayland;
}
#endif

namespace Test
{
class VirtualInputDevice;
class VirtualInputDeviceTabletTool;
}

class WaylandTestApplication : public Application
{
    Q_OBJECT
public:
    WaylandTestApplication(int &argc, char **argv, bool runOnKMS);
    ~WaylandTestApplication() override;

    void setInputMethodServerToStart(const QString &inputMethodServer)
    {
        m_inputMethodServerToStart = inputMethodServer;
    }

    Test::VirtualInputDevice *virtualPointer() const;
    Test::VirtualInputDevice *virtualKeyboard() const;
    Test::VirtualInputDevice *virtualTouch() const;
    Test::VirtualInputDevice *virtualTabletPad() const;
    Test::VirtualInputDevice *virtualTablet() const;
    Test::VirtualInputDeviceTabletTool *virtualTabletTool() const;

#if KWIN_BUILD_X11
    XwaylandInterface *xwayland() const override;
#endif

protected:
    void performStartup() override;

private:
    void finalizeStartup();

    void createVirtualInputDevices();
    void destroyVirtualInputDevices();

    std::unique_ptr<WaylandServer> m_waylandServer;
#if KWIN_BUILD_X11
    std::unique_ptr<Xwl::Xwayland> m_xwayland;
#endif
    QString m_inputMethodServerToStart;

    std::unique_ptr<Test::VirtualInputDevice> m_virtualPointer;
    std::unique_ptr<Test::VirtualInputDevice> m_virtualKeyboard;
    std::unique_ptr<Test::VirtualInputDevice> m_virtualTouch;
    std::unique_ptr<Test::VirtualInputDevice> m_virtualTabletPad;
    std::unique_ptr<Test::VirtualInputDevice> m_virtualTablet;
    std::unique_ptr<Test::VirtualInputDeviceTabletTool> m_virtualTabletTool;
};

namespace Test
{

class ScreencastingV1;
class MockInputMethod;
class WpTabletV2;
class WpTabletPadV2;
class WpTabletSeatV2;
class WpTabletToolV2;

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
    void commitString(const QString &text);

protected:
    void zwp_text_input_v3_preedit_string(const QString &text, int32_t cursor_begin, int32_t cursor_end) override
    {
        Q_EMIT preeditString(text, cursor_begin, cursor_end);
    }
    void zwp_text_input_v3_commit_string(const QString &text) override
    {
        Q_EMIT commitString(text);
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

Q_SIGNALS:
    void preferredScaleChanged();

private:
    uint m_preferredScale = 120;
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

class XdgWmDialogV1 : public QtWayland::xdg_wm_dialog_v1
{
public:
    ~XdgWmDialogV1() override;
};

class XdgDialogV1 : public QtWayland::xdg_dialog_v1
{
public:
    XdgDialogV1(XdgWmDialogV1 *wm, XdgToplevel *toplevel);
    ~XdgDialogV1() override;
};

class WpTabletManagerV2 : public QtWayland::zwp_tablet_manager_v2
{
public:
    WpTabletManagerV2(::wl_registry *registry, uint32_t id, int version);
    ~WpTabletManagerV2() override;

    std::unique_ptr<WpTabletSeatV2> createSeat(KWayland::Client::Seat *seat);
};

class WpTabletSeatV2 : public QObject, public QtWayland::zwp_tablet_seat_v2
{
    Q_OBJECT

public:
    explicit WpTabletSeatV2(::zwp_tablet_seat_v2 *seat);
    ~WpTabletSeatV2() override;

Q_SIGNALS:
    void toolAdded(WpTabletToolV2 *tool);

protected:
    void zwp_tablet_seat_v2_tablet_added(::zwp_tablet_v2 *id) override;
    void zwp_tablet_seat_v2_tool_added(::zwp_tablet_tool_v2 *id) override;
    void zwp_tablet_seat_v2_pad_added(::zwp_tablet_pad_v2 *id) override;

private:
    std::vector<std::unique_ptr<WpTabletV2>> m_tablets;
    std::vector<std::unique_ptr<WpTabletToolV2>> m_tools;
    std::vector<std::unique_ptr<WpTabletPadV2>> m_pads;
};

class WpTabletV2 : public QtWayland::zwp_tablet_v2
{
public:
    explicit WpTabletV2(::zwp_tablet_v2 *id);
    ~WpTabletV2() override;
};

class WpTabletPadV2 : public QtWayland::zwp_tablet_pad_v2
{
public:
    explicit WpTabletPadV2(::zwp_tablet_pad_v2 *id);
    ~WpTabletPadV2() override;
};

class WpTabletToolV2 : public QObject, public QtWayland::zwp_tablet_tool_v2
{
    Q_OBJECT

public:
    explicit WpTabletToolV2(::zwp_tablet_tool_v2 *id);
    ~WpTabletToolV2() override;

    bool ready() const;

Q_SIGNALS:
    void done();
    void down(uint32_t serial);
    void up();
    void motion(const QPointF &position);

protected:
    void zwp_tablet_tool_v2_done() override;
    void zwp_tablet_tool_v2_down(uint32_t serial) override;
    void zwp_tablet_tool_v2_up() override;
    void zwp_tablet_tool_v2_motion(wl_fixed_t x, wl_fixed_t y) override;

private:
    bool m_ready = false;
};

class WpPrimarySelectionOfferV1 : public QObject, public QtWayland::zwp_primary_selection_offer_v1
{
    Q_OBJECT

public:
    explicit WpPrimarySelectionOfferV1(::zwp_primary_selection_offer_v1 *id);
    ~WpPrimarySelectionOfferV1() override;

    QList<QMimeType> mimeTypes() const;

protected:
    void zwp_primary_selection_offer_v1_offer(const QString &mime_type) override;

private:
    QList<QMimeType> m_mimeTypes;
};

class WpPrimarySelectionSourceV1 : public QObject, public QtWayland::zwp_primary_selection_source_v1
{
    Q_OBJECT

public:
    explicit WpPrimarySelectionSourceV1(::zwp_primary_selection_source_v1 *id);
    ~WpPrimarySelectionSourceV1() override;

Q_SIGNALS:
    void sendDataRequested(const QString &mimeType, int32_t fd);
    void cancelled();

protected:
    void zwp_primary_selection_source_v1_send(const QString &mime_type, int32_t fd) override;
    void zwp_primary_selection_source_v1_cancelled() override;
};

class WpPrimarySelectionDeviceV1 : public QObject, public QtWayland::zwp_primary_selection_device_v1
{
    Q_OBJECT

public:
    explicit WpPrimarySelectionDeviceV1(::zwp_primary_selection_device_v1 *id);
    ~WpPrimarySelectionDeviceV1() override;

    WpPrimarySelectionOfferV1 *offer() const;

Q_SIGNALS:
    void selectionOffered(WpPrimarySelectionOfferV1 *offer);
    void selectionCleared();

protected:
    void zwp_primary_selection_device_v1_data_offer(::zwp_primary_selection_offer_v1 *offer) override;
    void zwp_primary_selection_device_v1_selection(::zwp_primary_selection_offer_v1 *id) override;

private:
    std::unique_ptr<WpPrimarySelectionOfferV1> m_offer;
};

class WpPrimarySelectionDeviceManagerV1 : public QtWayland::zwp_primary_selection_device_manager_v1
{
public:
    WpPrimarySelectionDeviceManagerV1(::wl_registry *registry, uint32_t id, int version);
    ~WpPrimarySelectionDeviceManagerV1() override;

    std::unique_ptr<WpPrimarySelectionDeviceV1> getDevice(KWayland::Client::Seat *seat);
    std::unique_ptr<WpPrimarySelectionSourceV1> createSource();
};

enum class AdditionalWaylandInterface {
    Seat = 1 << 0,
    DataDeviceManager = 1 << 1,
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
    XdgDialogV1 = 1 << 21,
    ColorManagement = 1 << 22,
    FifoV1 = 1 << 23,
    PresentationTime = 1 << 24,
    XdgActivation = 1 << 25,
    XdgSessionV1 = 1 << 26,
    WpTabletV2 = 1 << 27,
    KeyState = 1 << 28,
    WpPrimarySelectionV1 = 1 << 29,
};
Q_DECLARE_FLAGS(AdditionalWaylandInterfaces, AdditionalWaylandInterface)

class VirtualInputDeviceTabletTool : public InputDeviceTabletTool
{
    Q_OBJECT

public:
    explicit VirtualInputDeviceTabletTool(QObject *parent = nullptr);

    void setSerialId(quint64 serialId);
    void setUniqueId(quint64 uniqueId);
    void setType(Type type);
    void setCapabilities(const QList<Capability> &capabilities);

    quint64 serialId() const override;
    quint64 uniqueId() const override;
    Type type() const override;
    QList<Capability> capabilities() const override;

private:
    quint64 m_serialId = 0;
    quint64 m_uniqueId = 0;
    Type m_type = Type::Pen;
    QList<Capability> m_capabilities;
};

class VirtualInputDevice : public InputDevice
{
    Q_OBJECT

public:
    explicit VirtualInputDevice(QObject *parent = nullptr);

    void setPointer(bool set);
    void setKeyboard(bool set);
    void setTouch(bool set);
    void setLidSwitch(bool set);
    void setTabletPad(bool set);
    void setTabletTool(bool set);
    void setName(const QString &name);
    void setGroup(uintptr_t group);

    QString name() const override;
    void *group() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

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
    void *m_group = nullptr;
    bool m_pointer = false;
    bool m_keyboard = false;
    bool m_touch = false;
    bool m_lidSwitch = false;
    bool m_tabletPad = false;
    bool m_tabletTool = false;
};

class ColorManagerV1 : public QtWayland::wp_color_manager_v1
{
public:
    explicit ColorManagerV1(::wl_registry *registry, uint32_t id, int version);
    ~ColorManagerV1() override;
};

class FifoManagerV1 : public QtWayland::wp_fifo_manager_v1
{
public:
    explicit FifoManagerV1(::wl_registry *registry, uint32_t id, int version);
    ~FifoManagerV1() override;
};

class PresentationTime : public QtWayland::wp_presentation
{
public:
    explicit PresentationTime(::wl_registry *registry, uint32_t id, int version);
    ~PresentationTime() override;
};

class WpPresentationFeedback : public QObject, public QtWayland::wp_presentation_feedback
{
    Q_OBJECT
public:
    explicit WpPresentationFeedback(struct ::wp_presentation_feedback *obj);
    ~WpPresentationFeedback() override;

Q_SIGNALS:
    void presented(std::chrono::nanoseconds timestamp, std::chrono::nanoseconds refreshDuration);
    void discarded();

private:
    void wp_presentation_feedback_presented(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t refresh, uint32_t seq_hi, uint32_t seq_lo, uint32_t flags) override;
    void wp_presentation_feedback_discarded() override;
};

class XdgActivationToken : public QObject, public QtWayland::xdg_activation_token_v1
{
    Q_OBJECT
public:
    explicit XdgActivationToken(::xdg_activation_token_v1 *object);
    ~XdgActivationToken() override;

    QString commitAndWait();

Q_SIGNALS:
    void tokenReceived();

private:
    void xdg_activation_token_v1_done(const QString &token) override;

    QString m_token;
};

class XdgActivation : public QtWayland::xdg_activation_v1
{
public:
    explicit XdgActivation(::wl_registry *registry, uint32_t id, int version);
    ~XdgActivation() override;

    std::unique_ptr<XdgActivationToken> createToken();
};

class XdgToplevelSessionV1 : public QObject, public QtWayland::xx_toplevel_session_v1
{
    Q_OBJECT

public:
    explicit XdgToplevelSessionV1(::xx_toplevel_session_v1 *session);
    ~XdgToplevelSessionV1() override;

Q_SIGNALS:
    void restored();

protected:
    void xx_toplevel_session_v1_restored(struct ::xdg_toplevel *surface) override;
};

class XdgSessionV1 : public QObject, public QtWayland::xx_session_v1
{
    Q_OBJECT

public:
    explicit XdgSessionV1(::xx_session_v1 *session);
    ~XdgSessionV1() override;

    std::unique_ptr<XdgToplevelSessionV1> add(XdgToplevel *toplevel, const QString &toplevelId);
    std::unique_ptr<XdgToplevelSessionV1> restore(XdgToplevel *toplevel, const QString &toplevelId);

Q_SIGNALS:
    void created(const QString &id);
    void restored();
    void replaced();

protected:
    void xx_session_v1_created(const QString &id) override;
    void xx_session_v1_restored() override;
    void xx_session_v1_replaced() override;
};

class XdgSessionManagerV1 : public QtWayland::xx_session_manager_v1
{
public:
    XdgSessionManagerV1(::wl_registry *registry, uint32_t id, int version);
    ~XdgSessionManagerV1() override;
};

class KeyStateV1 : public QObject, public QtWayland::org_kde_kwin_keystate
{
    Q_OBJECT
public:
    explicit KeyStateV1(::wl_registry *registry, uint32_t id, int version);
    ~KeyStateV1() override;

    QHash<uint32_t, uint32_t> keyToState;

Q_SIGNALS:
    void stateChanged();

private:
    void org_kde_kwin_keystate_stateChanged(uint32_t key, uint32_t state) override;
};

struct Connection
{
    static std::unique_ptr<Connection> setup(AdditionalWaylandInterfaces interfaces = AdditionalWaylandInterfaces());
    ~Connection();

    bool sync();

    KWayland::Client::ConnectionThread *connection = nullptr;
    KWayland::Client::EventQueue *queue = nullptr;
    KWayland::Client::Compositor *compositor = nullptr;
    KWayland::Client::SubCompositor *subCompositor = nullptr;
    KWayland::Client::ShadowManager *shadowManager = nullptr;
    KWayland::Client::DataDeviceManager *dataDeviceManager = nullptr;
    XdgShell *xdgShell = nullptr;
    KWayland::Client::ShmPool *shm = nullptr;
    KWayland::Client::Seat *seat = nullptr;
    KWayland::Client::PlasmaShell *plasmaShell = nullptr;
    KWayland::Client::PlasmaWindowManagement *windowManagement = nullptr;
    KWayland::Client::PointerConstraints *pointerConstraints = nullptr;
    KWayland::Client::Registry *registry = nullptr;
    WaylandOutputManagementV2 *outputManagementV2 = nullptr;
    QThread *thread = nullptr;
    QList<KWayland::Client::Output *> outputs;
    QList<WaylandOutputDeviceV2 *> outputDevicesV2;
    IdleInhibitManagerV1 *idleInhibitManagerV1 = nullptr;
    KWayland::Client::AppMenuManager *appMenu = nullptr;
    XdgDecorationManagerV1 *xdgDecorationManagerV1 = nullptr;
    KWayland::Client::TextInputManager *textInputManager = nullptr;
    QtWayland::zwp_input_panel_v1 *inputPanelV1 = nullptr;
    MockInputMethod *inputMethodV1 = nullptr;
    QtWayland::zwp_input_method_context_v1 *inputMethodContextV1 = nullptr;
    LayerShellV1 *layerShellV1 = nullptr;
    TextInputManagerV3 *textInputManagerV3 = nullptr;
    FractionalScaleManagerV1 *fractionalScaleManagerV1 = nullptr;
    ScreencastingV1 *screencastingV1 = nullptr;
    ScreenEdgeManagerV1 *screenEdgeManagerV1 = nullptr;
    CursorShapeManagerV1 *cursorShapeManagerV1 = nullptr;
    FakeInput *fakeInput = nullptr;
    SecurityContextManagerV1 *securityContextManagerV1 = nullptr;
    XdgWmDialogV1 *xdgWmDialogV1;
    std::unique_ptr<ColorManagerV1> colorManager;
    std::unique_ptr<FifoManagerV1> fifoManager;
    std::unique_ptr<PresentationTime> presentationTime;
    std::unique_ptr<XdgActivation> xdgActivation;
    std::unique_ptr<XdgSessionManagerV1> sessionManager;
    std::unique_ptr<WpTabletManagerV2> tabletManager;
    std::unique_ptr<KeyStateV1> keyState;
    std::unique_ptr<WpPrimarySelectionDeviceManagerV1> primarySelectionManager;
};

void keyboardKeyPressed(quint32 key, quint32 time);
void keyboardKeyReleased(quint32 key, quint32 time);
void pointerAxisHorizontal(qreal delta,
                           quint32 time,
                           qint32 discreteDelta = 0,
                           PointerAxisSource source = PointerAxisSource::Unknown);
void pointerAxisVertical(qreal delta,
                         quint32 time,
                         qint32 discreteDelta = 0,
                         PointerAxisSource source = PointerAxisSource::Unknown);
void pointerButtonPressed(quint32 button, quint32 time);
void pointerButtonReleased(quint32 button, quint32 time);
void pointerMotion(const QPointF &position, quint32 time);
void pointerMotionRelative(const QPointF &delta, quint32 time);
void touchCancel();
void touchDown(qint32 id, const QPointF &pos, quint32 time);
void touchMotion(qint32 id, const QPointF &pos, quint32 time);
void touchUp(qint32 id, quint32 time);
void tabletPadButtonPressed(quint32 button, quint32 time);
void tabletPadButtonReleased(quint32 button, quint32 time);
void tabletPadDialEvent(double delta, int number, quint32 time);
void tabletPadRingEvent(int position, int number, quint32 group, quint32 mode, quint32 time);
void tabletToolButtonPressed(quint32 button, quint32 time);
void tabletToolButtonReleased(quint32 button, quint32 time);
void tabletToolProximityEvent(const QPointF &pos, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipNear, qreal sliderPosition, quint32 time);
void tabletToolAxisEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, quint32 time);
void tabletToolTipEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, quint32 time);

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
KWayland::Client::DataDeviceManager *waylandDataDeviceManager();
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
ColorManagerV1 *colorManager();
FifoManagerV1 *fifoManager();
PresentationTime *presentationTime();
XdgActivation *xdgActivation();
WpTabletManagerV2 *tabletManager();
KeyStateV1 *keyState();
WpPrimarySelectionDeviceManagerV1 *primarySelectionManager();

bool waitForWaylandSurface(Window *window);

bool waitForWaylandPointer();
bool waitForWaylandPointer(KWayland::Client::Seat *seat);
bool waitForWaylandTouch();
bool waitForWaylandTouch(KWayland::Client::Seat *seat);
bool waitForWaylandKeyboard();
bool waitForWaylandKeyboard(KWayland::Client::Seat *seat);
bool waitForWaylandTabletTool(Test::WpTabletToolV2 *tool);

void flushWaylandConnection();

/**
 * Ensures that all client requests are processed by the compositor and all events
 * sent by the compositor are seen by the client.
 */
bool waylandSync();

std::unique_ptr<KWayland::Client::Surface> createSurface();
std::unique_ptr<KWayland::Client::Surface> createSurface(KWayland::Client::Compositor *compositor);
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
std::unique_ptr<XdgToplevel> createXdgToplevelSurface(XdgShell *shell, KWayland::Client::Surface *surface);
std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface, CreationSetup configureMode);
std::unique_ptr<XdgToplevel> createXdgToplevelSurface(XdgShell *shell, KWayland::Client::Surface *surface, CreationSetup configureMode);
std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface, std::function<void(XdgToplevel *toplevel)> setup);
std::unique_ptr<XdgToplevel> createXdgToplevelSurface(XdgShell *shell, KWayland::Client::Surface *surface, std::function<void(XdgToplevel *toplevel)> setup);

std::unique_ptr<XdgPositioner> createXdgPositioner();

std::unique_ptr<XdgPopup> createXdgPopupSurface(KWayland::Client::Surface *surface, XdgSurface *parentSurface,
                                                XdgPositioner *positioner,
                                                CreationSetup configureMode = CreationSetup::CreateAndConfigure);

std::unique_ptr<XdgToplevelDecorationV1> createXdgToplevelDecorationV1(XdgToplevel *toplevel);
std::unique_ptr<IdleInhibitorV1> createIdleInhibitorV1(KWayland::Client::Surface *surface);
std::unique_ptr<AutoHideScreenEdgeV1> createAutoHideScreenEdgeV1(KWayland::Client::Surface *surface, uint32_t border);
std::unique_ptr<CursorShapeDeviceV1> createCursorShapeDeviceV1(KWayland::Client::Pointer *pointer);
std::unique_ptr<XdgDialogV1> createXdgDialogV1(XdgToplevel *toplevel);
std::unique_ptr<XdgSessionV1> createXdgSessionV1(XdgSessionManagerV1::reason reason, const QString &sessionId = QString());
std::unique_ptr<XdgSessionV1> createXdgSessionV1(XdgSessionManagerV1 *manager, XdgSessionManagerV1::reason reason, const QString &sessionId = QString());

/**
 * Creates a shared memory buffer of @p size in @p color and attaches it to the @p surface.
 * The @p surface gets damaged and committed, thus it's rendered.
 */
void render(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format = QImage::Format_ARGB32_Premultiplied);
void render(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format = QImage::Format_ARGB32_Premultiplied);

/**
 * Creates a shared memory buffer using the supplied image @p img and attaches it to the @p surface
 */
void render(KWayland::Client::Surface *surface, const QImage &img);
void render(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QImage &img);

/**
 * Waits till a new Window is shown and returns the created Window.
 * If no Window gets shown during @p timeout @c null is returned.
 */
Window *waitForWaylandWindowShown(int timeout = 5000);

/**
 * Combination of @link{render} and @link{waitForWaylandWindowShown}.
 */
Window *renderAndWaitForShown(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format = QImage::Format_ARGB32, int timeout = 5000);
Window *renderAndWaitForShown(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format = QImage::Format_ARGB32, int timeout = 5000);

Window *renderAndWaitForShown(KWayland::Client::Surface *surface, const QImage &img, int timeout = 5000);
Window *renderAndWaitForShown(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QImage &img, int timeout = 5000);

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

enum {
    MWM_HINTS_FUNCTIONS = (1L << 0),

    MWM_FUNC_ALL = (1L << 0),
    MWM_FUNC_RESIZE = (1L << 1),
    MWM_FUNC_MOVE = (1L << 2),
    MWM_FUNC_MINIMIZE = (1L << 3),
    MWM_FUNC_MAXIMIZE = (1L << 4),
    MWM_FUNC_CLOSE = (1L << 5),

    MWM_HINTS_DECORATIONS = (1L << 1),

    MWM_DECOR_ALL = (1L << 0),
    MWM_DECOR_BORDER = (1L << 1),
    MWM_DECOR_RESIZEH = (1L << 2),
    MWM_DECOR_TITLE = (1L << 3),
    MWM_DECOR_MENU = (1L << 4),
    MWM_DECOR_MINIMIZE = (1L << 5),
    MWM_DECOR_MAXIMIZE = (1L << 6),
};

struct MotifHints
{
    uint32_t flags = 0;
    uint32_t functions = 0;
    uint32_t decorations = 0;
    int32_t input_mode = 0;
    uint32_t status = 0;
};

void applyMotifHints(xcb_connection_t *connection, xcb_window_t window, const MotifHints &hints);
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

using XkbContextPtr = std::unique_ptr<xkb_context, decltype(&xkb_context_unref)>;
using XkbKeymapPtr = std::unique_ptr<xkb_keymap, decltype(&xkb_keymap_unref)>;
using XkbStatePtr = std::unique_ptr<xkb_state, decltype(&xkb_state_unref)>;

class SimpleKeyboard : public QObject
{
    Q_OBJECT
public:
    explicit SimpleKeyboard(QObject *parent = nullptr);
    KWayland::Client::Keyboard *keyboard();
    QString receviedText();
Q_SIGNALS:
    void receviedTextChanged();
    void keySymRecevied(xkb_keysym_t keysym);

private:
    KWayland::Client::Keyboard *m_keyboard;
    QString m_receviedText;
    XkbContextPtr m_ctx = XkbContextPtr(xkb_context_new(XKB_CONTEXT_NO_FLAGS), &xkb_context_unref);
    XkbKeymapPtr m_keymap{nullptr, &xkb_keymap_unref};
    XkbStatePtr m_state{nullptr, &xkb_state_unref};
};

struct OutputInfo
{
    QRect geometry;
    double scale = 1;
    bool internal = false;
    QSize physicalSizeInMM;
    QList<std::tuple<QSize, uint64_t, OutputMode::Flags>> modes;
    OutputTransform panelOrientation = OutputTransform::Kind::Normal;
    QByteArray edid;
    std::optional<QByteArray> edidIdentifierOverride;
    std::optional<QString> connectorName;
    std::optional<QByteArray> mstPath;
};
void setOutputConfig(const QList<QRect> &geometries);
void setOutputConfig(const QList<OutputInfo> &infos);
}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Test::AdditionalWaylandInterfaces)
Q_DECLARE_METATYPE(KWin::Test::XdgToplevel::States)
Q_DECLARE_METATYPE(QtWayland::zxdg_toplevel_decoration_v1::mode)

#define WAYLANDTEST_MAIN_OPT(TestObject, useDrm)                                                                                          \
    int main(int argc, char *argv[])                                                                                                      \
    {                                                                                                                                     \
        setenv("QT_QPA_PLATFORM", "wayland-org.kde.kwin.qpa", true);                                                                      \
        setenv("QT_QPA_PLATFORM_PLUGIN_PATH", QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath().toLocal8Bit().constData(), true); \
        setenv("KWIN_FORCE_OWN_QPA", "1", true);                                                                                          \
        qunsetenv("KDE_FULL_SESSION");                                                                                                    \
        qunsetenv("KDE_SESSION_VERSION");                                                                                                 \
        qunsetenv("XDG_SESSION_DESKTOP");                                                                                                 \
        qunsetenv("XDG_CURRENT_DESKTOP");                                                                                                 \
        KWin::WaylandTestApplication app(argc, argv, useDrm);                                                                             \
        app.setAttribute(Qt::AA_Use96Dpi, true);                                                                                          \
        TestObject tc;                                                                                                                    \
        return QTest::qExec(&tc, argc, argv);                                                                                             \
    }

#define WAYLANDTEST_MAIN(TestObject) WAYLANDTEST_MAIN_OPT(TestObject, false)
#define WAYLAND_DRM_TEST_MAIN(TestObject) WAYLANDTEST_MAIN_OPT(TestObject, true)

#endif
