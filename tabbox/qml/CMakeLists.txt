# packages
install( DIRECTORY desktops/informative DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/desktoptabbox )
install( DIRECTORY desktops/previews DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/desktoptabbox )

# service files
install( FILES desktops/informative/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_desktop_switcher_informative.desktop )
install( FILES desktops/previews/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_desktop_switcher_previews.desktop )
