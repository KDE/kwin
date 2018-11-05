install(DIRECTORY package/
        DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/effects/kwin4_effect_fade)

install(FILES package/metadata.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME}
        RENAME kwin4_effect_fade.desktop)

file(COPY package/ DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/effects/kwin4_effect_fade)
