#######################################
# Effect

# Data files
install(FILES data/cubecap.png DESTINATION ${DATA_INSTALL_DIR}/kwin)

#######################################
# Config

set(kwin_cube_config_SRCS cube_config.cpp)
ki18n_wrap_ui(kwin_cube_config_SRCS cube_config.ui)
kconfig_add_kcfg_files(kwin_cube_config_SRCS cubeconfig.kcfgc)

add_library(kwin_cube_config MODULE ${kwin_cube_config_SRCS})

target_link_libraries(kwin_cube_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KF5::KIOWidgets
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_cube_config cube_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_cube_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
