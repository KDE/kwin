#######################################
# Config
set(kwin_windowgeometry_config_SRCS windowgeometry_config.cpp)
ki18n_wrap_ui(kwin_windowgeometry_config_SRCS windowgeometry_config.ui)
qt5_add_dbus_interface(kwin_windowgeometry_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_windowgeometry_config_SRCS windowgeometryconfig.kcfgc)

add_library(kwin_windowgeometry_config MODULE ${kwin_windowgeometry_config_SRCS})

target_link_libraries(kwin_windowgeometry_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
)

kcoreaddons_desktop_to_json(kwin_windowgeometry_config windowgeometry_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_windowgeometry_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
