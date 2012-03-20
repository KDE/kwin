install( FILES tabbox.qml DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( FILES desktop.qml DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )

# packages
install( DIRECTORY clients/big_icons    DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/compact      DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/informative  DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/present_windows  DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/small_icons  DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/text         DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/thumbnails   DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )
install( DIRECTORY clients/window_strip DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )

# service files
install( FILES clients/big_icons/metadata.desktop    DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_big_icons.desktop )
install( FILES clients/compact/metadata.desktop      DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_compact.desktop )
install( FILES clients/informative/metadata.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_informative.desktop )
install( FILES clients/present_windows/metadata.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_present_windows.desktop )
install( FILES clients/small_icons/metadata.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_small_icons.desktop )
install( FILES clients/text/metadata.desktop         DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_text.desktop )
install( FILES clients/thumbnails/metadata.desktop   DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_thumbnails.desktop )
install( FILES clients/window_strip/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kwin RENAME kwin4_window_switcher_window_strip.desktop )

# install additional icon tabbox into those that need it
install (FILES IconTabBox.qml DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox/big_icons/contents/ui)
install (FILES IconTabBox.qml DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox/small_icons/contents/ui)
