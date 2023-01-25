# for things that are also used by kwin libraries
configure_file(libkwineffects/kwinconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects/kwinconfig.h)
configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h)

set(kwin_effects_dbus_xml ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kwin.Effects.xml)
qt_add_dbus_interface(effects_interface_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
add_library(KWinEffectsInterface STATIC ${effects_interface_SRCS})
set_property(TARGET KWinEffectsInterface PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(KWinEffectsInterface Qt::DBus)

add_subdirectory(effects)
add_subdirectory(scripts)
add_subdirectory(tabbox)
add_subdirectory(scripting)
add_subdirectory(helpers)
add_subdirectory(qml)

add_subdirectory(libkwineffects)
if (KWIN_BUILD_KCMS)
    add_subdirectory(kcms)
endif()

add_library(kwin SHARED)
set_target_properties(kwin PROPERTIES
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_sources(kwin PRIVATE
    3rdparty/xcursor.c
    activation.cpp activation.h
    appmenu.cpp appmenu.h
    atoms.cpp atoms.h
    client_machine.cpp client_machine.h
    colors/colordevice.cpp colors/colordevice.h
    colors/colormanager.cpp colors/colormanager.h
    composite.cpp composite.h
    core/colorlut.cpp core/colorlut.h
    core/colorpipelinestage.cpp core/colorpipelinestage.h
    core/colortransformation.cpp core/colortransformation.h
    core/inputbackend.cpp core/inputbackend.h
    core/inputdevice.cpp core/inputdevice.h
    core/output.cpp core/output.h
    core/outputbackend.cpp core/outputbackend.h
    core/outputconfiguration.cpp core/outputconfiguration.h
    core/outputlayer.cpp core/outputlayer.h
    core/overlaywindow.cpp core/overlaywindow.h
    core/renderbackend.cpp core/renderbackend.h
    core/renderjournal.cpp core/renderjournal.h
    core/renderlayer.cpp core/renderlayer.h
    core/renderlayerdelegate.cpp core/renderlayerdelegate.h
    core/renderloop.cpp core/renderloop.h
    core/rendertarget.cpp core/rendertarget.h
    core/session.cpp core/session.h
    core/session_consolekit.cpp core/session_consolekit.h
    core/session_logind.cpp core/session_logind.h
    core/session_noop.cpp core/session_noop.h
    cursor.cpp cursor.h
    cursordelegate_opengl.cpp cursordelegate_opengl.h
    cursordelegate_qpainter.cpp cursordelegate_qpainter.h
    cursorsource.cpp cursorsource.h
    dbusinterface.cpp dbusinterface.h
    debug_console.cpp debug_console.h
    decorations/decoratedclient.cpp decorations/decoratedclient.h
    decorations/decorationbridge.cpp decorations/decorationbridge.h
    decorations/decorationpalette.cpp decorations/decorationpalette.h
    decorations/decorations_logging.cpp decorations/decorations_logging.h
    decorations/settings.cpp decorations/settings.h
    deleted.cpp deleted.h
    dmabuftexture.cpp dmabuftexture.h
    dpmsinputeventfilter.cpp dpmsinputeventfilter.h
    effectloader.cpp effectloader.h
    effects.cpp effects.h
    events.cpp events.h
    focuschain.cpp focuschain.h
    ftrace.cpp ftrace.h
    gestures.cpp gestures.h
    globalshortcuts.cpp globalshortcuts.h
    group.cpp group.h
    hide_cursor_spy.cpp hide_cursor_spy.h
    idle_inhibition.cpp idle_inhibition.h
    idledetector.cpp idledetector.h
    input.cpp input.h
    input_event.cpp input_event.h
    input_event_spy.cpp input_event_spy.h
    inputmethod.cpp inputmethod.h
    inputpanelv1integration.cpp inputpanelv1integration.h
    inputpanelv1window.cpp inputpanelv1window.h
    internalwindow.cpp internalwindow.h
    keyboard_input.cpp keyboard_input.h
    keyboard_layout.cpp keyboard_layout.h
    keyboard_layout_switching.cpp keyboard_layout_switching.h
    keyboard_repeat.cpp keyboard_repeat.h
    killwindow.cpp killwindow.h
    layers.cpp layers.h
    layershellv1integration.cpp layershellv1integration.h
    layershellv1window.cpp layershellv1window.h
    linux_dmabuf.cpp linux_dmabuf.h
    main.cpp main.h
    modifier_only_shortcuts.cpp modifier_only_shortcuts.h
    mousebuttons.cpp mousebuttons.h
    moving_client_x11_filter.cpp moving_client_x11_filter.h
    netinfo.cpp netinfo.h
    onscreennotification.cpp onscreennotification.h
    options.cpp options.h
    osd.cpp osd.h
    outline.cpp outline.h
    placeholderinputeventfilter.cpp placeholderinputeventfilter.h
    placeholderoutput.cpp placeholderoutput.h
    placement.cpp placement.h
    placementtracker.cpp placementtracker.h
    plugin.cpp plugin.h
    pluginmanager.cpp pluginmanager.h
    pointer_input.cpp pointer_input.h
    popup_input_filter.cpp popup_input_filter.h
    rootinfo_filter.cpp rootinfo_filter.h
    rulebooksettings.cpp rulebooksettings.h
    rules.cpp rules.h
    scene/cursoritem.cpp scene/cursoritem.h
    scene/cursorscene.cpp scene/cursorscene.h
    scene/decorationitem.cpp scene/decorationitem.h
    scene/dndiconitem.cpp scene/dndiconitem.h
    scene/imageitem.cpp scene/imageitem.h
    scene/item.cpp scene/item.h
    scene/itemrenderer.cpp scene/itemrenderer.h
    scene/itemrenderer_opengl.cpp scene/itemrenderer_opengl.h
    scene/itemrenderer_qpainter.cpp scene/itemrenderer_qpainter.h
    scene/scene.cpp scene/scene.h
    scene/shadowitem.cpp scene/shadowitem.h
    scene/surfaceitem.cpp scene/surfaceitem.h
    scene/surfaceitem_internal.cpp scene/surfaceitem_internal.h
    scene/surfaceitem_wayland.cpp scene/surfaceitem_wayland.h
    scene/surfaceitem_x11.cpp scene/surfaceitem_x11.h
    scene/windowitem.cpp scene/windowitem.h
    scene/workspacescene.cpp scene/workspacescene.h
    scene/workspacescene_opengl.cpp scene/workspacescene_opengl.h
    scene/workspacescene_qpainter.cpp scene/workspacescene_qpainter.h
    screenedge.cpp screenedge.h
    scripting/dbuscall.cpp scripting/dbuscall.h
    scripting/desktopbackgrounditem.cpp scripting/desktopbackgrounditem.h
    scripting/screenedgeitem.cpp scripting/screenedgeitem.h
    scripting/scriptedeffect.cpp scripting/scriptedeffect.h
    scripting/scripting.cpp scripting/scripting.h
    scripting/scripting_logging.cpp scripting/scripting_logging.h
    scripting/scriptingutils.cpp scripting/scriptingutils.h
    scripting/tilemodel.cpp scripting/tilemodel.h
    scripting/v2/clientmodel.cpp scripting/v2/clientmodel.h
    scripting/v3/clientmodel.cpp scripting/v3/clientmodel.h
    scripting/v3/virtualdesktopmodel.cpp scripting/v3/virtualdesktopmodel.h
    scripting/windowthumbnailitem.cpp scripting/windowthumbnailitem.h
    scripting/workspace_wrapper.cpp scripting/workspace_wrapper.h
    shadow.cpp shadow.h
    sm.cpp sm.h
    syncalarmx11filter.cpp syncalarmx11filter.h
    tablet_input.cpp tablet_input.h
    tabletmodemanager.cpp tabletmodemanager.h
    tiles/customtile.cpp tiles/customtile.h
    tiles/quicktile.cpp tiles/quicktile.h
    tiles/tile.cpp tiles/tile.h
    tiles/tilemanager.cpp tiles/tilemanager.h
    touch_input.cpp touch_input.h
    unmanaged.cpp unmanaged.h
    useractions.cpp useractions.h
    virtualdesktops.cpp virtualdesktops.h
    virtualdesktopsdbustypes.cpp virtualdesktopsdbustypes.h
    virtualkeyboard_dbus.cpp virtualkeyboard_dbus.h
    was_user_interaction_x11_filter.cpp was_user_interaction_x11_filter.h
    wayland_server.cpp wayland_server.h
    waylandshellintegration.cpp waylandshellintegration.h
    waylandwindow.cpp waylandwindow.h
    window.cpp window.h
    window_property_notify_x11_filter.cpp window_property_notify_x11_filter.h
    workspace.cpp workspace.h
    x11eventfilter.cpp x11eventfilter.h
    x11syncmanager.cpp x11syncmanager.h
    x11window.cpp x11window.h
    xdgactivationv1.cpp xdgactivationv1.h
    xdgshellintegration.cpp xdgshellintegration.h
    xdgshellwindow.cpp xdgshellwindow.h
    xkb.cpp xkb.h
    xwaylandwindow.cpp xwaylandwindow.h
)

target_link_libraries(kwin
    kwineffects
    kwinglutils

    Qt::Concurrent
    Qt::DBus
    Qt::Quick

    KF5::ConfigCore
    KF5::ConfigWidgets
    KF5::CoreAddons
    KF5::Crash
    KF5::GlobalAccel
    KF5::GlobalAccelPrivate
    KF5::I18n
    KF5::Package
    KF5::Service
    KF5::WindowSystem

    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private

    XCB::COMPOSITE
    XCB::CURSOR
    XCB::DAMAGE
    XCB::GLX
    XCB::ICCCM
    XCB::KEYSYMS
    XCB::RANDR
    XCB::RENDER
    XCB::SHAPE
    XCB::SHM
    XCB::SYNC
    XCB::XCB
    XCB::XFIXES
    XCB::XINERAMA

    UDev::UDev
    XKB::XKB
    EGL::EGL
    epoxy::epoxy

    Threads::Threads
    Wayland::Server
    lcms2::lcms2
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(kwin Qt5::X11Extras Qt5::XkbCommonSupportPrivate)
endif()

if (KWIN_BUILD_NOTIFICATIONS)
    target_link_libraries(kwin KF5::Notifications)
endif()

kconfig_add_kcfg_files(kwin
    settings.kcfgc
    rulesettings.kcfgc
    rulebooksettingsbase.kcfgc
)

ki18n_wrap_ui(kwin
    debug_console.ui
    shortcutdialog.ui
)

set(kwin_dbus_SRCS)
qt_add_dbus_adaptor(kwin_dbus_SRCS scripting/org.kde.kwin.Script.xml scripting/scripting.h KWin::AbstractScript)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.xml dbusinterface.h KWin::DBusInterface)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.kwin.Compositing.xml dbusinterface.h KWin::CompositorDBusInterface)
qt_add_dbus_adaptor(kwin_dbus_SRCS ${kwin_effects_dbus_xml} effects.h KWin::EffectsHandlerImpl)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.VirtualDesktopManager.xml dbusinterface.h KWin::VirtualDesktopManagerDBusInterface)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Session.xml sm.h KWin::SessionManager)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Plugins.xml dbusinterface.h KWin::PluginManagerDBusInterface)
qt_add_dbus_interface(kwin_dbus_SRCS org.freedesktop.DBus.Properties.xml dbusproperties_interface)

if (KWIN_BUILD_SCREENLOCKER)
    qt_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/kf5_org.freedesktop.ScreenSaver.xml screenlocker_interface)
    qt_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/org.kde.screensaver.xml kscreenlocker_interface)
endif()

qt_add_dbus_interface(kwin_dbus_SRCS org.kde.kappmenu.xml appmenu_interface)

target_sources(kwin PRIVATE
    ${kwin_dbus_SRCS}
)

add_subdirectory(backends)
add_subdirectory(utils)
add_subdirectory(wayland)
add_subdirectory(xwayland)

if (KWIN_BUILD_ACTIVITIES)
    target_sources(kwin PRIVATE activities.cpp activities.h)
    target_link_libraries(kwin KF5::Activities)
endif()

if (KWIN_BUILD_SCREENLOCKER)
    target_sources(kwin PRIVATE screenlockerwatcher.cpp screenlockerwatcher.h)
    target_link_libraries(kwin PW::KScreenLocker)
endif()

if (KWIN_BUILD_TABBOX)
    target_sources(kwin PRIVATE
        tabbox/clientmodel.cpp tabbox/clientmodel.h
        tabbox/desktopchain.cpp tabbox/desktopchain.h
        tabbox/desktopmodel.cpp tabbox/desktopmodel.h
        tabbox/switcheritem.cpp tabbox/switcheritem.h
        tabbox/tabbox.cpp tabbox/tabbox.h
        tabbox/tabbox_logging.cpp tabbox/tabbox_logging.h
        tabbox/tabboxconfig.cpp tabbox/tabboxconfig.h
        tabbox/tabboxhandler.cpp tabbox/tabboxhandler.h
        tabbox/x11_filter.cpp tabbox/x11_filter.h
    )
    target_link_libraries(kwin Qt::GuiPrivate)
endif()

qt_generate_dbus_interface(virtualkeyboard_dbus.h org.kde.kwin.VirtualKeyboard.xml OPTIONS -A)
qt_generate_dbus_interface(tabletmodemanager.h org.kde.KWin.TabletModeManager.xml OPTIONS -A)

generate_export_header(kwin EXPORT_FILE_NAME kwin_export.h)

add_executable(kwin_x11 main_x11.cpp main_x11.h)
target_link_libraries(kwin_x11
    KWinX11Platform
    kwin
)
kcoreaddons_target_static_plugins(kwin_x11 "kwin/effects/plugins")

install(TARGETS kwin ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
install(TARGETS kwin_x11 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

add_executable(kwin_wayland main_wayland.cpp main_wayland.h)

target_link_libraries(kwin_wayland
    kwin
    KWinXwaylandServerModule
)
kcoreaddons_target_static_plugins(kwin_wayland "kwin/effects/plugins")

install(TARGETS kwin_wayland ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
if (HAVE_LIBCAP)
    install(
    CODE "execute_process(
            COMMAND
                ${SETCAP_EXECUTABLE}
                CAP_SYS_NICE=+ep
                \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/kwin_wayland)"
    )
endif()

add_subdirectory(platformsupport)
add_subdirectory(plugins)

target_link_libraries(kwin_wayland
    KWinQpaPlugin
    KF5GlobalAccelKWinPlugin
    KF5WindowSystemKWinPlugin
    KF5IdleTimeKWinPlugin
)

add_custom_target(
    KWinDBusInterfaces
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml
            ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml
)

if (PipeWire_FOUND)
    target_link_libraries(kwin_wayland KWinScreencastPlugin)
endif()

install(FILES kwin.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
if (KWIN_BUILD_NOTIFICATIONS)
    install(FILES kwin.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
endif()
install(
    FILES
        org.kde.KWin.VirtualDesktopManager.xml
        org.kde.KWin.xml
        org.kde.kwin.Compositing.xml
        org.kde.kwin.Effects.xml
        org.kde.KWin.Plugins.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml
    DESTINATION
        ${KDE_INSTALL_DBUSINTERFACEDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kwin_export.h DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)

# Install the KWin/Script service type
install(FILES scripting/kwinscript.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPESDIR})
