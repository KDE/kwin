#######################################
# Config

set(kwin_cubeslide_config_SRCS cubeslide_config.cpp)
ki18n_wrap_ui(kwin_cubeslide_config_SRCS cubeslide_config.ui)
kconfig_add_kcfg_files(kwin_cubeslide_config_SRCS cubeslideconfig.kcfgc)

add_library(kwin_cubeslide_config MODULE ${kwin_cubeslide_config_SRCS})

target_link_libraries(kwin_cubeslide_config
    KF5::ConfigWidgets
    KF5::I18n
    kwineffects
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_cubeslide_config cubeslide_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_cubeslide_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
