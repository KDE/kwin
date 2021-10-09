#######################################
# Effect

set(magiclamp_SOURCES
    magiclamp.cpp
)

kconfig_add_kcfg_files(magiclamp_SOURCES
    magiclampconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_magiclamp ${magiclamp_SOURCES})

#######################################
# Config
set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
ki18n_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

add_library(kwin_magiclamp_config MODULE ${kwin_magiclamp_config_SRCS})

target_link_libraries(kwin_magiclamp_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_magiclamp_config magiclamp_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_magiclamp_config
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
