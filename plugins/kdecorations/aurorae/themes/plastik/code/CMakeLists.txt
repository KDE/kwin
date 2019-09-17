set(plastik_plugin_SRCS
    plastikbutton.cpp
    plastikplugin.cpp
)

add_library(plastikplugin SHARED ${plastik_plugin_SRCS})
set_target_properties(plastikplugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org/kde/kwin/decorations/plastik")
target_link_libraries(plastikplugin Qt5::Core Qt5::Quick KF5::ConfigWidgets)
install(TARGETS plastikplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/decorations/plastik)
install(FILES qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/decorations/plastik)
file(COPY qmldir DESTINATION ${CMAKE_BINARY_DIR}/bin/org/kde/kwin/decorations/plastik)
