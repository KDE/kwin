install(DIRECTORY contents DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/effects/kwin4_effect_maximize)
install(FILES metadata.desktop DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/effects/kwin4_effect_maximize)

install(FILES metadata.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME}
        RENAME kwin4_effect_maximize.desktop)
