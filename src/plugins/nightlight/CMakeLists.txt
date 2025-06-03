kcoreaddons_add_plugin(nightlight INSTALL_NAMESPACE "kwin/plugins")
target_sources(nightlight PRIVATE
    nightlightdbusinterface.cpp
    nightlightmanager.cpp
    main.cpp
)

ecm_qt_declare_logging_category(nightlight
    HEADER nightlightlogging.h
    IDENTIFIER KWIN_NIGHTLIGHT
    CATEGORY_NAME kwin_nightlight
    DEFAULT_SEVERITY Critical
)

kconfig_add_kcfg_files(nightlight nightlightsettings.kcfgc)

kconfig_target_kcfg_file(nightlight
    FILE nightlightstate.kcfg
    CLASS_NAME NightLightState
    MUTATORS
)

set(nightlight_xml_SOURCES)
qt_add_dbus_adaptor(nightlight_xml_SOURCES org.kde.KWin.NightLight.xml nightlightdbusinterface.h KWin::NightLightDBusInterface)
target_sources(nightlight PRIVATE ${nightlight_xml_SOURCES})

target_link_libraries(nightlight
    kwin
    KNightTime
    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
)

install(FILES nightlightsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES org.kde.KWin.NightLight.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
