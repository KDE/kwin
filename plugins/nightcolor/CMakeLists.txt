set(nightcolor_SOURCES
    clockskewnotifier.cpp
    clockskewnotifierengine.cpp
    nightcolordbusinterface.cpp
    nightcolormanager.cpp
    main.cpp
    suncalc.cpp
)

ecm_qt_declare_logging_category(nightcolor_SOURCES
    HEADER nightcolorlogging.h
    IDENTIFIER KWIN_NIGHTCOLOR
    CATEGORY_NAME kwin_nightcolor
    DEFAULT_SEVERITY Critical
)

kconfig_add_kcfg_files(nightcolor_SOURCES nightcolorsettings.kcfgc)
qt5_add_dbus_adaptor(nightcolor_SOURCES org.kde.kwin.ColorCorrect.xml
    nightcolordbusinterface.h KWin::NightColorDBusInterface)

add_library(KWinNightColorPlugin SHARED ${nightcolor_SOURCES})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(KWinNightColorPlugin PRIVATE clockskewnotifierengine_linux.cpp)
endif()

target_link_libraries(KWinNightColorPlugin kwin)

install(FILES nightcolorsettings.kcfg DESTINATION ${KCFG_INSTALL_DIR} RENAME ${KWIN_NAME}_colorcorrect.kcfg)
install(FILES org.kde.kwin.ColorCorrect.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
install(TARGETS KWinNightColorPlugin DESTINATION ${PLUGIN_INSTALL_DIR}/kwin/plugins/)
