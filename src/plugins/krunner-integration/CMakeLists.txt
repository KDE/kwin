set(krunnerintegration_SOURCES
    main.cpp
    windowsrunnerinterface.cpp
)

qt_add_dbus_adaptor(krunnerintegration_SOURCES org.kde.krunner1.xml windowsrunnerinterface.h KWin::WindowsRunner)

kcoreaddons_add_plugin(krunnerintegration SOURCES ${krunnerintegration_SOURCES} INSTALL_NAMESPACE "kwin/plugins")
target_link_libraries(krunnerintegration kwin KF6::Runner)

install(FILES kwin-runner-windows.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)
