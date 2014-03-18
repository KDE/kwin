#######################################
# Effect
install( FILES
    glide.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_glide_config_SRCS glide_config.cpp)
qt5_wrap_ui(kwin_glide_config_SRCS glide_config.ui)
qt5_add_dbus_interface(kwin_glide_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_glide_config_SRCS glideconfig.kcfgc)

add_library(kwin_glide_config MODULE ${kwin_glide_config_SRCS})

target_link_libraries(kwin_glide_config
    Qt5::DBus
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_glide_config glide_config.desktop)

install(
    TARGETS
        kwin_glide_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)

