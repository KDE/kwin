set(plastik_plugin_SRCS
    plastikbutton.cpp
    plastikplugin.cpp
    )

qt4_automoc(${plastik_plugin_SRCS})

add_library(plastikplugin SHARED ${plastik_plugin_SRCS})
target_link_libraries(plastikplugin Qt5::Core Qt5::Declarative KF5::KConfigWidgets kdecorations)
install(TARGETS plastikplugin DESTINATION ${IMPORTS_INSTALL_DIR}/org/kde/kwin/decorations/plastik)
install(FILES qmldir DESTINATION ${IMPORTS_INSTALL_DIR}/org/kde/kwin/decorations/plastik)
