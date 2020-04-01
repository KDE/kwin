#######################################
# Config
set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
ki18n_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

add_library(kwin_magiclamp_config MODULE ${kwin_magiclamp_config_SRCS})

target_link_libraries(kwin_magiclamp_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_magiclamp_config magiclamp_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_magiclamp_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
