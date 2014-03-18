#######################################
# Effect
install( FILES
         dashboard.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_dashboard_config_SRCS dashboard_config.cpp)
qt5_wrap_ui(kwin_dashboard_config_SRCS dashboard_config.ui)
qt5_add_dbus_interface(kwin_dashboard_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_dashboard_config_SRCS dashboardconfig.kcfgc)

add_library(kwin_dashboard_config MODULE ${kwin_dashboard_config_SRCS})

target_link_libraries(kwin_dashboard_config
    Qt5::DBus
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_dashboard_config dashboard_config.desktop)

install(
    TARGETS
        kwin_dashboard_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
