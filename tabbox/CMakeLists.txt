add_subdirectory( qml )

# Install the KWin/WindowSwitcher service type
install( FILES kwinwindowswitcher.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )
