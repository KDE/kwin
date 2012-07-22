add_subdirectory( qml )
add_subdirectory( tests )

# Install the KWin/WindowSwitcher service type
install( FILES kwinwindowswitcher.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )
