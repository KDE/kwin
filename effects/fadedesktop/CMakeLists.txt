install(DIRECTORY package/
        DESTINATION ${KDE_INSTALL_DATADIR}/${KWIN_NAME}/effects/kwin4_effect_fadedesktop)

install(FILES package/metadata.desktop
        DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/${KWIN_NAME}
        RENAME kwin4_effect_fadedesktop.desktop)

file(COPY package/ DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/effects/kwin4_effect_fadedesktop)
