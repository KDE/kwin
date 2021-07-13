set(expoplugin_SOURCES
    expolayout.cpp
    expoplugin.cpp
)

add_library(kwin_script_expoplugin ${expoplugin_SOURCES})

target_link_libraries(kwin_script_expoplugin
    Qt5::Core
    Qt5::Gui
    Qt5::Qml
    Qt5::Quick
)

install(TARGETS kwin_script_expoplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/scripts/expo)
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/scripts/expo)
