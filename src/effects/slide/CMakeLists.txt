#######################################
# Effect

set(slide_SOURCES
    slide.cpp
)

kconfig_add_kcfg_files(slide_SOURCES
    slideconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_slide ${slide_SOURCES})

#######################################
# Config
set(kwin_slide_config_SRCS slide_config.cpp)
ki18n_wrap_ui(kwin_slide_config_SRCS slide_config.ui)
kconfig_add_kcfg_files(kwin_slide_config_SRCS slideconfig.kcfgc)

add_library(kwin_slide_config MODULE ${kwin_slide_config_SRCS})

target_link_libraries(kwin_slide_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_slide_config slide_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_slide_config
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
