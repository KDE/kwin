#######################################
# Effect

#######################################
# Config
set(kwin_lookingglass_config_SRCS lookingglass_config.cpp)
ki18n_wrap_ui(kwin_lookingglass_config_SRCS lookingglass_config.ui)
qt5_add_dbus_interface(kwin_lookingglass_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_lookingglass_config_SRCS lookingglassconfig.kcfgc)

add_library(kwin_lookingglass_config MODULE ${kwin_lookingglass_config_SRCS})

target_link_libraries(kwin_lookingglass_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
)

kcoreaddons_desktop_to_json(kwin_lookingglass_config lookingglass_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_lookingglass_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
