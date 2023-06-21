ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KWIN
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWinConfigVersion.cmake"
                  SOVERSION 6
)

set(KWIN_KILLER_BIN ${CMAKE_INSTALL_FULL_LIBEXECDIR}/kwin_killer_helper)

configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h)

set(kwin_effects_dbus_xml ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kwin.Effects.xml)
qt_add_dbus_interface(effects_interface_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
add_library(KWinEffectsInterface STATIC ${effects_interface_SRCS})
set_property(TARGET KWinEffectsInterface PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(KWinEffectsInterface Qt::DBus)

add_subdirectory(helpers)
add_subdirectory(qml)

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcms)
endif()

add_library(kwin SHARED)
target_include_directories(kwin INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/kwin>")
set_target_properties(kwin PROPERTIES
   VERSION ${PROJECT_VERSION}
   SOVERSION 6
)

target_compile_definitions(kwin PRIVATE
    -DTRANSLATION_DOMAIN=\"kwin\"
)

target_sources(kwin PRIVATE
    3rdparty/xcursor.c
    activation.cpp
    appmenu.cpp
    client_machine.cpp
    colors/colordevice.cpp
    colors/colormanager.cpp
    compositor.cpp
    compositor_wayland.cpp
    core/brightnessdevice.cpp
    core/colorlut3d.cpp
    core/colorpipeline.cpp
    core/colorpipelinestage.cpp
    core/colorspace.cpp
    core/colortransformation.cpp
    core/drmdevice.cpp
    core/gbmgraphicsbufferallocator.cpp
    core/graphicsbuffer.cpp
    core/graphicsbufferallocator.cpp
    core/graphicsbufferview.cpp
    core/iccprofile.cpp
    core/inputbackend.cpp
    core/inputdevice.cpp
    core/output.cpp
    core/outputbackend.cpp
    core/outputconfiguration.cpp
    core/outputlayer.cpp
    core/overlaywindow.cpp
    core/renderbackend.cpp
    core/renderjournal.cpp
    core/renderlayer.cpp
    core/renderlayerdelegate.cpp
    core/renderloop.cpp
    core/rendertarget.cpp
    core/renderviewport.cpp
    core/session.cpp
    core/session_consolekit.cpp
    core/session_logind.cpp
    core/session_noop.cpp
    core/shmgraphicsbufferallocator.cpp
    core/syncobjtimeline.cpp
    cursor.cpp
    cursorsource.cpp
    dbusinterface.cpp
    debug_console.cpp
    decorations/decoratedclient.cpp
    decorations/decorationbridge.cpp
    decorations/decorationpalette.cpp
    decorations/decorations_logging.cpp
    decorations/settings.cpp
    dpmsinputeventfilter.cpp
    effect/anidata.cpp
    effect/animationeffect.cpp
    effect/effect.cpp
    effect/effectframe.cpp
    effect/effecthandler.cpp
    effect/effectloader.cpp
    effect/effecttogglablestate.cpp
    effect/effectwindow.cpp
    effect/logging.cpp
    effect/offscreeneffect.cpp
    effect/offscreenquickview.cpp
    effect/quickeffect.cpp
    effect/timeline.cpp
    focuschain.cpp
    ftrace.cpp
    gestures.cpp
    globalshortcuts.cpp
    hide_cursor_spy.cpp
    idle_inhibition.cpp
    idledetector.cpp
    input.cpp
    input_event.cpp
    input_event_spy.cpp
    inputmethod.cpp
    inputpanelv1integration.cpp
    inputpanelv1window.cpp
    internalwindow.cpp
    keyboard_input.cpp
    keyboard_layout.cpp
    keyboard_layout_switching.cpp
    keyboard_repeat.cpp
    killprompt.cpp
    killwindow.cpp
    kscreenintegration.cpp
    layers.cpp
    layershellv1integration.cpp
    layershellv1window.cpp
    lidswitchtracker.cpp
    main.cpp
    mousebuttons.cpp
    onscreennotification.cpp
    opengl/abstract_opengl_context_attribute_builder.cpp
    opengl/egl_context_attribute_builder.cpp
    opengl/eglcontext.cpp
    opengl/egldisplay.cpp
    opengl/eglimagetexture.cpp
    opengl/eglnativefence.cpp
    opengl/eglswapchain.cpp
    opengl/glframebuffer.cpp
    opengl/gllut.cpp
    opengl/gllut3D.cpp
    opengl/glplatform.cpp
    opengl/glrendertimequery.cpp
    opengl/glshader.cpp
    opengl/glshadermanager.cpp
    opengl/gltexture.cpp
    opengl/glutils.cpp
    opengl/glvertexbuffer.cpp
    opengl/openglcontext.cpp
    options.cpp
    osd.cpp
    outline.cpp
    outputconfigurationstore.cpp
    placeholderinputeventfilter.cpp
    placeholderoutput.cpp
    placement.cpp
    placementtracker.cpp
    plugin.cpp
    pluginmanager.cpp
    pointer_input.cpp
    popup_input_filter.cpp
    resources.qrc
    rulebooksettings.cpp
    rules.cpp
    scene/cursordelegate_opengl.cpp
    scene/cursordelegate_qpainter.cpp
    scene/cursordelegate_vulkan.cpp
    scene/cursoritem.cpp
    scene/cursorscene.cpp
    scene/decorationitem.cpp
    scene/dndiconitem.cpp
    scene/imageitem.cpp
    scene/item.cpp
    scene/itemgeometry.cpp
    scene/itemrenderer.cpp
    scene/itemrenderer_opengl.cpp
    scene/itemrenderer_qpainter.cpp
    scene/itemrenderer_vulkan.cpp
    scene/rootitem.cpp
    scene/scene.cpp
    scene/shadowitem.cpp
    scene/surfaceitem.cpp
    scene/surfaceitem_internal.cpp
    scene/surfaceitem_wayland.cpp
    scene/windowitem.cpp
    scene/workspacescene.cpp
    scene/workspacescene_opengl.cpp
    scene/workspacescene_qpainter.cpp
    scene/workspacescene_vulkan.cpp
    screenedge.cpp
    scripting/dbuscall.cpp
    scripting/desktopbackgrounditem.cpp
    scripting/gesturehandler.cpp
    scripting/screenedgehandler.cpp
    scripting/scriptedeffect.cpp
    scripting/scriptedquicksceneeffect.cpp
    scripting/scripting.cpp
    scripting/scripting_logging.cpp
    scripting/scriptingutils.cpp
    scripting/shortcuthandler.cpp
    scripting/tilemodel.cpp
    scripting/virtualdesktopmodel.cpp
    scripting/windowmodel.cpp
    scripting/windowthumbnailitem.cpp
    scripting/workspace_wrapper.cpp
    shadow.cpp
    sm.cpp
    tablet_input.cpp
    tabletmodemanager.cpp
    tiles/customtile.cpp
    tiles/quicktile.cpp
    tiles/tile.cpp
    tiles/tilemanager.cpp
    touch_input.cpp
    useractions.cpp
    utils/svgcursorreader.cpp
    utils/version.cpp
    utils/xcursorreader.cpp
    virtualdesktops.cpp
    virtualdesktopsdbustypes.cpp
    virtualkeyboard_dbus.cpp
    vulkan/vulkan_device.cpp
    vulkan/vulkan_logging.cpp
    vulkan/vulkan_swapchain.cpp
    vulkan/vulkan_texture.cpp
    wayland_server.cpp
    waylandshellintegration.cpp
    waylandwindow.cpp
    window.cpp
    workspace.cpp
    xdgactivationv1.cpp
    xdgshellintegration.cpp
    xdgshellwindow.cpp
    xkb.cpp
)

target_link_libraries(kwin
    PUBLIC
        Qt::DBus
        Qt::Quick
        Qt::Widgets
        Wayland::Server
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::WindowSystem
        epoxy::epoxy

    PRIVATE
        Qt::Concurrent
        Qt::GuiPrivate
        Qt::Sensors
        Qt::Svg

        KF6::ColorScheme
        KF6::ConfigGui
        KF6::ConfigQml
        KF6::Crash
        KF6::GlobalAccel
        KF6::I18n
        KF6::Package
        KF6::Service

        KDecoration2::KDecoration
        KDecoration2::KDecoration2Private

        UDev::UDev
        XKB::XKB
        EGL::EGL
        epoxy::epoxy
        Vulkan::Vulkan

        Threads::Threads
        lcms2::lcms2
        PkgConfig::libdisplayinfo
)

if (TARGET K::KGlobalAccelD)
    target_link_libraries(kwin PRIVATE K::KGlobalAccelD)
endif()

if (KWIN_BUILD_X11)
    target_sources(kwin
        PRIVATE
            atoms.cpp
            events.cpp
            compositor_x11.cpp
            group.cpp
            moving_client_x11_filter.cpp
            netinfo.cpp
            rootinfo_filter.cpp
            scene/surfaceitem_x11.cpp
            syncalarmx11filter.cpp
            was_user_interaction_x11_filter.cpp
            window_property_notify_x11_filter.cpp
            x11eventfilter.cpp
            x11syncmanager.cpp
            x11window.cpp
    )
    target_link_libraries(kwin
        PRIVATE
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
    )
endif()

if (KWIN_BUILD_NOTIFICATIONS)
    target_link_libraries(kwin PRIVATE KF6::Notifications)
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
qt_add_dbus_adaptor(kwin_dbus_SRCS ${kwin_effects_dbus_xml} effect/effecthandler.h KWin::EffectsHandler)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.VirtualDesktopManager.xml dbusinterface.h KWin::VirtualDesktopManagerDBusInterface)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Session.xml sm.h KWin::SessionManager)
qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Plugins.xml dbusinterface.h KWin::PluginManagerDBusInterface)
qt_add_dbus_interface(kwin_dbus_SRCS org.freedesktop.DBus.Properties.xml dbusproperties_interface)

if (KWIN_BUILD_SCREENLOCKER)
    qt_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/kf6_org.freedesktop.ScreenSaver.xml screenlocker_interface)
    qt_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/org.kde.screensaver.xml kscreenlocker_interface)
endif()

qt_add_dbus_interface(kwin_dbus_SRCS org.kde.kappmenu.xml appmenu_interface)

target_sources(kwin PRIVATE
    ${kwin_dbus_SRCS}
)

add_subdirectory(backends)
add_subdirectory(platformsupport)
add_subdirectory(plugins)
add_subdirectory(utils)
add_subdirectory(wayland)

if (KWIN_BUILD_X11)
    add_subdirectory(xwayland)
endif()

if (KWIN_BUILD_ACTIVITIES)
    target_sources(kwin PRIVATE activities.cpp)
    target_link_libraries(kwin PRIVATE Plasma::Activities)
endif()

if (KWIN_BUILD_SCREENLOCKER)
    target_sources(kwin PRIVATE screenlockerwatcher.cpp)
    target_link_libraries(kwin PRIVATE PW::KScreenLocker)
endif()

if (KWIN_BUILD_TABBOX)
    target_sources(kwin PRIVATE
        tabbox/clientmodel.cpp
        tabbox/switcheritem.cpp
        tabbox/tabbox.cpp
        tabbox/tabbox_logging.cpp
        tabbox/tabboxconfig.cpp
        tabbox/tabboxhandler.cpp
    )
    add_subdirectory(tabbox/switchers)

    if (KWIN_BUILD_X11)
        target_sources(kwin PRIVATE tabbox/x11_filter.cpp)
    endif()
endif()

qt_generate_dbus_interface(virtualkeyboard_dbus.h org.kde.kwin.VirtualKeyboard.xml OPTIONS -A)
qt_generate_dbus_interface(tabletmodemanager.h org.kde.KWin.TabletModeManager.xml OPTIONS -A)

generate_export_header(kwin EXPORT_FILE_NAME kwin_export.h)

if (KWIN_BUILD_X11)
    add_executable(kwin_x11 main_x11.cpp)
    target_link_libraries(kwin_x11
        KWinX11Platform
        kwin
        KF6::GlobalAccel
    )
    target_compile_definitions(kwin_x11 PRIVATE
        -DTRANSLATION_DOMAIN=\"kwin\"
    )
    kcoreaddons_target_static_plugins(kwin_x11 NAMESPACE "kwin/effects/plugins")
    install(TARGETS kwin_x11 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
endif()

install(TARGETS kwin EXPORT KWinTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

add_executable(kwin_wayland main_wayland.cpp)

if(TARGET PkgConfig::libsystemd)
    ecm_qt_declare_logging_category(kwin_wayland
        HEADER watchdoglogging.h
        IDENTIFIER KWIN_WATCHDOG
        CATEGORY_NAME kwin_watchdog
        DEFAULT_SEVERITY Info
    )

    target_sources(kwin_wayland PRIVATE watchdog.cpp)
    target_link_libraries(kwin_wayland PkgConfig::libsystemd)
endif()

target_link_libraries(kwin_wayland
    kwin
    KF6::Crash
    KF6::I18n
)
if (KWIN_BUILD_X11)
    target_link_libraries(kwin_wayland KWinXwaylandServerModule)
endif()
target_compile_definitions(kwin_wayland PRIVATE
    -DTRANSLATION_DOMAIN=\"kwin\"
)
kcoreaddons_target_static_plugins(kwin_wayland NAMESPACE "kwin/effects/plugins")

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

target_link_libraries(kwin_wayland
    KWinQpaPlugin
    KF6WindowSystemKWinPlugin
    KF6IdleTimeKWinPlugin
)

if (TARGET KF6GlobalAccelKWinPlugin)
    target_link_libraries(kwin_wayland KF6GlobalAccelKWinPlugin)
endif()

add_custom_target(
    KWinDBusInterfaces
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml
            ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml
)

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

install(EXPORT KWinTargets DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/KWin" FILE KWinTargets.cmake NAMESPACE KWin:: )

if (KWIN_BUILD_X11)
    install(FILES atoms.h DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin COMPONENT Devel)
endif()

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwin_export.h
    activities.h
    appmenu.h
    client_machine.h
    compositor.h
    cursor.h
    cursorsource.h
    dbusinterface.h
    debug_console.h
    focuschain.h
    ftrace.h
    gestures.h
    globalshortcuts.h
    group.h
    idledetector.h
    idle_inhibition.h
    input_event.h
    input_event_spy.h
    input.h
    inputmethod.h
    inputpanelv1integration.h
    inputpanelv1window.h
    internalwindow.h
    keyboard_input.h
    keyboard_layout.h
    keyboard_layout_switching.h
    keyboard_repeat.h
    killwindow.h
    kscreenintegration.h
    layershellv1integration.h
    layershellv1window.h
    lidswitchtracker.h
    main.h
    mousebuttons.h
    netinfo.h
    onscreennotification.h
    options.h
    osd.h
    outline.h
    outputconfigurationstore.h
    placeholderoutput.h
    placement.h
    placementtracker.h
    plugin.h
    pluginmanager.h
    pointer_input.h
    rulebooksettings.h
    rules.h
    screenedge.h
    screenlockerwatcher.h
    shadow.h
    sm.h
    tablet_input.h
    tabletmodemanager.h
    touch_input.h
    useractions.h
    virtualdesktopsdbustypes.h
    virtualdesktops.h
    virtualkeyboard_dbus.h
    wayland_server.h
    waylandshellintegration.h
    waylandwindow.h
    window.h
    workspace.h
    x11eventfilter.h
    x11window.h
    xdgactivationv1.h
    xdgshellintegration.h
    xdgshellwindow.h
    xkb.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin COMPONENT Devel)

install(FILES
    core/colorlut3d.h
    core/colorpipeline.h
    core/colorpipelinestage.h
    core/colorspace.h
    core/colortransformation.h
    core/drmdevice.h
    core/gbmgraphicsbufferallocator.h
    core/graphicsbuffer.h
    core/graphicsbufferallocator.h
    core/graphicsbufferview.h
    core/iccprofile.h
    core/inputbackend.h
    core/inputdevice.h
    core/output.h
    core/outputbackend.h
    core/outputconfiguration.h
    core/outputlayer.h
    core/overlaywindow.h
    core/pixelgrid.h
    core/renderbackend.h
    core/renderjournal.h
    core/renderlayer.h
    core/renderlayerdelegate.h
    core/renderloop.h
    core/renderloop_p.h
    core/rendertarget.h
    core/renderviewport.h
    core/session.h
    core/session_consolekit.h
    core/session_logind.h
    core/session_noop.h
    core/shmgraphicsbufferallocator.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/core COMPONENT Devel)

install(FILES
    utils/c_ptr.h
    utils/common.h
    utils/cursortheme.h
    utils/damagejournal.h
    utils/drm_format_helper.h
    utils/edid.h
    utils/executable_path.h
    utils/filedescriptor.h
    utils/kernel.h
    utils/memorymap.h
    utils/orientationsensor.h
    utils/ramfile.h
    utils/realtime.h
    utils/resource.h
    utils/serviceutils.h
    utils/softwarevsyncmonitor.h
    utils/subsurfacemonitor.h
    utils/udev.h
    utils/version.h
    utils/vsyncmonitor.h
    utils/xcbutils.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/utils COMPONENT Devel)

install(FILES
    effect/animationeffect.h
    effect/effect.h
    effect/effecthandler.h
    effect/effecttogglablestate.h
    effect/effectwindow.h
    effect/globals.h
    effect/offscreeneffect.h
    effect/offscreenquickview.h
    effect/quickeffect.h
    effect/timeline.h
    effect/xcb.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/effect COMPONENT Devel)

install(FILES
    opengl/abstract_opengl_context_attribute_builder.h
    opengl/egl_context_attribute_builder.h
    opengl/eglcontext.h
    opengl/egldisplay.h
    opengl/eglimagetexture.h
    opengl/eglnativefence.h
    opengl/eglswapchain.h
    opengl/eglutils_p.h
    opengl/glframebuffer.h
    opengl/gllut3D.h
    opengl/gllut.h
    opengl/glplatform.h
    opengl/glrendertimequery.h
    opengl/glshader.h
    opengl/glshadermanager.h
    opengl/gltexture.h
    opengl/gltexture_p.h
    opengl/glutils.h
    opengl/glvertexbuffer.h
    opengl/openglcontext.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/opengl COMPONENT Devel
)

install(FILES
    vulkan/vulkan_device.h
    vulkan/vulkan_include.h
    vulkan/vulkan_swapchain.h
    vulkan/vulkan_texture.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/vulkan COMPONENT Devel
)

install(FILES
    scene/cursordelegate_opengl.h
    scene/cursordelegate_qpainter.h
    scene/cursoritem.h
    scene/cursorscene.h
    scene/decorationitem.h
    scene/dndiconitem.h
    scene/imageitem.h
    scene/itemgeometry.h
    scene/item.h
    scene/itemrenderer.h
    scene/itemrenderer_opengl.h
    scene/itemrenderer_qpainter.h
    scene/rootitem.h
    scene/scene.h
    scene/shadowitem.h
    scene/surfaceitem.h
    scene/surfaceitem_internal.h
    scene/surfaceitem_wayland.h
    scene/surfaceitem_x11.h
    scene/windowitem.h
    scene/workspacescene.h
    scene/workspacescene_opengl.h
    scene/workspacescene_qpainter.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/scene COMPONENT Devel
)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KWin")

install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/KWinConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/KWinConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel )

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KWinConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KWinConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(EXPORT KWinTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KWinTargets.cmake NAMESPACE KWin:: )
