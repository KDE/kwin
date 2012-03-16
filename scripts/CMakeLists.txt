# packages
install( DIRECTORY videowall DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/scripts )
install( DIRECTORY synchronizeskipswitcher DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/scripts )
install( DIRECTORY desktopchangeosd DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/scripts )

# service files
install( FILES videowall/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR} RENAME kwin-script-videowall.desktop )
install( FILES synchronizeskipswitcher/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR} RENAME kwin-script-synchronizeskipswitcher.desktop )
install( FILES desktopchangeosd/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR} RENAME kwin-script-desktopchangeosd.desktop )
