kcoreaddons_add_plugin(nightlight INSTALL_NAMESPACE "kwin/plugins")
target_sources(nightlight PRIVATE
    clockskewnotifier.cpp
    clockskewnotifierengine.cpp
    nightlightdbusinterface.cpp
    nightlightmanager.cpp
    main.cpp
    suncalc.cpp
)

ecm_qt_declare_logging_category(nightlight
    HEADER nightlightlogging.h
    IDENTIFIER KWIN_nightlight
    CATEGORY_NAME kwin_nightlight
    DEFAULT_SEVERITY Critical
)

kconfig_add_kcfg_files(nightlight nightlightsettings.kcfgc)

set(nightlight_xml_SOURCES)
qt_add_dbus_adaptor(nightlight_xml_SOURCES org.kde.kwin.ColorCorrect.xml nightlightdbusinterface.h KWin::nightlightDBusInterface)
target_sources(nightlight PRIVATE ${nightlight_xml_SOURCES})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(nightlight PRIVATE clockskewnotifierengine_linux.cpp)
endif()

target_link_libraries(nightlight kwin KF6::ConfigGui KF6::GlobalAccel KF6::I18n)

install(FILES nightlightsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES org.kde.kwin.ColorCorrect.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
