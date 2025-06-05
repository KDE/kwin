#######################################
# Effect

set(zoom_SOURCES
    main.cpp
    zoom.cpp
    zoom.qrc
)

if (HAVE_ACCESSIBILITY)
    set(zoom_SOURCES ${zoom_SOURCES}
        accessibilityintegration.cpp
    )
endif()

kconfig_add_kcfg_files(zoom_SOURCES
    zoomconfig.kcfgc
)

kwin_add_builtin_effect(zoom ${zoom_SOURCES})
target_link_libraries(zoom PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin-wayland/effects/zoom)

if (HAVE_ACCESSIBILITY)
    target_link_libraries(zoom PRIVATE QAccessibilityClient6)
endif()

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_zoom_config_SRCS zoom_config.cpp)
    ki18n_wrap_ui(kwin_zoom_config_SRCS zoom_config.ui)
    kconfig_add_kcfg_files(kwin_zoom_config_SRCS zoomconfig.kcfgc)

    kwin_add_effect_config(kwin_zoom_config ${kwin_zoom_config_SRCS})

    target_link_libraries(kwin_zoom_config
        KF6::KCMUtils
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::WindowSystem
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
