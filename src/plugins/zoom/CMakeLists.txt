#######################################
# Effect

set(zoom_SOURCES
    main.cpp
    zoom.cpp
    shortcutshintmodel.cpp
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
