#######################################
# Effect
install( FILES
    showfps.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_showfps_config_SRCS showfps_config.cpp)
qt5_wrap_ui(kwin_showfps_config_SRCS showfps_config.ui)
kconfig_add_kcfg_files(kwin_showfps_config_SRCS showfpsconfig.kcfgc)

add_library(kwin_showfps_config MODULE ${kwin_showfps_config_SRCS})

target_link_libraries(kwin_showfps_config
    kwineffects
    KF5::Completion
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_showfps_config showfps_config.desktop)

install(
    TARGETS
        kwin_showfps_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
