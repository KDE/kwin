add_subdirectory(v2)

if (KWIN_BUILD_KCMS)
    set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
    qt_add_dbus_interface(kcm_kwin4_genericscripted_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
    add_library(kcm_kwin4_genericscripted MODULE ${kcm_kwin4_genericscripted_SRCS})
    target_link_libraries(kcm_kwin4_genericscripted
        KF5::ConfigWidgets #KCModule
        KF5::I18n
        KF5::Service
        Qt::DBus
        Qt::UiTools
        kwineffects
    )
    install(TARGETS kcm_kwin4_genericscripted DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
endif()
