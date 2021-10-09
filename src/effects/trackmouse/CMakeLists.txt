#######################################
# Effect
# Data files
install(FILES data/tm_inner.png data/tm_outer.png DESTINATION ${KDE_INSTALL_DATADIR}/kwin)

set(trackmouse_SOURCES
    trackmouse.cpp
)

kconfig_add_kcfg_files(trackmouse_SOURCES
    trackmouseconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_trackmouse ${trackmouse_SOURCES})

#######################################
# Config
set(kwin_trackmouse_config_SRCS trackmouse_config.cpp)
ki18n_wrap_ui(kwin_trackmouse_config_SRCS trackmouse_config.ui)
kconfig_add_kcfg_files(kwin_trackmouse_config_SRCS trackmouseconfig.kcfgc)

add_library(kwin_trackmouse_config MODULE ${kwin_trackmouse_config_SRCS})

target_link_libraries(kwin_trackmouse_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_trackmouse_config trackmouse_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_trackmouse_config
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
