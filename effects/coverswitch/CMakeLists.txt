#######################################
# Effect
install( FILES
    coverswitch.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

install( FILES
    shaders/1.10/coverswitch-reflection.glsl
    DESTINATION ${DATA_INSTALL_DIR}/kwin/shaders/1.10 )
install( FILES
    shaders/1.40/coverswitch-reflection.glsl
    DESTINATION ${DATA_INSTALL_DIR}/kwin/shaders/1.40 )


#######################################
# Config
set(kwin_coverswitch_config_SRCS coverswitch_config.cpp)
qt5_wrap_ui(kwin_coverswitch_config_SRCS coverswitch_config.ui)
kconfig_add_kcfg_files(kwin_coverswitch_config_SRCS coverswitchconfig.kcfgc)

add_library(kwin_coverswitch_config MODULE ${kwin_coverswitch_config_SRCS})

target_link_libraries(kwin_coverswitch_config
    kwineffects
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_coverswitch_config coverswitch_config.desktop)

install(
    TARGETS
        kwin_coverswitch_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
