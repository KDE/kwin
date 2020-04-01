#######################################
# Config
set(kwin_showfps_config_SRCS showfps_config.cpp)
ki18n_wrap_ui(kwin_showfps_config_SRCS showfps_config.ui)
kconfig_add_kcfg_files(kwin_showfps_config_SRCS showfpsconfig.kcfgc)

add_library(kwin_showfps_config MODULE ${kwin_showfps_config_SRCS})

target_link_libraries(kwin_showfps_config
    KF5::Completion
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_showfps_config showfps_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_showfps_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
