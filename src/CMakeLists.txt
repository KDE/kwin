ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KWIN
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWinConfigVersion.cmake"
                  SOVERSION 6
)

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
    atoms.cpp
    client_machine.cpp
    colors/colordevice.cpp
    colors/colormanager.cpp
    compositor.cpp
    compositor_wayland.cpp
    compositor_x11.cpp
    core/colorlut.cpp
    core/colorlut3d.cpp
    core/colorpipelinestage.cpp
    core/colortransformation.cpp
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
    core/session.cpp
    core/session_consolekit.cpp
    core/session_logind.cpp
    core/session_noop.cpp
    core/shmgraphicsbufferallocator.cpp
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
    effectloader.cpp
    effects.cpp
    events.cpp
    focuschain.cpp
    ftrace.cpp
    gestures.cpp
    globalshortcuts.cpp
    group.cpp
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
    killwindow.cpp
    kscreenintegration.cpp
    layers.cpp
    layershellv1integration.cpp
    layershellv1window.cpp
    libkwineffects/anidata.cpp
    libkwineffects/colorspace.cpp
    libkwineffects/effecttogglablestate.cpp
    libkwineffects/glframebuffer.cpp
    libkwineffects/gllut.cpp
    libkwineffects/gllut3D.cpp
    libkwineffects/glplatform.cpp
    libkwineffects/glshader.cpp
    libkwineffects/glshadermanager.cpp
    libkwineffects/gltexture.cpp
    libkwineffects/glutils.cpp
    libkwineffects/glutils_funcs.cpp
    libkwineffects/glvertexbuffer.cpp
    libkwineffects/kwinanimationeffect.cpp
    libkwineffects/kwineffects.cpp
    libkwineffects/kwinoffscreeneffect.cpp
    libkwineffects/kwinoffscreenquickview.cpp
    libkwineffects/kwinquickeffect.cpp
    libkwineffects/logging.cpp
    libkwineffects/openglcontext.cpp
    libkwineffects/rendertarget.cpp
    libkwineffects/renderviewport.cpp
    libkwineffects/version.cpp
    lidswitchtracker.cpp
    main.cpp
    modifier_only_shortcuts.cpp
    mousebuttons.cpp
    moving_client_x11_filter.cpp
    netinfo.cpp
    onscreennotification.cpp
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
    rootinfo_filter.cpp
    rulebooksettings.cpp
    rules.cpp
    scene/cursordelegate_opengl.cpp
    scene/cursordelegate_qpainter.cpp
    scene/cursoritem.cpp
    scene/cursorscene.cpp
    scene/decorationitem.cpp
    scene/dndiconitem.cpp
    scene/imageitem.cpp
    scene/item.cpp
    scene/itemrenderer.cpp
    scene/itemrenderer_opengl.cpp
    scene/itemrenderer_qpainter.cpp
    scene/scene.cpp
    scene/shadowitem.cpp
    scene/surfaceitem.cpp
    scene/surfaceitem_internal.cpp
    scene/surfaceitem_wayland.cpp
    scene/surfaceitem_x11.cpp
    scene/windowitem.cpp
    scene/workspacescene.cpp
    scene/workspacescene_opengl.cpp
    scene/workspacescene_qpainter.cpp
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
    syncalarmx11filter.cpp
    tablet_input.cpp
    tabletmodemanager.cpp
    tiles/customtile.cpp
    tiles/quicktile.cpp
    tiles/tile.cpp
    tiles/tilemanager.cpp
    touch_input.cpp
    useractions.cpp
    virtualdesktops.cpp
    virtualdesktopsdbustypes.cpp
    virtualkeyboard_dbus.cpp
    was_user_interaction_x11_filter.cpp
    wayland_server.cpp
    waylandshellintegration.cpp
    waylandwindow.cpp
    window.cpp
    window_property_notify_x11_filter.cpp
    workspace.cpp
    x11eventfilter.cpp
    x11syncmanager.cpp
    x11window.cpp
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
    Qt::Sensors

    KF6::ConfigQml
    KF6::ConfigWidgets
    KF6::Crash
    KF6::GlobalAccel
    KF6::I18n
    KF6::Package
    KF6::Service

    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private

    K::KGlobalAccelD

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
    lcms2::lcms2
    PkgConfig::libdisplayinfo
)

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
qt_add_dbus_adaptor(kwin_dbus_SRCS ${kwin_effects_dbus_xml} effects.h KWin::EffectsHandlerImpl)
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
add_subdirectory(xwayland)

if (KWIN_BUILD_ACTIVITIES)
    target_sources(kwin PRIVATE activities.cpp)
    target_link_libraries(kwin PRIVATE KF6::Activities)
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
        tabbox/x11_filter.cpp
    )
    target_link_libraries(kwin PRIVATE Qt::GuiPrivate)
    add_subdirectory(tabbox/switchers)
endif()

qt_generate_dbus_interface(virtualkeyboard_dbus.h org.kde.kwin.VirtualKeyboard.xml OPTIONS -A)
qt_generate_dbus_interface(tabletmodemanager.h org.kde.KWin.TabletModeManager.xml OPTIONS -A)

generate_export_header(kwin EXPORT_FILE_NAME kwin_export.h)

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

install(TARGETS kwin EXPORT KWinTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
install(TARGETS kwin_x11 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

add_executable(kwin_wayland main_wayland.cpp)

target_link_libraries(kwin_wayland
    kwin
    KWinXwaylandServerModule
    KF6::Crash
)
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
    KF6GlobalAccelKWinPlugin
    KF6WindowSystemKWinPlugin
    KF6IdleTimeKWinPlugin
)

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

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwin_export.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin COMPONENT Devel)

install(FILES
    core/output.h
    core/renderloop.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/core COMPONENT Devel)

install(FILES
    utils/edid.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/utils COMPONENT Devel)

install(FILES
    libkwineffects/colorspace.h
    libkwineffects/glframebuffer.h
    libkwineffects/gllut.h
    libkwineffects/gllut3D.h
    libkwineffects/glplatform.h
    libkwineffects/glshader.h
    libkwineffects/glshadermanager.h
    libkwineffects/gltexture.h
    libkwineffects/glutils.h
    libkwineffects/glutils_funcs.h
    libkwineffects/glvertexbuffer.h
    libkwineffects/kwinanimationeffect.h
    libkwineffects/kwineffects.h
    libkwineffects/kwinglobals.h
    libkwineffects/kwinoffscreeneffect.h
    libkwineffects/kwinoffscreenquickview.h
    libkwineffects/kwinquickeffect.h
    libkwineffects/rendertarget.h
    libkwineffects/renderviewport.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/libkwineffects COMPONENT Devel)

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
