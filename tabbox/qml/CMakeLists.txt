# packages
install( DIRECTORY clients/big_icons    DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/compact      DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/informative  DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/present_windows  DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/sidebar      DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/small_icons  DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/text         DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )
install( DIRECTORY clients/thumbnails   DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox )

install( DIRECTORY desktops/informative DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/desktoptabbox )
install( DIRECTORY desktops/previews DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/desktoptabbox )

# service files
install( FILES clients/big_icons/metadata.desktop    DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_big_icons.desktop )
install( FILES clients/compact/metadata.desktop      DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_compact.desktop )
install( FILES clients/informative/metadata.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_informative.desktop )
install( FILES clients/present_windows/metadata.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_present_windows.desktop )
install( FILES clients/sidebar/metadata.desktop      DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_sidebar.desktop )
install( FILES clients/small_icons/metadata.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_small_icons.desktop )
install( FILES clients/text/metadata.desktop         DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_text.desktop )
install( FILES clients/thumbnails/metadata.desktop   DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_window_switcher_thumbnails.desktop )

install( FILES desktops/informative/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_desktop_switcher_informative.desktop )
install( FILES desktops/previews/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR}/${KWIN_NAME} RENAME kwin4_desktop_switcher_previews.desktop )

install (FILES IconTabBox.qml DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox/big_icons/contents/ui)
install (FILES IconTabBox.qml DESTINATION ${DATA_INSTALL_DIR}/${KWIN_NAME}/tabbox/small_icons/contents/ui)
