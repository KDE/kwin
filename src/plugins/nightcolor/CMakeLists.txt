add_library(KWinNightColorPlugin SHARED)
target_sources(KWinNightColorPlugin PRIVATE
    clockskewnotifier.cpp
    clockskewnotifierengine.cpp
    nightcolordbusinterface.cpp
    nightcolormanager.cpp
    main.cpp
    suncalc.cpp
)

ecm_qt_declare_logging_category(KWinNightColorPlugin
    HEADER nightcolorlogging.h
    IDENTIFIER KWIN_NIGHTCOLOR
    CATEGORY_NAME kwin_nightcolor
    DEFAULT_SEVERITY Critical
)

kconfig_add_kcfg_files(KWinNightColorPlugin nightcolorsettings.kcfgc)

set(nightcolor_xml_SOURCES)
qt_add_dbus_adaptor(nightcolor_xml_SOURCES org.kde.kwin.ColorCorrect.xml nightcolordbusinterface.h KWin::NightColorDBusInterface)
target_sources(KWinNightColorPlugin PRIVATE ${nightcolor_xml_SOURCES})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(KWinNightColorPlugin PRIVATE clockskewnotifierengine_linux.cpp)
endif()

target_link_libraries(KWinNightColorPlugin kwin)

install(FILES nightcolorsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES org.kde.kwin.ColorCorrect.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
install(TARGETS KWinNightColorPlugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/plugins/)
