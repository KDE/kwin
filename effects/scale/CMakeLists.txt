#######################################
# Config
set(kwin_scale_config_SRCS scale_config.cpp)
ki18n_wrap_ui(kwin_scale_config_SRCS scale_config.ui)
qt5_add_dbus_interface(kwin_scale_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_scale_config_SRCS scaleconfig.kcfgc)

add_library(kwin_scale_config MODULE ${kwin_scale_config_SRCS})

target_link_libraries(kwin_scale_config
    Qt5::DBus
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kcoreaddons_desktop_to_json(kwin_scale_config scale_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_scale_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)

