#######################################
# Effect
install( FILES
    desktopgrid.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
install( FILES
    main.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/effects/desktopgrid/
)

#######################################
# Config
set(kwin_desktopgrid_config_SRCS desktopgrid_config.cpp)
qt5_wrap_ui(kwin_desktopgrid_config_SRCS desktopgrid_config.ui)
qt5_add_dbus_interface(kwin_desktopgrid_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_desktopgrid_config_SRCS desktopgridconfig.kcfgc)

add_library(kwin_desktopgrid_config MODULE ${kwin_desktopgrid_config_SRCS})

target_link_libraries(kwin_desktopgrid_config
    kwineffects
    Qt5::Quick
    KF5::Completion
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::Service
    KF5::XmlGui
)

kservice_desktop_to_json(kwin_desktopgrid_config desktopgrid_config.desktop)

install(
    TARGETS
        kwin_desktopgrid_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
