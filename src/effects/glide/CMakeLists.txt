#######################################
# Effect

set(glide_SOURCES
    glide.cpp
)

kconfig_add_kcfg_files(glide_SOURCES
    glideconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_glide ${glide_SOURCES})

#######################################
# Config
set(kwin_glide_config_SRCS glide_config.cpp)
ki18n_wrap_ui(kwin_glide_config_SRCS glide_config.ui)
kconfig_add_kcfg_files(kwin_glide_config_SRCS glideconfig.kcfgc)

add_library(kwin_glide_config MODULE ${kwin_glide_config_SRCS})

target_link_libraries(kwin_glide_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_glide_config glide_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_glide_config
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)

