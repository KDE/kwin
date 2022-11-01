add_subdirectory(code)

install(DIRECTORY package/
        DESTINATION ${KDE_INSTALL_DATADIR}/kwin/decorations/kwin4_decoration_qml_plastik)

file(COPY package/ DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/decorations/kwin4_decoration_qml_plastik)
