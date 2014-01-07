set(plastik_plugin_SRCS
    plastikbutton.cpp
    plastikplugin.cpp
    )

add_library(plastikplugin SHARED ${plastik_plugin_SRCS})
target_link_libraries(plastikplugin Qt5::Core Qt5::Quick KF5::ConfigWidgets kdecorations)
install(TARGETS plastikplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/decorations/plastik)
install(FILES qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/decorations/plastik)
