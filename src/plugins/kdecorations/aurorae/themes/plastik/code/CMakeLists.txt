set(plastik_plugin_SRCS
    plastikbutton.cpp
    plastikplugin.cpp
)

add_library(plastikplugin SHARED ${plastik_plugin_SRCS})
set_target_properties(plastikplugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org/kde/kwin/decorations/plastik")
target_link_libraries(plastikplugin Qt::Core Qt::Quick KF5::ConfigWidgets)
install(TARGETS plastikplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/decorations/plastik)
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/decorations/plastik)
file(COPY qmldir DESTINATION ${CMAKE_BINARY_DIR}/bin/org/kde/kwin/decorations/plastik)
