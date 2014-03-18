#######################################
# Effect

# .desktop files
install( FILES
    cube.desktop
    cubeslide.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    data/cubecap.png
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

install( FILES
    data/1.10/cube-cap.glsl
    data/1.10/cube-reflection.glsl
    data/1.10/cylinder.vert
    data/1.10/sphere.vert
    DESTINATION ${DATA_INSTALL_DIR}/kwin/shaders/1.10
)

install( FILES
    data/1.40/cube-cap.glsl
    data/1.40/cube-reflection.glsl
    data/1.40/cylinder.vert
    data/1.40/sphere.vert
    DESTINATION ${DATA_INSTALL_DIR}/kwin/shaders/1.40
)

#######################################
# Config

# cube
set(kwin_cube_config_SRCS cube_config.cpp)
qt5_wrap_ui(kwin_cube_config_SRCS cube_config.ui)
qt5_add_dbus_interface(kwin_cube_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_cube_config_SRCS cubeconfig.kcfgc)

add_library(kwin_cube_config MODULE ${kwin_cube_config_SRCS})

target_link_libraries(kwin_cube_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KF5::KIOWidgets
    KF5::Service
)

kservice_desktop_to_json(kwin_cube_config cube_config.desktop)

# cube slide
set(kwin_cubeslide_config_SRCS cubeslide_config.cpp)
qt5_wrap_ui(kwin_cubeslide_config_SRCS cubeslide_config.ui)
qt5_add_dbus_interface(kwin_cubeslide_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_cubeslide_config_SRCS cubeslideconfig.kcfgc)

add_library(kwin_cubeslide_config MODULE ${kwin_cubeslide_config_SRCS})

target_link_libraries(kwin_cubeslide_config
    kwineffects
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_cubeslide_config cubeslide_config.desktop)

install(
    TARGETS
        kwin_cube_config
        kwin_cubeslide_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
