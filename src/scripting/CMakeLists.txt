if (KWIN_BUILD_KCMS)
    set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
    qt_add_dbus_interface(kcm_kwin4_genericscripted_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
    add_library(kcm_kwin4_genericscripted MODULE ${kcm_kwin4_genericscripted_SRCS})
    target_link_libraries(kcm_kwin4_genericscripted
        KF6::ConfigWidgets #KCModule
        KF6::I18n
        Qt::DBus
        Qt::UiTools
        kwineffects
    )
    install(TARGETS kcm_kwin4_genericscripted DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
endif()
