# packages
install( DIRECTORY videowall DESTINATION ${DATA_INSTALL_DIR}/kwin/scripts )
install( DIRECTORY synchronizeskipswitcher DESTINATION ${DATA_INSTALL_DIR}/kwin/scripts )

# service files
install( FILES videowall/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR} RENAME kwin-script-videowall.desktop )
install( FILES synchronizeskipswitcher/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR} RENAME kwin-script-synchronizeskipswitcher.desktop )
