#######################################
# Effect

set(zoom_SOURCES
    zoom.cpp
)

if (HAVE_ACCESSIBILITY)
    set(zoom_SOURCES ${zoom_SOURCES}
        accessibilityintegration.cpp
    )
endif()

kconfig_add_kcfg_files(zoom_SOURCES
    zoomconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_zoom ${zoom_SOURCES})

#######################################
# Config
set(kwin_zoom_config_SRCS zoom_config.cpp)
ki18n_wrap_ui(kwin_zoom_config_SRCS zoom_config.ui)
kconfig_add_kcfg_files(kwin_zoom_config_SRCS zoomconfig.kcfgc)

add_library(kwin_zoom_config MODULE ${kwin_zoom_config_SRCS})

target_link_libraries(kwin_zoom_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_zoom_config zoom_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_zoom_config
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
