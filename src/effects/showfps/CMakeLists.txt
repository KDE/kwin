#######################################
# Effect

set(showfps_SOURCES
    showfps.cpp
)

kconfig_add_kcfg_files(showfps_SOURCES
    showfpsconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_showfps ${showfps_SOURCES})

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
    Qt::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_showfps_config showfps_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_showfps_config
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
