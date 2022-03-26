#######################################
# Effect

set(zoom_SOURCES
    main.cpp
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
target_link_libraries(kwin4_effect_zoom PRIVATE
    kwineffects
    kwinglutils

    KF5::ConfigGui
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
)

if (HAVE_ACCESSIBILITY)
    target_include_directories(kwin4_effect_zoom PRIVATE ${QACCESSIBILITYCLIENT_INCLUDE_DIR})
    target_link_libraries(kwin4_effect_zoom PRIVATE ${QACCESSIBILITYCLIENT_LIBRARY})
endif()

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_zoom_config_SRCS zoom_config.cpp)
    ki18n_wrap_ui(kwin_zoom_config_SRCS zoom_config.ui)
    kconfig_add_kcfg_files(kwin_zoom_config_SRCS zoomconfig.kcfgc)

    kwin_add_effect_config(kwin_zoom_config ${kwin_zoom_config_SRCS})

    target_link_libraries(kwin_zoom_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        KWinEffectsInterface
    )
endif()
