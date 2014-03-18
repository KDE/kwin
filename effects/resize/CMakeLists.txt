#######################################
# Effect
install( FILES
    resize.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_resize_config_SRCS resize_config.cpp)
qt5_wrap_ui(kwin_resize_config_SRCS resize_config.ui)
qt5_add_dbus_interface(kwin_resize_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_resize_config_SRCS resizeconfig.kcfgc)

add_library(kwin_resize_config MODULE ${kwin_resize_config_SRCS})

target_link_libraries(kwin_resize_config
    Qt5::DBus
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_resize_config resize_config.desktop)

install(
    TARGETS
        kwin_resize_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
