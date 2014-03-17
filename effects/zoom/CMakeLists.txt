#######################################
# Effect
install( FILES
    zoom.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_zoom_config_SRCS zoom_config.cpp)
qt5_wrap_ui(kwin_zoom_config_SRCS zoom_config.ui)
kconfig_add_kcfg_files(kwin_zoom_config_SRCS zoomconfig.kcfgc)

add_library(kwin_zoom_config MODULE ${kwin_zoom_config_SRCS})

target_link_libraries(kwin_zoom_config
    kwineffects
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::Service
    KF5::XmlGui
)

kservice_desktop_to_json(kwin_zoom_config zoom_config.desktop)

install(
    TARGETS
        kwin_zoom_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
