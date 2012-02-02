install(DIRECTORY package/
        DESTINATION ${DATA_INSTALL_DIR}/kwin/effects/kwin4_effect_fadedesktop)

install(FILES package/metadata.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin
        RENAME kwin4_effect_fadedesktop.desktop)
