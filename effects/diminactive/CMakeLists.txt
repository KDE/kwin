#######################################
# Effect
install( FILES
    diminactive.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_diminactive_config_SRCS diminactive_config.cpp)
qt5_wrap_ui(kwin_diminactive_config_SRCS diminactive_config.ui)
kconfig_add_kcfg_files(kwin_diminactive_config_SRCS diminactiveconfig.kcfgc)

add_library(kwin_diminactive_config MODULE ${kwin_diminactive_config_SRCS})

target_link_libraries(kwin_diminactive_config
    kwineffects
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_diminactive_config diminactive_config.desktop)

install(
    TARGETS
        kwin_diminactive_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
