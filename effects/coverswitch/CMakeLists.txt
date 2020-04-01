#######################################
# Effect

#######################################
# Config
set(kwin_coverswitch_config_SRCS coverswitch_config.cpp)
ki18n_wrap_ui(kwin_coverswitch_config_SRCS coverswitch_config.ui)
kconfig_add_kcfg_files(kwin_coverswitch_config_SRCS coverswitchconfig.kcfgc)

add_library(kwin_coverswitch_config MODULE ${kwin_coverswitch_config_SRCS})

target_link_libraries(kwin_coverswitch_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_coverswitch_config coverswitch_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_coverswitch_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
