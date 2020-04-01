#######################################
# Effect
install(FILES main.qml DESTINATION ${DATA_INSTALL_DIR}/kwin/effects/desktopgrid/)

#######################################
# Config
set(kwin_desktopgrid_config_SRCS desktopgrid_config.cpp)
ki18n_wrap_ui(kwin_desktopgrid_config_SRCS desktopgrid_config.ui)
kconfig_add_kcfg_files(kwin_desktopgrid_config_SRCS desktopgridconfig.kcfgc)

add_library(kwin_desktopgrid_config MODULE ${kwin_desktopgrid_config_SRCS})

target_link_libraries(kwin_desktopgrid_config
    KF5::Completion
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    Qt5::Quick
    kwineffects
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_desktopgrid_config desktopgrid_config.desktop SERVICE_TYPES kcmodule.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_desktopgrid_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
