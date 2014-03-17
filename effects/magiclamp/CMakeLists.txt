#######################################
# Effect
install( FILES
    magiclamp.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
qt5_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

add_library(kwin_magiclamp_config MODULE ${kwin_magiclamp_config_SRCS})

target_link_libraries(kwin_magiclamp_config
    kwineffects
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_magiclamp_config magiclamp_config.desktop)

install(
    TARGETS
        kwin_magiclamp_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
