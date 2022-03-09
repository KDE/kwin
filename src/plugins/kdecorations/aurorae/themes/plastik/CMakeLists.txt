add_subdirectory(code)

install(DIRECTORY package/
        DESTINATION ${KDE_INSTALL_DATADIR}/${KWIN_NAME}/decorations/kwin4_decoration_qml_plastik)

install(FILES package/metadata.desktop
        DESTINATION ${KDE_INSTALL_KSERVICESDIR}/${KWIN_NAME}
        RENAME kwin4_decoration_qml_plastik.desktop)

file(COPY package/ DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/decorations/kwin4_decoration_qml_plastik)
